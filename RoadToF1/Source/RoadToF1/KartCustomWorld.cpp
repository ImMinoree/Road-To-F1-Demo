#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/CollisionProfile.h"
#include "HAL/IConsoleManager.h"
#include "KartMeshSceneMotion.h"

bool UKartPhysicsWorldSubsystem::IsTickable() const
{return !IsTemplate() && !FleetActors.IsEmpty() && !bFleetFault;}
TStatId UKartPhysicsWorldSubsystem::GetStatId() const
{RETURN_QUICK_DECLARE_CYCLE_STAT(UKartPhysicsWorldSubsystem,STATGROUP_Tickables);}
void UKartPhysicsWorldSubsystem::Tick(float Dt)
{Super::Tick(Dt);if(!FleetActors.IsEmpty())AdvanceCustomWorld(Dt);}
bool UKartPhysicsWorldSubsystem::IsFleetKart(const AKartPawn& Kart) const
{for(const auto& E:FleetActors)if(E.Pawn.Get()==&Kart)return true;return false;}
bool UKartPhysicsWorldSubsystem::GetStaticChassisQuery(const AKartPawn& Kart,FKartConvexShape& Shape) const
{
    for(const auto& R:FleetActors)if(R.Pawn.Get()==&Kart)
        if(const auto* B=Fleet.Bodies.FindByPredicate([&](const FKartFleetBody& E){return E.Id==R.Id;}))
        {
            Shape.Orientation=Kart.GetActorQuat();Shape.Position=Kart.GetActorLocation()+Shape.Orientation.RotateVector(B->Vehicle.Settings.StaticShapeOffset());
            Shape.HalfExtents=B->Vehicle.Settings.StaticShapeHalfExtent();return true;
        }
    return false;
}

