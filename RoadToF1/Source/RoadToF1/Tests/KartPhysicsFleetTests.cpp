#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartCustomFleet.h"
#include "KartStaticMeshGeometry.h"
#include "KartPhysicsSolver.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "KartPhysicsDrivingEnvelope.h"
namespace KartFleetTest
{
UWorld* MakeWorld()
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* A=W->SpawnActor<AActor>();auto* C=NewObject<UBoxComponent>(A);A->SetRootComponent(C);
    C->SetBoxExtent(FVector(20000,20000,10));C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);C->RegisterComponent();A->SetActorLocation(FVector(0,0,-10));return W;
}
FKartFleetBody Make(uint64 Id,FVector P,FVector Velocity)
{
    FKartFleetBody E;E.Id={Id};FKartVehicleSettings C;C.SpeedCapKmh=55;C.CollisionHalfExtent=FVector(98,63,20);E.Vehicle.Reset(C,P);
    E.Vehicle.Body.SetVelocities(Velocity,FVector::ZeroVector);for(auto& Wheel:E.Vehicle.Wheels)Wheel.AngularVelocity=Velocity.X/12.;return E;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartSpeculativePairTest,"RoadToF1.Physics.Stage9SpeculativePair",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartSpeculativePairTest::RunTest(const FString&)
{
    FKartRigidBodyState A,B;A.ConfigureBox(160,FVector(100,80,30));B=A;A.SetPose(FVector(0,0,22),FQuat::Identity);B.SetPose(FVector(205,0,22),FQuat::Identity);A.SetVelocities(FVector(1500,0,0),FVector::ZeroVector);
    FKartContactManifold M;M.Pair={{1},{2}};M.Status=EKartManifoldStatus::Valid;M.Normal=FVector(1,0,0);
    FKartContactPoint P;P.WorldA=P.WorldB=FVector(102.5,0,22);P.LocalA=P.WorldA-A.GetPosition();P.LocalB=P.WorldB-B.GetPosition();P.AllowedClosingSpeed=9*120;M.Points.Add(P);
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> Bodies;Bodies.Add({1},A);Bodies.Add({2},B);
    const auto R=FKartPhysicsSolver::Solve(Bodies,{M},1./120);
    TestTrue(TEXT("Speculative shared constraint valid"),R.bSuccess);
    TestTrue(TEXT("Follower retains analytical speed1290"),FMath::Abs(Bodies[{1}].GetLinearVelocity().X-1290)<1.e-7);
    TestTrue(TEXT("Leader receives equal impulse210"),FMath::Abs(Bodies[{2}].GetLinearVelocity().X-210)<1.e-7);
    TestTrue(TEXT("Relative motion closes real gap exactly"),FMath::Abs((Bodies[{1}].GetLinearVelocity().X-Bodies[{2}].GetLinearVelocity().X)/120-9)<1.e-7);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartFleetCollisionTest,"RoadToF1.Physics.Stage9FleetContacts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartFleetCollisionTest::RunTest(const FString&)
{
    auto* W=KartFleetTest::MakeWorld();FKartStaticMeshGeometry Scene;FString Failure;if(!TestTrue(TEXT("Exact floor snapshot"),Scene.Build(*W,Failure)))return false;
    const TCHAR* Names[]={TEXT("direct"),TEXT("side-by-side"),TEXT("glance"),TEXT("rear-end"),TEXT("three-chain")};
    for(int Case=0;Case<5;++Case)
    {
        FKartCustomFleet Fleet;Fleet.bDetailedDiagnostics=true;
        if(Case==1){Fleet.Bodies.Add(KartFleetTest::Make(1,FVector(0,-65,22),FVector(1000,0,0)));Fleet.Bodies.Add(KartFleetTest::Make(2,FVector(0,65,22),FVector(1000,0,0)));}
        else
        {
            Fleet.Bodies.Add(KartFleetTest::Make(1,FVector(-150,Case==2?45:0,22),FVector(1000,0,0)));
            Fleet.Bodies.Add(KartFleetTest::Make(2,FVector(150,0,22),FVector(Case<3?-1000:300,0,0)));
            if(Case==4)Fleet.Bodies.Add(KartFleetTest::Make(3,FVector(-410,0,22),FVector(1200,0,0)));
        }
        bool Valid=true;double MaxDepth=0;int FirstOverlap=-1;
        for(int Step=0;Step<240;++Step)
        {
            if(!Fleet.Step(*W,Scene,1./120,Failure)){Valid=false;AddInfo(FString::Printf(TEXT("FLEET_FAIL %s step%d %s"),Names[Case],Step,*Failure));break;}
            for(int I=0;I<Fleet.Bodies.Num();++I)for(int J=I+1;J<Fleet.Bodies.Num();++J)
            {
                FKartConvexShape A,B;const auto& VA=Fleet.Bodies[I].Vehicle;const auto& VB=Fleet.Bodies[J].Vehicle;
                A.Position=VA.Body.GetPosition();A.Orientation=VA.Body.GetOrientation();A.HalfExtents=VA.Settings.CollisionHalfExtent;
                B.Position=VB.Body.GetPosition();B.Orientation=VB.Body.GetOrientation();B.HalfExtents=VB.Settings.CollisionHalfExtent;
                const auto M=KartManifolds::Build({Fleet.Bodies[I].Id,Fleet.Bodies[J].Id},A,B);
                if(M.Status!=EKartManifoldStatus::NoContact && M.Status!=EKartManifoldStatus::Valid){Valid=false;Failure=TEXT("Post-step manifold failure");}
                for(const auto& P:M.Points)MaxDepth=FMath::Max(MaxDepth,P.Depth);
            }
            if(MaxDepth>=.2 && FirstOverlap<0){FirstOverlap=Step;AddInfo(Fleet.LastConstraintSummary);
                for(const auto& E:Fleet.Bodies)AddInfo(FString::Printf(TEXT("FIRST_CHAIN_OVERLAP case%s step%d id%llu pos%s velocity%s omega%s impulse%s staticPairs%llu"),Names[Case],Step,E.Id.Value,*E.Vehicle.Body.GetPosition().ToString(),*E.Vehicle.Body.GetLinearVelocity().ToString(),*E.Vehicle.Body.GetAngularVelocity().ToString(),*E.Impulse.ToString(),Fleet.GetDiagnostics().StaticPairsSolved));}
        }
        TestTrue(Names[Case],Valid);TestTrue(TEXT("Actual pair penetration below2mm"),MaxDepth<.2);
        TestEqual(TEXT("No duplicate physical pair"),Fleet.GetDiagnostics().DuplicatePairs,0);
        if(Case!=1)TestTrue(TEXT("Real collision constraint exercised"),Fleet.GetDiagnostics().PairsSolved>0);
        AddInfo(FString::Printf(TEXT("FLEET_CONTACT %s valid%d maxDepth%.9g pairs%llu maxStepMs%.6g failure%s"),Names[Case],Valid,MaxDepth,Fleet.GetDiagnostics().PairsSolved,Fleet.GetDiagnostics().MaxStepMilliseconds,*Failure));
        if(!Valid || MaxDepth>=.2)return false;
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartFleetProgressionTest,"RoadToF1.Physics.Stage9FleetProgression",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartFleetProgressionTest::RunTest(const FString&)
{
    auto* W=KartFleetTest::MakeWorld();FKartStaticMeshGeometry Scene;FString Failure;if(!TestTrue(TEXT("Exact floor snapshot"),Scene.Build(*W,Failure)))return false;
    for(int Count:{2,5,10,20})
    {
        FKartCustomFleet Fleet,Replay;
        for(int I=0;I<Count;++I){auto E=KartFleetTest::Make(I+1,FVector(-(I/2)*260,I%2?90:-90,22),FVector::ZeroVector);E.Input.Throttle=1;Fleet.Bodies.Add(E);}
        Replay.Bodies=Fleet.Bodies;Algo::Reverse(Replay.Bodies);
        bool Valid=true;double StartX=Fleet.Bodies[0].Vehicle.Body.GetPosition().X;
        for(int Step=0;Step<360;++Step)
        {
            if(!Fleet.Step(*W,Scene,1./120,Failure) || !Replay.Step(*W,Scene,1./120,Failure)){Valid=false;break;}
        }
        TestTrue(FString::Printf(TEXT("Progressive%d-body world simulation"),Count),Valid);
        if(!Valid){AddError(Failure);return false;}
        TestTrue(TEXT("Fleet accelerates and advances"),Fleet.Bodies[0].Vehicle.Body.GetPosition().X>StartX+500);
        TestEqual(TEXT("Physical launch remains zero-contact"),Fleet.GetDiagnostics().PairsSolved,uint64(0));
        for(int I=0;I<Count;++I)TestTrue(TEXT("Ordering-independent exact replay"),Fleet.Bodies[I].Vehicle.Body.GetPosition()==Replay.Bodies[I].Vehicle.Body.GetPosition() && Fleet.Bodies[I].Vehicle.Body.GetLinearVelocity()==Replay.Bodies[I].Vehicle.Body.GetLinearVelocity());
        AddInfo(FString::Printf(TEXT("FLEET_PROGRESS bodies%d steps%llu meanMs%.6g maxMs%.6g treeHeight%d"),Count,Fleet.GetDiagnostics().Steps,Fleet.GetDiagnostics().TotalStepMilliseconds/Fleet.GetDiagnostics().Steps,Fleet.GetDiagnostics().MaxStepMilliseconds,Fleet.GetBroadphase().GetDiagnostics().Height));
        TestTrue(TEXT("Remove a live body"),Fleet.Remove(Fleet.Bodies.Last().Id));
        if(Fleet.Bodies.Num())TestTrue(TEXT("Removal leaves valid world step"),Fleet.Step(*W,Scene,1./120,Failure));
        Fleet.ResetCaches();TestEqual(TEXT("Reset clears diagnostics"),Fleet.GetDiagnostics().Steps,uint64(0));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartAngularFleetTest,"RoadToF1.Physics.Stage10AngularPairRefinement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartAngularFleetTest::RunTest(const FString&)
{
    auto* W=KartFleetTest::MakeWorld();FKartStaticMeshGeometry Scene;FString Failure;
    if(!TestTrue(TEXT("Angular fixture floor"),Scene.Build(*W,Failure)))return false;
    FKartCustomFleet Fleet;
    // Measured full-race angular/closing velocities, moved apart along the
    // captured separating axis to start with a real clear gap, not overlap.
    const FVector N=FVector(-.998,.047,.030).GetSafeNormal();
    auto A=KartFleetTest::Make(1,FVector(-981.1708418,-5429.1329943,24.5224199)-N*8,FVector(355.3835785,2.6551255,11.0978481));
    auto B=KartFleetTest::Make(2,FVector(-1168.0435034,-5415.8712332,24.5522460)+N*8,FVector(1228.3921570,35.8109278,14.1940570));
    A.Vehicle.Body.SetPose(A.Vehicle.Body.GetPosition(),FQuat(.0015268501,.0147741149,-.0236586632,.9996097548).GetNormalized());
    B.Vehicle.Body.SetPose(B.Vehicle.Body.GetPosition(),FQuat(.0004666654,.0148609844,-.0216564706,-.9996549058).GetNormalized());
    A.Vehicle.Body.SetVelocities(A.Vehicle.Body.GetLinearVelocity(),FVector(.7857917,3.3006089,-6.7970814));
    B.Vehicle.Body.SetVelocities(B.Vehicle.Body.GetLinearVelocity(),FVector(.2120988,-3.5732303,6.3587851));
    A.Vehicle.Settings.StaticBottomInsetCm=B.Vehicle.Settings.StaticBottomInsetCm=7.5;
    Fleet.Bodies={A,B};
    for(int I=0;I<240;++I)if(!Fleet.Step(*W,Scene,1./120,Failure)){AddError(Failure);return false;}
    TestTrue(TEXT("Rotating pair does not penetrate2mm"),Fleet.GetDiagnostics().MaxActualBoxOverlapCm<.2);
    // The1mm integrator may safely accept this small isolated pair directly;
    // the actual packed-race fixture separately requires refinement activation.
    TestEqual(TEXT("No duplicate response"),Fleet.GetDiagnostics().DuplicatePairs,0);
    AddInfo(FString::Printf(TEXT("ANGULAR_PAIR actualDepth%.9g refinements%llu maxLevel%d"),Fleet.GetDiagnostics().MaxActualBoxOverlapCm,Fleet.GetDiagnostics().RefinedSteps,Fleet.GetDiagnostics().MaxRefinement));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartCapturedEdgeTest,"RoadToF1.Physics.Stage10CapturedEdgeContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartCapturedEdgeTest::RunTest(const FString&)
{
    FKartConvexShape A,B;A.HalfExtents=B.HalfExtents=FVector(98,63,20);
    A.Position=FVector(12484.933810121189,-3728.3030032811757,17.986183761209208);
    A.Orientation=FQuat(-.00097255671143530213,-.000078851854878445981,.06167536590910741,.99809578556158918);
    B.Position=FVector(12693.207964745199,-3833.711666622482,23.037644650610712);
    B.Orientation=FQuat(-.012620285326250228,-.014804567532864119,.041998806405852566,.99892825239800531);
    const auto M=KartManifolds::Build({{1},{2}},A,B);
    TestEqual(TEXT("Captured edge contact valid"),M.Status,EKartManifoldStatus::Valid);
    TestTrue(TEXT("Finite exact edge witness"),KartManifolds::ValidContact(M,A,B));
    TestEqual(TEXT("Independent SAT fallback identified honestly"),M.Method,EKartManifoldMethod::BoxSATClip);
    for(const auto& P:M.Points)TestTrue(TEXT("Captured contact remains shallow"),P.Depth<.03);
    const auto Swap=KartManifolds::Build({{1},{2}},B,A);TestTrue(TEXT("Swapped edge geometry valid"),KartManifolds::ValidContact(Swap,B,A));
    TestTrue(TEXT("Swapped edge normal reverses"),Swap.Normal.Equals(-M.Normal,1.e-6));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStaticRetreatTest,"RoadToF1.Physics.Stage10StaticRetreatQuery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStaticRetreatTest::RunTest(const FString&)
{
    auto* W=KartFleetTest::MakeWorld();
    auto Wall=[&](FVector P,FVector Extent)
    {
        auto* A=W->SpawnActor<AActor>();auto* C=NewObject<UBoxComponent>(A);A->SetRootComponent(C);C->SetBoxExtent(Extent);C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);C->RegisterComponent();A->SetActorLocation(P);
    };
    Wall(FVector(0,75,100),FVector(500,12,100));Wall(FVector(-800,0,100),FVector(20,200,100));
    auto* K=W->SpawnActor<AKartPawn>(FVector(0,0,22),FRotator::ZeroRotator);K->ConfigureRacer(0,false);
    auto* F=W->GetSubsystem<UKartPhysicsWorldSubsystem>();if(!TestTrue(TEXT("Retreat fixture admitted"),F->SelectFleet({K})))return false;
    TestTrue(TEXT("Tangent retreat along touching wall/floor allowed"),F->CanTranslateStaticChassis(*K,FVector(-300,0,0)));
    TestTrue(TEXT("Separating retreat from contact allowed"),F->CanTranslateStaticChassis(*K,FVector(0,-100,0)));
    TestFalse(TEXT("Moving deeper into touching wall rejected"),F->CanTranslateStaticChassis(*K,FVector(0,100,0)));
    TestFalse(TEXT("New obstacle in retreat corridor rejected"),F->CanTranslateStaticChassis(*K,FVector(-1000,0,0)));
    F->ReleaseFleet();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartHeadingRecoveryTest,"RoadToF1.Physics.Stage10HeadingRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartHeadingRecoveryTest::RunTest(const FString&)
{
    auto* W=KartFleetTest::MakeWorld();FKartStaticMeshGeometry Scene;FString Failure;if(!Scene.Build(*W,Failure))return false;
    FKartCustomFleet Fleet;auto E=KartFleetTest::Make(1,FVector(0,0,24.5),FVector::ZeroVector);
    E.Vehicle.Settings.StaticBottomInsetCm=7.5;E.Vehicle.Body.SetPose(E.Vehicle.Body.GetPosition(),FQuat(FVector::UpVector,PI));Fleet.Bodies={E};
    const FVector Target(1000,0,24.5);bool Reacquired=false;
    TestTrue(TEXT("Directly behind target has nonzero bounded steering"),FMath::Abs(KartPhysicsDriving::SteeringTowardTarget(FVector(-1000,0,0),0))==1);
    TestEqual(TEXT("Normal forward steering unchanged"),KartPhysicsDriving::SteeringTowardTarget(FVector(1000,50,0),.2),.2);
    for(int I=0;I<2400;++I)
    {
        auto& V=Fleet.Bodies[0].Vehicle;const auto Local=V.Body.GetOrientation().UnrotateVector(Target-V.Body.GetPosition());
        const double Speed=V.Body.GetLinearVelocity().Size2D();const double Curvature=2*Local.Y/FMath::Max(10000.,Local.SizeSquared2D());
        const double Nominal=FMath::Clamp(FMath::Atan(Curvature*100)*(1+FMath::Square(Speed/1300.))/.45,-1.,1.);
        Fleet.Bodies[0].Input={Speed<250?1.:0.,KartPhysicsDriving::SteeringTowardTarget(Local,Nominal),Speed>300?1.:0.};
        if(!Fleet.Step(*W,Scene,1./120,Failure)){AddError(Failure);return false;}
        if(Local.X>200 && FMath::Abs(FMath::Atan2(Local.Y,Local.X))<.5){Reacquired=true;break;}
    }
    TestTrue(TEXT("Backward-facing physical kart reacquires target using normal controls"),Reacquired);return true;
}
#endif
