#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartFixedStepClock.h"
#include "KartCustomSceneBridge.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "KartPhysicsManifoldBuilder.h"
namespace KartStage9
{
AActor* Box(UWorld* W,FVector P,FVector E)
{
    auto* A=W->SpawnActor<AActor>();auto* C=NewObject<UBoxComponent>(A);A->SetRootComponent(C);
    C->SetBoxExtent(E);C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);
    C->RegisterComponent();A->SetActorLocation(P);return A;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage9Test,"RoadToF1.Physics.Stage9SingleIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage9Test::RunTest(const FString&)
{
    TestFalse(TEXT("Runtime admission closed while contact regression fails"),FKartCustomSceneBridge::IsGameplayReady());
    for(int FPS:{30,60,120}){FKartFixedStepClock C;int Count=0;for(int I=0;I<FPS*2;++I)C.Advance(1./FPS,[&](double){++Count;return true;});
        TestEqual(TEXT("Fixed cadence"),Count,240);TestTrue(TEXT("Bounded interpolation"),C.Alpha()>=0 && C.Alpha()<1);}
    FKartFixedStepClock Hitch;Hitch.Advance(1,[](double){return true;});TestEqual(TEXT("Hitch cap"),Hitch.Steps,uint64(8));TestTrue(TEXT("Dropped time counted"),Hitch.DroppedSeconds>.9);
    UWorld* W=FAutomationEditorCommonUtils::CreateNewMap();KartStage9::Box(W,FVector(0,0,-10),FVector(5000,5000,10));
    auto* Wall=KartStage9::Box(W,FVector(300,0,50),FVector(1,500,50));
    FKartExperimentalVehicle V;V.Reset({});V.Body.SetVelocities(FVector(1500,0,0),FVector::ZeroVector);
    for(auto& Wheel:V.Wheels)Wheel.AngularVelocity=1500/12.;
    bool Valid=true;double MaxX=0,Impulse=0;bool RecordedThinDivergence=false;
    for(int I=0;I<60;++I){FVector Hit;FString Reason;const bool Ok=FKartCustomSceneBridge::Step(*W,nullptr,V,{},1./120,Hit,&Reason);if(Ok && !RecordedThinDivergence && V.Body.GetPosition().X>249.001){RecordedThinDivergence=true;AddInfo(FString::Printf(TEXT("First thin-wall bound divergence step%d pos%s quat%s velocity%s omega%s impulse%s"),I,*V.Body.GetPosition().ToString(),*V.Body.GetOrientation().ToString(),*V.Body.GetLinearVelocity().ToString(),*V.Body.GetAngularVelocity().ToString(),*Hit.ToString()));}Valid&=Ok;MaxX=FMath::Max(MaxX,V.Body.GetPosition().X);Impulse+=Hit.Size();if(!Ok){AddInfo(FString::Printf(TEXT("Bridge stopped step%d: %s angular %.6f"),I,*Reason,V.Body.GetAngularVelocity().Size()));break;}}
    TestTrue(TEXT("Thin wall swept response valid"),Valid);
    TestTrue(TEXT("No translation tunneling"),MaxX<=249.001 && Impulse>0);
    AddInfo(FString::Printf(TEXT("Thin wall max x %.9f impulse %.9f valid %d"),MaxX,Impulse,Valid));
    for(double Angle:{.25,.65})for(double Speed:{300.,1500.})
    {
        FKartExperimentalVehicle Glance;Glance.Reset({});const FQuat Q(FVector(0,0,1),Angle);
        Glance.Body.SetPose(FVector(0,0,22),Q);Glance.Body.SetVelocities(Q.RotateVector(FVector(Speed,0,0)),FVector::ZeroVector);
        for(auto& Wheel:Glance.Wheels)Wheel.AngularVelocity=Speed/12.;
        bool Ok=true;FString Reason;int FailedStep=-1;
        for(int J=0;J<240;++J){FVector JImpulse;if(!FKartCustomSceneBridge::Step(*W,nullptr,Glance,{},1./120,JImpulse,&Reason)){Ok=false;FailedStep=J;break;}}
        TestTrue(TEXT("Angled wall continuation"),Ok);
        AddInfo(FString::Printf(TEXT("Glancing angle %.2f speed %.0f valid%d step%d reason %s"),Angle,Speed,Ok,FailedStep,*Reason));
    }
    Wall->Destroy();
    auto* Pawn=W->SpawnActor<AKartPawn>(FVector(0,0,22),FRotator::ZeroRotator);
    TestFalse(TEXT("Custom needs explicit tag"),Pawn->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));
    Pawn->Tags.Add(TEXT("CustomPhysicsTest"));TestTrue(TEXT("Single grounded tagged kart admitted"),Pawn->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));
    Pawn->SetDriveInput(1,0);auto* Coordinator=W->GetSubsystem<UKartPhysicsWorldSubsystem>();
    for(int I=0;I<120;++I)Coordinator->AdvanceTestKart(*Pawn,1./120);
    TestTrue(TEXT("Custom input drives actor and HUD speed"),Pawn->GetActorLocation().X>50 && Pawn->GetSpeedKmh()>5);
    TestFalse(TEXT("Moving owner swap rejected"),Pawn->TrySelectPhysicsBackend(EKartPhysicsBackend::Legacy));
    const auto Q=Pawn->GetActorQuat();Pawn->ReceiveCollision(800,FVector(0,1,0));
    TestTrue(TEXT("Incident does not rotate custom body"),Pawn->GetActorQuat()==Q);
    Pawn->ResetKart();TestTrue(TEXT("Reset zero velocity"),Pawn->GetVelocity().IsZero() && Pawn->GetSpeedKmh()==0);
    TestTrue(TEXT("Stationary Legacy fallback"),Pawn->TrySelectPhysicsBackend(EKartPhysicsBackend::Legacy));
    auto* Second=W->SpawnActor<AKartPawn>(FVector(0,300,22),FRotator::ZeroRotator);
    TestFalse(TEXT("Mixed/multiple kart admission rejected"),Pawn->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));Second->Destroy();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage9Matrix,"RoadToF1.Physics.Stage9StaticMatrix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage9Matrix::RunTest(const FString&)
{
    struct FCase {const TCHAR* Name;double WallYaw,KartYaw,Speed,Gap,Mu;bool Reverse=false,Corner=false;};
    const FCase Cases[]={
        {TEXT("head-on"),0,0,1500,100,0},{TEXT("shallow"),0,.25,300,100,0},
        {TEXT("medium"),0,.65,300,100,0},{TEXT("tangential"),0,1.4,300,1,0},
        {TEXT("world-Y"),UE_DOUBLE_PI/2,.25,300,100,0},{TEXT("wall30"),UE_DOUBLE_PI/6,.25,300,100,0},
        {TEXT("wall45"),UE_DOUBLE_PI/4,.25,300,100,0},{TEXT("wall60"),UE_DOUBLE_PI/3,.25,300,100,0},
        {TEXT("initial-overlap"),0,0,0,-.1,0},{TEXT("resting"),0,0,0,-.01,0},
        {TEXT("reverse-away"),0,0,300,1,0,true},{TEXT("repeated-small"),0,.25,20,.01,0},
        {TEXT("low-friction"),0,.65,300,1,.1},{TEXT("high-friction"),0,.65,300,1,.8},
        {TEXT("off-centre"),0,.45,600,1,0},{TEXT("near-zero"),0,0,.01,0,0},
        {TEXT("fast-thin"),0,0,1500,100,0},{TEXT("corner"),0,.65,300,1,0,false,true}};
    UWorld* W=FAutomationEditorCommonUtils::CreateNewMap();KartStage9::Box(W,FVector(0,0,-10),FVector(5000,5000,10));
    for(const auto& C:Cases)
    {
        const FQuat WallQ(FVector::UpVector,C.WallYaw),Q=WallQ*FQuat(FVector::UpVector,C.KartYaw);
        auto* Wall=KartStage9::Box(W,FVector(300,0,50),FVector(1,500,50));Wall->SetActorRotation(WallQ);
        AActor* Other=C.Corner?KartStage9::Box(W,FVector(0,90,50),FVector(500,1,50)):nullptr;
        const double Radius=50*FMath::Abs(FMath::Cos(C.KartYaw))+40*FMath::Abs(FMath::Sin(C.KartYaw));
        FKartExperimentalVehicle V;V.Reset({});
        V.Body.SetPose(FVector(300,0,22)-WallQ.RotateVector(FVector(1+Radius+C.Gap,0,0)),Q);
        V.Body.SetVelocities(Q.RotateVector(FVector(C.Speed,0,0)),FVector::ZeroVector);
        const FVector InitialPosition=V.Body.GetPosition();int FirstRestDivergence=-1;
        for(auto& Wheel:V.Wheels)Wheel.AngularVelocity=C.Speed/12.;
        double MaxDepth=0,MaxOmega=0,MaxSpeed=0,MaxResidual=0;int Contacts=0,Iterations=0;bool Valid=true;FString Reason;int Failed=-1;
        for(int I=0;I<600;++I)
        {
            FKartVehicleInput Input;if(C.Reverse && I>=180)Input.Throttle=-1;
            if(FString(C.Name)==TEXT("repeated-small"))Input.Throttle=(I/60)%2?.1:-.1;
            FVector J;FKartSceneStepStats S;
            if(!FKartCustomSceneBridge::Step(*W,nullptr,V,Input,1./120,J,&Reason,&S,{C.Mu,C.Mu,0})){Valid=false;Failed=I;break;}
            MaxDepth=FMath::Max(MaxDepth,S.MaxPenetrationCm);MaxOmega=FMath::Max(MaxOmega,V.Body.GetAngularVelocity().Size());
            MaxSpeed=FMath::Max(MaxSpeed,V.Body.GetLinearVelocity().Size());MaxResidual=FMath::Max(MaxResidual,S.MaxResidualClosingSpeed);
            Contacts+=S.Contacts;Iterations+=S.Iterations;
            if(C.Speed==0 && FirstRestDivergence<0 && V.Body.GetLinearVelocity().Size()>5)
            {FirstRestDivergence=I;AddInfo(FString::Printf(TEXT("First resting divergence %s step%d pos%s velocity%s omega%s depth%.9g impulse%s"),C.Name,I,*V.Body.GetPosition().ToString(),*V.Body.GetLinearVelocity().ToString(),*V.Body.GetAngularVelocity().ToString(),S.MaxPenetrationCm,*J.ToString()));}
            const double Across=WallQ.UnrotateVector(V.Body.GetPosition()-Wall->GetActorLocation()).X;
            if(Across>=0){Valid=false;Reason=TEXT("Crossed wall centre plane (tunneling)");Failed=I;break;}
        }
        TestTrue(C.Name,Valid);TestTrue(TEXT("Bounded penetration <= 2 cm"),MaxDepth<=2);
        TestTrue(TEXT("Finite bounded angular response"),FMath::IsFinite(MaxOmega) && MaxOmega<30);
        if(C.Speed==0){TestTrue(TEXT("Resting contact drift under 5 cm over 5 seconds"),(V.Body.GetPosition()-InitialPosition).Size()<5);
            TestTrue(TEXT("Resting correction stays under 5 cm/s"),MaxSpeed<5);}
        if(C.Reverse)TestTrue(TEXT("Reverse leaves barrier by at least 20 cm"),V.Body.GetPosition().X<229);
        if(FString(C.Name)==TEXT("tangential"))TestTrue(TEXT("Tangential velocity produces real sliding distance"),V.Body.GetPosition().Y-InitialPosition.Y>200);
        AddInfo(FString::Printf(TEXT("%s valid%d step%d depth%.9g omega%.9g speed%.9g residual%.9g contacts%d iterations%d end%s velocity%s reason%s"),
            C.Name,Valid,Failed,MaxDepth,MaxOmega,MaxSpeed,MaxResidual,Contacts,Iterations,*V.Body.GetPosition().ToString(),*V.Body.GetLinearVelocity().ToString(),*Reason));
        Wall->Destroy();if(Other)Other->Destroy();
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage9NearTouch,"RoadToF1.Physics.Stage9BoxNearTouch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage9NearTouch::RunTest(const FString&)
{
    FKartConvexShape A,B;A.HalfExtents=FVector(50,40,10);B.HalfExtents=FVector(1,500,50);B.Position=FVector(300,0,50);
    for(double Angle:{0.,.25,.28,.65})for(double Depth:{-.001,-.0001,.0001,.001})
    {
        A.Orientation=FQuat(FVector::UpVector,Angle);const double Radius=50*FMath::Cos(Angle)+40*FMath::Sin(Angle);
        A.Position=FVector(299-Radius+Depth,22,22);const auto M=KartManifolds::Build({{1},{2}},A,B);
        if(Depth<0)TestEqual(TEXT("Analytical separated boxes"),M.Status,EKartManifoldStatus::NoContact);
        else {TestEqual(TEXT("Analytical shallow box overlap"),M.Status,EKartManifoldStatus::Valid);
            TestTrue(TEXT("Fresh anchors and witnesses"),KartManifolds::ValidContact(M,A,B));}
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage9Coverage,"RoadToF1.Physics.Stage9CoverageAudit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage9Coverage::RunTest(const FString&)
{
    if(!TestTrue(TEXT("Load actual race map read-only"),FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true)))return false;
    UWorld* W=GEditor->GetEditorWorldContext().World();int Boxes=0,Unsupported=0;
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UPrimitiveComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components)if(C->IsQueryCollisionEnabled() && C->GetCollisionObjectType()==ECC_WorldStatic)
        {
            if(Cast<UBoxComponent>(C))++Boxes;
            else {++Unsupported;AddInfo(FString::Printf(TEXT("Unsupported static component %s class %s"),*C->GetPathName(),*C->GetClass()->GetName()));}
        }
    }
    AddInfo(FString::Printf(TEXT("Coverage supported boxes%d unsupported static components%d. Audit is not full-track acceptance."),Boxes,Unsupported));
    if(Unsupported>0)TestFalse(TEXT("Incomplete coverage keeps runtime admission closed"),FKartCustomSceneBridge::IsGameplayReady());
    return true;
}
#endif