bool UKartPhysicsWorldSubsystem::CanTranslateStaticChassis(const AKartPawn& Kart,const FVector& Delta) const
{
    FKartConvexShape Start;if(!FleetScene.IsCurrent() || Delta.ContainsNaN() || !GetStaticChassisQuery(Kart,Start))return false;
    auto End=Start;End.Position+=Delta;FKartPhysicsAABB A,B;Start.Bounds(A);End.Bounds(B);
    TArray<int32> Candidates;FleetScene.Query(FKartPhysicsAABB::Combine(A,B),Candidates);
    for(int Index:Candidates)
    {
        const auto& T=FleetScene.GetTriangles()[Index];
        const FVector Plane=FVector::CrossProduct(T.V[1]-T.V[0],T.V[2]-T.V[0]).GetSafeNormal();
        double Radius=0;for(int I=0;I<3;++I){FVector Axis=FVector::ZeroVector;Axis[I]=1;Radius+=Start.HalfExtents[I]*FMath::Abs(FVector::DotProduct(Start.Orientation.RotateVector(Axis),Plane));}
        const double Distance=FVector::DotProduct(Start.Position-T.V[0],Plane);
        // Exact plane separation remains valid over this entire translation.
        // Adjacent finite triangles on the same touching wall must not turn
        // tangent travel across their seam into a false new obstruction.
        if(FMath::Abs(Distance)>=Radius-1.e-7 && Distance*FVector::DotProduct(Delta,Plane)>=-1.e-7)continue;
        FVector N;const double Gap=FleetScene.Separation(Start,T,N);
        if(Gap<=.02)
        {
            // A currently touching floor/kerb cannot forbid a tangent or
            // separating retreat. Moving into that same separating plane is
            // still rejected; every NEW obstacle is independently swept below.
            if(FVector::DotProduct(Delta,N)>1.e-7)return false;
            continue;
        }
        double Time;bool Hit;FString Failure;
        if(!KartMeshMotion::CastTriangle(Start,End,T,Time,Hit,&Failure) || Hit)return false;
    }
    return true;
}
bool UKartPhysicsWorldSubsystem::SelectFleet(const TArray<AKartPawn*>& Karts,bool bExplicitDevelopmentOptIn)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    FleetFailure.Empty();auto Fail=[&](const TCHAR* Why){FleetFailure=Why;return false;};
    if(!GIsAutomationTesting && (!bExplicitDevelopmentOptIn || !IsExperimentalRaceReady()))return Fail(TEXT("Experimental race admission not validated"));
    if(!FleetActors.IsEmpty() || TestKart.IsValid() || Karts.IsEmpty() || Karts.Num()>20)return Fail(TEXT("Fleet already active or invalid size"));
    for(auto* K:Karts)if(!IsValid(K))return Fail(TEXT("Invalid fleet actor"));
    TArray<AKartPawn*> Sorted=Karts;Sorted.Sort([](const AKartPawn& A,const AKartPawn& B){return A.GetKartNumber()<B.GetKartNumber();});
    TSet<AKartPawn*> Members;TSet<int32> Numbers;
    for(auto* K:Sorted)
    {
        if(!IsValid(K) || K->GetWorld()!=GetWorld() || K->PhysicsBackend!=EKartPhysicsBackend::Legacy || K->Body->IsSimulatingPhysics()
            || FMath::Abs(K->Speed)>.01 || FMath::Abs(K->VerticalSpeed)>.01 || Members.Contains(K) || Numbers.Contains(K->KartNumber))return Fail(TEXT("Fleet requires unique stationary Legacy karts without engine simulation"));
        Members.Add(K);Numbers.Add(K->KartNumber);
    }
    for(TActorIterator<AKartPawn> It(GetWorld());It;++It)if(!Members.Contains(*It))return Fail(TEXT("Mixed Legacy/Custom racing is forbidden"));
    FKartStaticMeshGeometry Scene;if(!Scene.Build(*GetWorld(),FleetFailure))return false;
    FKartCustomFleet Prepared;TArray<FLiveKart> Actors;
    for(auto* K:Sorted)
    {
        FKartFleetBody E;E.Id=ExperimentalIds.Allocate();E.Ignore=K;FKartVehicleSettings Settings;Settings.SpeedCapKmh=K->SpeedLimitKmh;Settings.CollisionHalfExtent=K->GetCollisionHalfExtent();
        // Authored fairing minimum9.7348cm minus art/root offset22cm =-12.2652cm.
        // Conservative chassis lower face-12.5cm, upper face20cm. Preserve the
        // full98x63x20 kart-pair/tire envelope; do not make tires a solid road plow.
        Settings.StaticBottomInsetCm=7.5;
        if(!E.Id.IsValid() || !E.Vehicle.Reset(Settings,K->GetActorLocation()) || !E.Vehicle.Body.SetPose(K->GetActorLocation(),K->GetActorQuat()))return Fail(TEXT("Fleet body configuration invalid"));
        if(!FKartCustomSceneBridge::HasGroundSupport(*GetWorld(),K,E.Vehicle))return Fail(TEXT("Fleet spawn lacks four valid ground samples"));
        FKartConvexShape Shape;Shape.Orientation=E.Vehicle.Body.GetOrientation();Shape.Position=E.Vehicle.Body.GetPosition()+Shape.Orientation.RotateVector(Settings.StaticShapeOffset());Shape.HalfExtents=Settings.StaticShapeHalfExtent();
        FKartPhysicsAABB Bounds;Shape.Bounds(Bounds);TArray<int32> Near;Scene.Query(Bounds,Near);
        for(int T:Near){const auto M=Scene.Contact({{1},{2}},Shape,Scene.GetTriangles()[T]);
            if(M.Status!=EKartManifoldStatus::Valid && M.Status!=EKartManifoldStatus::NoContact)return Fail(TEXT("Fleet spawn contact geometry invalid"));
            for(const auto& P:M.Points)if(P.Depth>2)return Fail(TEXT("Fleet spawn deeply intersects static geometry"));}
        FLiveKart Record;Record.Id=E.Id;Record.Pawn=K;Record.PreviousPose=K->GetActorTransform();Record.bFirstPerson=K->bFirstPerson;Actors.Add(Record);Prepared.Bodies.Add(E);
    }
    // Commit ownership only after every member and static source was validated.
    Fleet=MoveTemp(Prepared);FleetActors=MoveTemp(Actors);FleetScene=MoveTemp(Scene);FleetClock.Reset();bFleetFault=false;
    for(auto& E:FleetActors){auto* K=E.Pawn.Get();K->PhysicsBackend=EKartPhysicsBackend::Custom;K->CustomVelocity=FVector::ZeroVector;K->Body->SetCollisionResponseToAllChannels(ECR_Overlap);}
    return true;
