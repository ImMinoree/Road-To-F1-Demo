#include "KartCustomSceneBridge.h"
#include "KartPhysicsSolver.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Components/BoxComponent.h"
#include "KartPhysicsManifoldBuilder.h"
#include "KartPhysicsIntegrator.h"
#include "KartMeshSceneMotion.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
void SampleGround(const UWorld& World,const AActor* Ignore,const FKartExperimentalVehicle& Vehicle,FKartWheelGround (&Ground)[4])
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KartCustomGround),false,Ignore);
    const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    for(int I=0;I<4;++I)
    {
        const auto Mount=Vehicle.WheelPosition(I);FHitResult Hit;
        Ground[I].bHit=false;auto GroundQuery=Query;
        for(int Attempt=0;Attempt<4;++Attempt)
        {
            if(!World.LineTraceSingleByObjectType(Hit,Mount+FVector(0,0,10),Mount-FVector(0,0,60),StaticObjects,GroundQuery))break;
            // An interior wall hit may report an upward normal at the ray origin.
            // It is not a wheel support surface; find real ground beneath it.
            if(!Hit.bStartPenetrating && Hit.Time>1.e-6 && Hit.Normal.Z>=.5 && Hit.ImpactPoint.Z<=Mount.Z)
            {Ground[I].bHit=true;break;}
            if(!Hit.GetComponent())break;GroundQuery.AddIgnoredComponent(Hit.GetComponent());
        }
        if(Ground[I].bHit){Ground[I].HeightCm=Hit.ImpactPoint.Z;Ground[I].Normal=Hit.Normal;
            if(Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("VisualKerb")))Ground[I].Surface=EKartContactSurface::Kerb;
            if(const auto* Mesh=Cast<UStaticMeshComponent>(Hit.GetComponent()))if(Mesh->GetStaticMesh())
            {const auto Name=Mesh->GetStaticMesh()->GetName();if(Name.Contains(TEXT("Grass")))Ground[I].Surface=EKartContactSurface::Grass;else if(Name.Contains(TEXT("Kerb")))Ground[I].Surface=EKartContactSurface::Kerb;}}
    }
}

// Exact OBB SAT for continuous-pose bracketing. Positive depth means overlap;
// a separated axis is authoritative even when GJK is ill-conditioned near touch.
double BoxDepth(const FKartConvexShape& A,const FKartConvexShape& B,FVector& Normal)
{
    FVector AX[3],BX[3];for(int I=0;I<3;++I){FVector E=FVector::ZeroVector;E[I]=1;AX[I]=A.Orientation.RotateVector(E);BX[I]=B.Orientation.RotateVector(E);}
    const auto Delta=B.Position-A.Position;double Depth=TNumericLimits<double>::Max();
    auto Axis=[&](FVector N){const double L=N.Size();if(L<1.e-12)return;N/=L;double Radius=0;
        for(int I=0;I<3;++I)Radius+=A.HalfExtents[I]*FMath::Abs(FVector::DotProduct(AX[I],N))+B.HalfExtents[I]*FMath::Abs(FVector::DotProduct(BX[I],N));
        const double D=Radius-FMath::Abs(FVector::DotProduct(Delta,N));if(D<Depth){Depth=D;Normal=FVector::DotProduct(Delta,N)>=0?N:-N;}};
    for(int I=0;I<3;++I){Axis(AX[I]);Axis(BX[I]);for(int J=0;J<3;++J)Axis(FVector::CrossProduct(AX[I],BX[J]));}return Depth;
}
}

