#include "KartPhysicsWorldSubsystem.h"
#include "KartCustomSceneBridge.h"
#include "KartPawn.h"
#include "KartRaceDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/CollisionProfile.h"
#include "HAL/IConsoleManager.h"

bool AKartPawn::TrySelectPhysicsBackend(EKartPhysicsBackend Requested)
{
    if(Requested==PhysicsBackend)return true;
    auto* Physics=GetWorld()?GetWorld()->GetSubsystem<UKartPhysicsWorldSubsystem>():nullptr;
    return Physics && Physics->SelectTestBackend(*this,Requested);
}
bool UKartPhysicsWorldSubsystem::SelectTestBackend(AKartPawn& Kart,EKartPhysicsBackend Requested)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    if(!FleetActors.IsEmpty())return false; // Ownership changes are atomic for the entire fleet.
    if(Kart.GetWorld()!=GetWorld() || FMath::Abs(Kart.Speed)>.01 || FMath::Abs(Kart.VerticalSpeed)>.01 || Kart.Body->IsSimulatingPhysics())return false;
    if(Requested==EKartPhysicsBackend::Legacy)
    {
        if(TestKart.Get()!=&Kart || TestVehicle.Body.GetLinearVelocity().Size()>.01 || TestVehicle.Body.GetAngularVelocity().Size()>.01)return false;
        Kart.PhysicsBackend=Requested;Kart.CustomVelocity=FVector::ZeroVector;
        Kart.Body->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);Kart.Body->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
        TestKart.Reset();TestClock.Reset();TestView.Reset();return true;
    }
    if(!FKartCustomSceneBridge::IsGameplayReady() && !GIsAutomationTesting)return false;
    if(!Kart.ActorHasTag(TEXT("CustomPhysicsTest")) || GetWorld()->GetMapName().Contains(TEXT("SouthGarda")))return false;
    if(TestKart.IsValid() && TestKart.Get()!=&Kart)return false;
    for(TActorIterator<AKartPawn> It(GetWorld());It;++It)if(*It!=&Kart)return false;
    for(TActorIterator<AKartRaceDirector> It(GetWorld());It;++It)return false;
    FKartVehicleSettings C;C.SpeedCapKmh=Kart.SpeedLimitKmh;
    if(!TestVehicle.Reset(C,Kart.GetActorLocation()) || !TestVehicle.Body.SetPose(Kart.GetActorLocation(),Kart.GetActorQuat()))return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KartCustomReadiness),false,&Kart);
    const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    for(int I=0;I<4;++I){const FVector P=TestVehicle.WheelPosition(I);FHitResult Hit;
        if(!GetWorld()->LineTraceSingleByObjectType(Hit,P+FVector(0,0,10),P-FVector(0,0,60),StaticObjects,Query) || Hit.Normal.Z<.5)return false;}
    TestKart=&Kart;Kart.PhysicsBackend=Requested;Kart.Body->SetCollisionResponseToAllChannels(ECR_Overlap);
    TestClock.Reset();TestView.Reset();bTestFault=false;PreviousTestPose=Kart.GetActorTransform();return true;