#endif
}
bool UKartPhysicsWorldSubsystem::ReleaseFleet()
{
    if(FleetActors.IsEmpty())return false;
    for(const auto& E:Fleet.Bodies)if(E.Vehicle.Body.GetLinearVelocity().Size()>.01 || E.Vehicle.Body.GetAngularVelocity().Size()>.01)return false;
    for(const auto& E:FleetActors)if(auto* K=E.Pawn.Get())
    {K->PhysicsBackend=EKartPhysicsBackend::Legacy;K->CustomVelocity=FVector::ZeroVector;K->Speed=K->VerticalSpeed=0;K->Body->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);K->Body->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);}
    FleetActors.Reset();Fleet=FKartCustomFleet();FleetScene=FKartStaticMeshGeometry();FleetClock.Reset();bFleetFault=false;FleetFailure.Empty();return true;
}
void UKartPhysicsWorldSubsystem::ResetFleetKart(AKartPawn& K)
{
    for(auto& R:FleetActors)if(R.Pawn.Get()==&K)
    {
        auto* E=Fleet.Bodies.FindByPredicate([&](const FKartFleetBody& B){return B.Id==R.Id;});
        if(!E){FKartFleetBody New;New.Id=R.Id;New.Ignore=&K;E=&Fleet.Bodies[Fleet.Bodies.Add(New)];}
        FKartVehicleSettings Settings;Settings.SpeedCapKmh=K.SpeedLimitKmh;Settings.CollisionHalfExtent=K.GetCollisionHalfExtent();
        Settings.StaticBottomInsetCm=7.5;
        bFleetFault=!E->Vehicle.Reset(Settings,K.GetActorLocation()) || !E->Vehicle.Body.SetPose(K.GetActorLocation(),K.GetActorQuat());
        R.bRetired=false;R.View.Reset();R.PreviousPose=K.GetActorTransform();R.bFirstPerson=K.bFirstPerson;E->Input={};
        K.CustomVelocity=FVector::ZeroVector;K.Body->SetCollisionResponseToAllChannels(ECR_Overlap);
        Fleet.ResetCaches();FleetClock.Reset();FleetFailure.Empty();
        if(!FleetScene.IsCurrent() && !FleetScene.Build(*GetWorld(),FleetFailure))bFleetFault=true;
        return;
    }
}
bool UKartPhysicsWorldSubsystem::AdvanceCustomWorld(double FrameSeconds)
{
    if(FleetActors.IsEmpty() || bFleetFault)return false;
    for(int I=FleetActors.Num()-1;I>=0;--I)
    {
        auto& E=FleetActors[I];auto* K=E.Pawn.Get();
        if(!IsValid(K)){Fleet.Remove(E.Id);FleetActors.RemoveAt(I);continue;}
        if(K->IsHidden() && K->bRaceFinished && !E.bRetired){Fleet.Remove(E.Id);E.bRetired=true;}
    }
    if(Fleet.Bodies.IsEmpty())return true;
    for(TActorIterator<AKartPawn> It(GetWorld());It;++It)if(!IsFleetKart(**It))
    {FleetFailure=TEXT("Unregistered Legacy kart appeared during Custom simulation");bFleetFault=true;return false;}
    const bool Ok=FleetClock.Advance(FrameSeconds,[&](double Dt)
    {
        for(auto& R:FleetActors)if(!R.bRetired)
        {
            auto* K=R.Pawn.Get();auto* E=Fleet.Bodies.FindByPredicate([&](const FKartFleetBody& B){return B.Id==R.Id;});
            if(!K || !E || K->Body->IsSimulatingPhysics() || K->PhysicsBackend!=EKartPhysicsBackend::Custom){FleetFailure=TEXT("Custom transform ownership violated");return false;}
            R.PreviousPose=FTransform(E->Vehicle.Body.GetOrientation(),E->Vehicle.Body.GetPosition());
            const bool Stunned=K->StunSeconds>0;
            E->Input={K->bRaceFinished || Stunned?0.:K->GetThrottleInput(),K->bRaceFinished?0.:K->GetSteeringInput(),K->bRaceFinished?.25:Stunned || K->IsBrakeHeld()?1.:0.};
            if(K->bOpponentRacer)
            {
                // Preserve the director's requested bicycle curvature, adapting only
                // the normalized steering range to this backend's rack/speed model.
                const double Speed=FMath::Abs(K->Speed);
                const double Angle=FMath::DegreesToRadians(22.)*E->Input.Steer/(1+Speed/1400.);
                const double Physical=FMath::Atan(FMath::Tan(Angle)*E->Vehicle.Settings.WheelbaseCm/104.);
                E->Input.Steer=FMath::Clamp(Physical*(1+FMath::Square(Speed/1300.))/E->Vehicle.Settings.SteerLimitRadians,-1.,1.);
            }
            E->Ignore=K;
        }
        if(!Fleet.Step(*GetWorld(),FleetScene,Dt,FleetFailure))return false;
        for(auto& R:FleetActors)if(!R.bRetired)
        {
            auto* K=R.Pawn.Get();const auto* E=Fleet.Bodies.FindByPredicate([&](const FKartFleetBody& B){return B.Id==R.Id;});
            K->UpdateIncident(Dt);if(!E->Impulse.IsNearlyZero())K->ReceiveCollision(E->Impulse.Size()/E->Vehicle.Body.GetMass(),E->Impulse.GetSafeNormal());
            if(R.bFirstPerson!=K->bFirstPerson){R.View.Reset();R.bFirstPerson=K->bFirstPerson;}
            FKartPhysicsViewInput V;const auto Q=E->Vehicle.Body.GetOrientation();V.LocalAcceleration=Q.UnrotateVector(E->Vehicle.Acceleration);V.LocalAngularVelocity=Q.UnrotateVector(E->Vehicle.Body.GetAngularVelocity());
            V.LocalImpactImpulse=Q.UnrotateVector(E->Impulse);V.MassKg=E->Vehicle.Body.GetMass();V.SpeedCmPerSecond=E->Vehicle.Body.GetLinearVelocity().Size();V.EngineActivity=FMath::Abs(E->Input.Throttle);
            if(!R.View.Step(V,Dt)){FleetFailure=TEXT("Custom view telemetry invalid");return false;}
        }
        return true;
    });
    if(!Ok)
    {
        bFleetFault=true;for(const auto& R:FleetActors)if(auto* K=R.Pawn.Get()){K->Speed=0;K->CustomVelocity=FVector::ZeroVector;}
        UE_LOG(LogTemp,Error,TEXT("Custom fleet stopped: %s"),*FleetFailure);return false;
    }
    const double Alpha=FleetClock.Alpha();
    for(auto& R:FleetActors)if(!R.bRetired)
    {
        auto* K=R.Pawn.Get();const auto* E=Fleet.Bodies.FindByPredicate([&](const FKartFleetBody& B){return B.Id==R.Id;});if(!K || !E)continue;
        const auto Q=E->Vehicle.Body.GetOrientation();K->CustomVelocity=E->Vehicle.Body.GetLinearVelocity();K->Speed=Q.UnrotateVector(K->CustomVelocity).X;
        K->SmoothedSteering=E->Vehicle.Steering/E->Vehicle.Settings.SteerLimitRadians;
        K->SetActorLocationAndRotation(FMath::Lerp(R.PreviousPose.GetLocation(),E->Vehicle.Body.GetPosition(),Alpha),FQuat::Slerp(R.PreviousPose.GetRotation(),Q,Alpha),false);
        K->UpdateDrivingView(FrameSeconds,false);
        if(K->bFirstPerson && K->IsLocallyControlled())
        {K->EyeCamera->SetRelativeLocation(K->ViewSettings.EyePosition+R.View.Offset);K->EyeCamera->SetRelativeRotation(FRotator(K->ViewSettings.EyePitch,0,0)+R.View.Rotation);K->EyeCamera->SetFieldOfView(R.View.FOV);}
    }
    return true;
}
#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs CustomRaceCommand(TEXT("RoadToF1.Physics.CustomRace"),TEXT("Explicit experimental fleet: 1 opt-in while stationary; 0 release after reset/stopping. Legacy stays default."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if(!World || Args.Num()!=1 || (Args[0]!=TEXT("1") && Args[0]!=TEXT("0")))return;
        auto* Physics=World->GetSubsystem<UKartPhysicsWorldSubsystem>();if(!Physics)return;
        TArray<AKartPawn*> Karts;for(TActorIterator<AKartPawn> It(World);It;++It)Karts.Add(*It);
        const bool Ok=Args[0]==TEXT("1")?Physics->SelectFleet(Karts,true):Physics->ReleaseFleet();
        UE_LOG(LogTemp,Display,TEXT("CustomRace %s: %s"),Ok?TEXT("accepted"):TEXT("rejected"),*Physics->GetFleetFailure());
    }));
#endif