bool FKartCustomSceneBridge::Step(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,
    const FKartVehicleInput& Input,double Dt,FVector& ImpactImpulse,FString* Failure,FKartSceneStepStats* Stats,const FKartContactMaterial& Material,const FKartStaticMeshGeometry* Scene)
{
    ImpactImpulse=FVector::ZeroVector;if(Stats)*Stats={};
    return Advance(World,Ignore,Vehicle,Input,Dt,ImpactImpulse,Failure,0,Stats,Material,true,0,Scene);
}
bool FKartCustomSceneBridge::Prepare(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,const FKartVehicleInput& Input,double Dt)
{
    auto V=Vehicle;FKartWheelGround Ground[4];SampleGround(World,Ignore,V,Ground);
    const auto P=V.Body.GetPosition();const auto Q=V.Body.GetOrientation();
    if(!V.Step(Input,Ground,Dt) || !V.Body.SetPose(P,Q))return false;Vehicle=V;return true;
}
bool FKartCustomSceneBridge::HasGroundSupport(const UWorld& World,const AActor* Ignore,const FKartExperimentalVehicle& Vehicle)
{
    FKartWheelGround Ground[4];SampleGround(World,Ignore,Vehicle,Ground);for(const auto& G:Ground)if(!G.bHit)return false;return true;
}
bool FKartCustomSceneBridge::Move(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,double Dt,
    FVector& Impulse,FString* Failure,FKartSceneStepStats* Stats,const FKartContactMaterial& Material,const FKartStaticMeshGeometry& Scene)
{
    Impulse=FVector::ZeroVector;if(Stats)*Stats={};
    return Advance(World,Ignore,Vehicle,{},Dt,Impulse,Failure,0,Stats,Material,false,0,&Scene);
}
bool FKartCustomSceneBridge::Advance(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,
    const FKartVehicleInput& Input,double Dt,FVector& ImpactImpulse,FString* Failure,int32 Depth,FKartSceneStepStats* Stats,const FKartContactMaterial& Material,bool bApplyForces,int32 MotionIterations,const FKartStaticMeshGeometry* Scene)
{
    if(Failure)Failure->Empty();auto Fail=[&](const TCHAR* Reason){if(Failure)*Failure=Reason;return false;};
    if(!Material.IsValid())return Fail(TEXT("Invalid bridge material"));
    auto V=Vehicle;const auto Start=V.Body.GetPosition();const auto Rotation=V.Body.GetOrientation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KartCustomGround),false,Ignore);
    const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    FKartWheelGround Ground[4];SampleGround(World,Ignore,V,Ground);
    if(bApplyForces)
    {if(!V.Step(Input,Ground,Dt))return Fail(TEXT("Vehicle numerical/support guard"));}
    else
    {
        // Forces/wheel torques were already integrated for this fixed step.
        V.Body.ClearAccumulators();if(!FKartPhysicsIntegrator::Step(V.Body,Dt))return Fail(TEXT("Remaining-motion integration failed"));
    }
    // This bridge is deliberately not declared rotational CCD. Restrict experimental angular sweep.
    if(V.Body.GetAngularVelocity().Size()*Dt>.04)
    {
        if(Depth>=3)return Fail(TEXT("Angular sweep exceeds eight-substep budget"));
        V=Vehicle;FVector First,Second;
        if(!Advance(World,Ignore,V,Input,Dt*.5,First,Failure,Depth+1,Stats,Material,bApplyForces,MotionIterations,Scene)
            || !Advance(World,Ignore,V,Input,Dt*.5,Second,Failure,Depth+1,Stats,Material,bApplyForces,MotionIterations,Scene))return false;
        V.Acceleration=(V.Body.GetLinearVelocity()-Vehicle.Body.GetLinearVelocity())/Dt;
        Vehicle=V;ImpactImpulse=First+Second;return true;
    }
    if(Scene)
    {
        FVector Impulse=FVector::ZeroVector;
        if(!KartMeshMotion::Advance(*Scene,V,Start,Rotation,Dt,Impulse,Failure,Stats,Material))return false;
        V.Acceleration=(V.Body.GetLinearVelocity()-Vehicle.Body.GetLinearVelocity())/Dt;
        Vehicle=V;ImpactImpulse=Impulse;return true;
    }
    FHitResult Hit;const FVector End=V.Body.GetPosition();
    const FVector Extent=V.Settings.CollisionHalfExtent;
    FVector Impulse=FVector::ZeroVector;
    const bool HasHit=World.SweepSingleByObjectType(Hit,Start,End,Rotation,StaticObjects,FCollisionShape::MakeBox(Extent),Query);
    // Near-zero casts can report a finite Time > 1. Such a hit is outside this
    // interval, not a valid TOI. Endpoint overlaps below remain authoritative.
    bool HasTOI=HasHit && !Hit.bStartPenetrating && FMath::IsFinite(Hit.Time) && Hit.Time>=0 && Hit.Time<=1;
    if(HasTOI)
    {
        const FVector ContactVelocity=V.Body.GetLinearVelocity()+FVector::CrossProduct(V.Body.GetAngularVelocity(),Hit.ImpactPoint-Start);
        // A touching cast must not repeatedly consume zero time while sliding away.
        HasTOI=FVector::DotProduct(ContactVelocity,Hit.Normal)<-1.e-6;
    }
    double ConsumedFraction=1;
    if(Stats && HasHit){Stats->InitialOverlaps+=Hit.bStartPenetrating?1:0;Stats->OutOfIntervalHits+=(Hit.Time<0 || Hit.Time>1)?1:0;}
    if(HasTOI)
    {
        if(!Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN() || Hit.Normal.ContainsNaN()
            || FMath::Abs(Hit.Normal.SizeSquared()-1)>1.e-6)return Fail(TEXT("Invalid TOI normal/point"));
        const double SafeTime=double(Hit.Time);
        ConsumedFraction=SafeTime;
        // Translation and rotation must consume the same fraction of the step.
        if(!V.Body.SetPose(FMath::Lerp(Start,End,SafeTime),FQuat::Slerp(Rotation,V.Body.GetOrientation(),SafeTime)))return Fail(TEXT("Invalid TOI pose"));
    }
    if(HasHit && !FMath::IsFinite(Hit.Time))return Fail(TEXT("Nonfinite cast interval"));
    TArray<FOverlapResult> Overlaps;
    World.OverlapMultiByObjectType(Overlaps,V.Body.GetPosition(),V.Body.GetOrientation(),StaticObjects,
        FCollisionShape::MakeBox(Extent+FVector(.02)),Query);
    TArray<UPrimitiveComponent*> Components;
    for(const auto& O:Overlaps)if(O.GetComponent())Components.AddUnique(O.GetComponent());
    if(HasHit && Hit.GetComponent())Components.AddUnique(Hit.GetComponent());
    Components.Sort([](const UPrimitiveComponent& A,const UPrimitiveComponent& B){return A.GetPathName()<B.GetPathName();});
    // A translation cast holds orientation fixed. Bracket any newly intersecting
    // endpoint using the actual interpolated box poses before accepting movement.
    // This is TOI advancement, not a post-impact depenetration teleport.
    const auto CandidatePosition=V.Body.GetPosition();const auto CandidateRotation=V.Body.GetOrientation();
    double Earliest=1;const UBoxComponent* TOIBox=nullptr;FVector TOINormal;
    for(const auto* C:Components)if(const auto* Box=Cast<UBoxComponent>(C))
    {
        FKartConvexShape A,B;A.HalfExtents=Extent;A.Position=Start;A.Orientation=Rotation;
        B.Position=Box->GetComponentLocation();B.Orientation=Box->GetComponentQuat();B.HalfExtents=Box->GetScaledBoxExtent();
        FVector N;if(BoxDepth(A,B,N)>0)continue; // Existing overlap uses split correction below.
        A.Position=CandidatePosition;A.Orientation=CandidateRotation;if(BoxDepth(A,B,N)<=0)continue;
        double Low=0,High=1;
        for(int I=0;I<32;++I){const double T=(Low+High)*.5;A.Position=FMath::Lerp(Start,CandidatePosition,T);A.Orientation=FQuat::Slerp(Rotation,CandidateRotation,T);
            if(BoxDepth(A,B,N)>0)High=T;else Low=T;}
        if(Low<Earliest){Earliest=Low;TOIBox=Box;TOINormal=N;}
    }
    if(TOIBox)
    {
        const double Travel=(CandidatePosition-Start).Size()+Extent.Size()*Rotation.AngularDistance(CandidateRotation);
        const double Safe=FMath::Max(0.,Earliest-.001/FMath::Max(Travel,1.e-9));
        ConsumedFraction*=Safe;
        if(!V.Body.SetPose(FMath::Lerp(Start,CandidatePosition,Safe),FQuat::Slerp(Rotation,CandidateRotation,Safe)))return Fail(TEXT("Invalid rotational TOI pose"));
        HasTOI=true;Hit.Component=const_cast<UBoxComponent*>(TOIBox);Hit.Normal=-TOINormal;
        FKartConvexShape S;S.Position=V.Body.GetPosition();S.Orientation=V.Body.GetOrientation();S.HalfExtents=Extent;
        if(!S.Support(TOINormal,Hit.ImpactPoint))return Fail(TEXT("Invalid rotational contact support"));
    }
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> Bodies;Bodies.Add({1},V.Body);
    TMap<FKartPhysicsBodyId,FKartContactMaterial> Materials;Materials.Add({1},Material);
    TArray<FKartContactManifold> Contacts;
    FKartConvexShape Chassis;Chassis.Position=V.Body.GetPosition();Chassis.Orientation=V.Body.GetOrientation();Chassis.HalfExtents=Extent;
    uint64 NextId=2;
    for(const auto* Component:Components)
    {
        // Exact box geometry only. Never replace a triangle mesh with its bounds.
        const auto* Box=Cast<UBoxComponent>(Component);
        if(!Box)return Fail(TEXT("Unsupported static contact geometry: convex/mesh adapter required"));
        FKartConvexShape Shape;Shape.Position=Box->GetComponentLocation();Shape.Orientation=Box->GetComponentQuat();Shape.HalfExtents=Box->GetScaledBoxExtent();
        const FKartPhysicsBodyId Id{NextId++};
        auto M=KartManifolds::Build({{1},Id},Chassis,Shape);
        if(M.Status!=EKartManifoldStatus::Valid && M.Status!=EKartManifoldStatus::NoContact)
        {if(Failure)*Failure=FString::Printf(TEXT("Static box manifold failed status%d GJK%d EPA%d pos%s quat%s"),int(M.Status),int(M.GJK),int(M.EPA),*Chassis.Position.ToString(),*Chassis.Orientation.ToString());return false;}
        if(M.Status==EKartManifoldStatus::NoContact && HasTOI && Component==Hit.GetComponent())
        {
            // Recover the full contact patch at the measured TOI instead of using
            // the cast's arbitrary single corner (which introduces artificial pitch).
            // A small geometric contact margin constructs the patch; the probe never moves the body.
            auto Probe=Chassis;const FVector Shift=-Hit.Normal*.051;Probe.Position+=Shift;
            auto Patch=KartManifolds::Build({{1},Id},Probe,Shape);
            if(Patch.Status==EKartManifoldStatus::Valid)
            {
                M=Patch;
                for(auto& P:M.Points)
                {
                    // Conservative zero-depth constraint across the small gap.
                    P.WorldA=P.WorldB=(P.WorldA-Shift+P.WorldB)*.5;P.Depth=0;
                    P.LocalA=KartManifolds::ToLocal(Chassis,P.WorldA);P.LocalB=KartManifolds::ToLocal(Shape,P.WorldB);
                }
            }
            else
            {
                M.Status=EKartManifoldStatus::Valid;M.Normal=-Hit.Normal;
                FKartContactPoint P;P.WorldA=P.WorldB=Hit.ImpactPoint;
                P.LocalA=KartManifolds::ToLocal(Chassis,P.WorldA);P.LocalB=KartManifolds::ToLocal(Shape,P.WorldB);M.Points.Add(P);
            }
        }
        if(M.Status==EKartManifoldStatus::Valid)
        {
            FKartRigidBodyState StaticBody;StaticBody.SetPose(Shape.Position,Shape.Orientation);StaticBody.SetImmovable(true);
            Bodies.Add(Id,StaticBody);Contacts.Add(M);
            Materials.Add(Id,Material);
            if(Stats)for(const auto& P:M.Points)Stats->MaxPenetrationCm=FMath::Max(Stats->MaxPenetrationCm,P.Depth);
        }
    }
    if(!Contacts.IsEmpty())
    {
        // Physical impulses are applied once. Penetration stabilization uses a
        // separate pseudo-velocity solve, so initial overlap cannot add kinetic energy.
        FKartSolverSettings Physical;Physical.BiasFraction=0;
        const auto R=FKartPhysicsSolver::Solve(Bodies,Contacts,Dt,Physical,&Materials);
        if(!R.bSuccess){if(Failure)*Failure=R.Failure;return false;}
        V.Body=Bodies[{1}];for(const auto& C:R.Contacts)Impulse-=C.Normal*C.NormalImpulse;
        if(Stats){Stats->Contacts+=R.Contacts.Num();Stats->Iterations+=R.Iterations;
            for(const auto& C:R.Contacts)Stats->MaxResidualClosingSpeed=FMath::Max(Stats->MaxResidualClosingSpeed,C.ResidualClosingSpeed);}
        bool NeedsCorrection=false;for(const auto& M:Contacts)for(const auto& P:M.Points)NeedsCorrection|=P.Depth>Physical.SlopCm;
        if(NeedsCorrection)
        {
            auto Correction=Bodies;for(auto& Entry:Correction)Entry.Value.SetVelocities(FVector::ZeroVector,FVector::ZeroVector);
            const auto Push=FKartPhysicsSolver::Solve(Correction,Contacts,Dt);
            if(!Push.bSuccess){if(Failure)*Failure=Push.Failure;return false;}
            auto Corrected=Correction[{1}];Corrected.ClearAccumulators();
            if(!FKartPhysicsIntegrator::Step(Corrected,Dt))return Fail(TEXT("Invalid split correction"));
            // Integrate only the bounded contact correction; retain physical velocity.
            if(!V.Body.SetPose(Corrected.GetPosition(),Corrected.GetOrientation()))return Fail(TEXT("Invalid corrected pose"));
            if(Stats)Stats->Iterations+=Push.Iterations;
        }
    }
    const double Remaining=Dt*(1-ConsumedFraction);
    const double RemainingTravel=Remaining*(V.Body.GetLinearVelocity().Size()+Extent.Size()*V.Body.GetAngularVelocity().Size());
    if(Remaining>1.e-7 && RemainingTravel>1.e-6)
    {
        if(MotionIterations>=16)return Fail(TEXT("Remaining-motion contact budget exceeded"));
        FVector MoreImpulse;
        if(!Advance(World,Ignore,V,Input,Remaining,MoreImpulse,Failure,Depth,Stats,Material,false,MotionIterations+1,Scene))return false;
        Impulse+=MoreImpulse;
    }
    V.Acceleration=(V.Body.GetLinearVelocity()-Vehicle.Body.GetLinearVelocity())/Dt;
    Vehicle=V;ImpactImpulse=Impulse;return true;
}
