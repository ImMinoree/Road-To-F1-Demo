#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartCustomFleet.h"
#include "KartStaticMeshGeometry.h"
#include "KartPhysicsSolver.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"
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
#endif