#endif
}
void UKartPhysicsWorldSubsystem::ResetTestKart(AKartPawn& Kart)
{
    if(IsFleetKart(Kart)){ResetFleetKart(Kart);return;}
    if(TestKart.Get()!=&Kart)return;
    FKartVehicleSettings C;C.SpeedCapKmh=Kart.SpeedLimitKmh;
    bTestFault=!TestVehicle.Reset(C,Kart.GetActorLocation()) || !TestVehicle.Body.SetPose(Kart.GetActorLocation(),Kart.GetActorQuat());
    Kart.Body->SetCollisionResponseToAllChannels(ECR_Overlap);Kart.CustomVelocity=FVector::ZeroVector;
    TestClock.Reset();TestView.Reset();PreviousTestPose=Kart.GetActorTransform();
}
void UKartPhysicsWorldSubsystem::AdvanceTestKart(AKartPawn& Kart,double FrameSeconds)
{
    if(TestKart.Get()!=&Kart || bTestFault)return;
    if(Kart.Body->IsSimulatingPhysics()){bTestFault=true;Kart.Speed=0;Kart.CustomVelocity=FVector::ZeroVector;return;}
    for(TActorIterator<AKartPawn> It(GetWorld());It;++It)if(*It!=&Kart){bTestFault=true;return;}
    if(bTestFirstPerson!=Kart.bFirstPerson){TestView.Reset();bTestFirstPerson=Kart.bFirstPerson;}
    const bool Ok=TestClock.Advance(FrameSeconds,[&](double Dt)
    {
        PreviousTestPose=FTransform(TestVehicle.Body.GetOrientation(),TestVehicle.Body.GetPosition());
        FVector Impact;
        const FKartVehicleInput Input{Kart.bRaceFinished?0:Kart.GetThrottleInput(),Kart.bRaceFinished?0:Kart.GetSteeringInput(),Kart.IsBrakeHeld()?1.:0.};
        if(!FKartCustomSceneBridge::Step(*GetWorld(),&Kart,TestVehicle,Input,Dt,Impact))return false;
        Kart.UpdateIncident(Dt);
        if(!Impact.IsNearlyZero())Kart.ReceiveCollision(Impact.Size()/TestVehicle.Body.GetMass(),Impact.GetSafeNormal());
        FKartPhysicsViewInput View;const auto Q=TestVehicle.Body.GetOrientation();
        View.LocalAcceleration=Q.UnrotateVector(TestVehicle.Acceleration);View.LocalAngularVelocity=Q.UnrotateVector(TestVehicle.Body.GetAngularVelocity());
        View.LocalImpactImpulse=Q.UnrotateVector(Impact);View.MassKg=TestVehicle.Body.GetMass();View.SpeedCmPerSecond=TestVehicle.Body.GetLinearVelocity().Size();View.EngineActivity=FMath::Abs(Input.Throttle);
        return TestView.Step(View,Dt);
    });
    if(!Ok){bTestFault=true;Kart.Speed=0;Kart.CustomVelocity=FVector::ZeroVector;UE_LOG(LogTemp,Error,TEXT("Custom test physics stopped on invalid/unsupported contact. Reset before continuing; Legacy remains available after stationary reset."));return;}
    const auto Q=TestVehicle.Body.GetOrientation();Kart.CustomVelocity=TestVehicle.Body.GetLinearVelocity();Kart.Speed=Q.UnrotateVector(Kart.CustomVelocity).X;
    Kart.SmoothedSteering=TestVehicle.Steering/TestVehicle.Settings.SteerLimitRadians;
    const double Alpha=TestClock.Alpha();const auto P=FMath::Lerp(PreviousTestPose.GetLocation(),TestVehicle.Body.GetPosition(),Alpha);
    Kart.SetActorLocationAndRotation(P,FQuat::Slerp(PreviousTestPose.GetRotation(),Q,Alpha),false); // Presentation only; no swept response.
    Kart.UpdateDrivingView(FrameSeconds,false);
    if(Kart.bFirstPerson && Kart.IsLocallyControlled())
    {Kart.EyeCamera->SetRelativeLocation(Kart.ViewSettings.EyePosition+TestView.Offset);
     Kart.EyeCamera->SetRelativeRotation(FRotator(Kart.ViewSettings.EyePitch,0,0)+TestView.Rotation);Kart.EyeCamera->SetFieldOfView(TestView.FOV);}
}
#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs CustomTestCommand(TEXT("RoadToF1.Physics.CustomTest"),TEXT("Explicit tagged single-kart test only: 1 selects Custom, 0 selects Legacy while stopped."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {if(!World || Args.Num()!=1)return;for(TActorIterator<AKartPawn> It(World);It;++It)
        {const bool Ok=It->TrySelectPhysicsBackend(Args[0]==TEXT("1")?EKartPhysicsBackend::Custom:EKartPhysicsBackend::Legacy);
        UE_LOG(LogTemp,Display,TEXT("CustomTest selection: %s"),Ok?TEXT("accepted"):TEXT("rejected (readiness/stationary/tag gate)"));break;}}));
#endif
