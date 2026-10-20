#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsManifoldCache.h"
#include "KartRigidBodyState.h"
#include "KartPawn.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include <limits>
namespace KartStage4Tests
{
const FKartPhysicsPair Pair({1},{2});
FKartConvexShape Box(FVector P=FVector::ZeroVector,FVector E=FVector(10),FQuat Q=FQuat::Identity)
{FKartConvexShape S;S.Position=P;S.HalfExtents=E;S.Orientation=Q;return S;}
FKartConvexShape Sphere(FVector P,double Radius=10){auto S=Box(P);S.Kind=EKartConvexKind::Sphere;S.Radius=Radius;return S;}
bool Surface(const FKartConvexShape& S,const FVector& W)
{
    const auto L=KartManifolds::ToLocal(S,W);
    if(S.Kind==EKartConvexKind::Sphere)return FMath::Abs(L.Size()-S.Radius)<.01;
    if(S.Kind!=EKartConvexKind::Box)return true;
    const auto D=L.GetAbs()-S.HalfExtents;
    return D.GetMax()<1.e-4 && FMath::Min3(FMath::Abs(D.X),FMath::Abs(D.Y),FMath::Abs(D.Z))<1.e-4;
}
bool Same(const TArray<FKartContactManifold>& A,const TArray<FKartContactManifold>& B)
{
    if(A.Num()!=B.Num())return false;
    for(int32 I=0;I<A.Num();++I)
    {
        if(!(A[I].Pair==B[I].Pair) || A[I].Status!=B[I].Status || A[I].Normal!=B[I].Normal || A[I].Points.Num()!=B[I].Points.Num())return false;
        for(int32 J=0;J<A[I].Points.Num();++J){const auto& X=A[I].Points[J];const auto& Y=B[I].Points[J];
            if(X.WorldA!=Y.WorldA || X.WorldB!=Y.WorldB || X.LocalA!=Y.LocalA || X.LocalB!=Y.LocalB || X.Feature!=Y.Feature || X.PersistentId!=Y.PersistentId || X.Age!=Y.Age || X.Depth!=Y.Depth)return false;}
    }
    return true;
}
void Register(FKartPhysicsBroadphase& Tree,TMap<FKartPhysicsBodyId,FKartConvexShape>& Shapes,uint64 Id,const FKartConvexShape& S,bool Static=false)
{FKartPhysicsProxy P;P.Id={Id};S.Bounds(P.Bounds);P.Mobility=Static?EKartProxyMobility::Static:EKartProxyMobility::Dynamic;Tree.RegisterProxy(P);Shapes.Add(P.Id,S);}
void Move(FKartPhysicsBroadphase& Tree,TMap<FKartPhysicsBodyId,FKartConvexShape>& Shapes,uint64 Id,const FKartConvexShape& S)
{FKartPhysicsAABB Bounds;S.Bounds(Bounds);Tree.UpdateProxy({Id},Bounds);Shapes.Add({Id},S);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage4Test,"RoadToF1.Physics.Stage4Manifolds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage4Test::RunTest(const FString& Parameters)
{
    using namespace KartStage4Tests;const auto A=Box(),B=Box(FVector(15,0,0));
    auto Check=[&](const FKartConvexShape& X,const FKartConvexShape& Y,int32 Count,double Depth)
    {
        const auto M=KartManifolds::Build(Pair,X,Y);TestEqual(TEXT("Valid manifold"),M.Status,EKartManifoldStatus::Valid);
        TestEqual(TEXT("Expected contact count"),M.Points.Num(),Count);TestTrue(TEXT("Valid anchor round trips"),KartManifolds::ValidContact(M,X,Y));
        for(const auto& P:M.Points){TestTrue(TEXT("Known depth"),FMath::Abs(P.Depth-Depth)<1.e-4);TestTrue(TEXT("Witness on actual surfaces"),Surface(X,P.WorldA) && Surface(Y,P.WorldB));}
        return M;
    };
    TestEqual(TEXT("No separated manifold"),KartManifolds::Build(Pair,A,Box(FVector(30,0,0))).Status,EKartManifoldStatus::NoContact);
    const auto Face=Check(A,B,4,5);TestTrue(TEXT("A towards B"),Face.Normal.Equals(FVector(1,0,0),1.e-6));
    for(int32 I=0;I<Face.Points.Num();++I)for(int32 J=I+1;J<Face.Points.Num();++J)TestTrue(TEXT("Genuine separate patch points"),(Face.Points[I].WorldA-Face.Points[J].WorldA).Size()>1);
    Check(A,Box(FVector(19.999,0,0)),4,.001);Check(A,Box(FVector(1,0,0)),4,19);
    const auto SS=Check(Sphere(FVector::ZeroVector),Sphere(FVector(15,0,0)),1,5);
    const auto Coincident=Check(Sphere(FVector::ZeroVector),Sphere(FVector::ZeroVector),1,20);
    TestTrue(TEXT("Coincident normal documented ambiguous"),Coincident.bAmbiguousNormal && Coincident.Normal==FVector(1,0,0));
    Check(A,Sphere(FVector(15,0,0)),1,5);Check(A,Sphere(FVector(2,0,0),3),1,11);
    const auto SphereBoxSwap=Check(Sphere(FVector(15,0,0)),A,1,5);TestTrue(TEXT("Sphere box swap direction"),SphereBoxSwap.Normal.Equals(FVector(-1,0,0),1.e-9));
    const auto NearCenters=KartManifolds::Build(Pair,Sphere(FVector::ZeroVector),Sphere(FVector(0,1.e-8,0)));
    TestTrue(TEXT("Distinct sphere centers retain unique normal"),!NearCenters.bAmbiguousNormal && NearCenters.Normal==FVector(0,1,0));
    const auto Reverse=Check(B,A,4,5);TestTrue(TEXT("Swap normal"),Reverse.Normal.Equals(-Face.Normal,1.e-6));
    for(const auto& P:Face.Points){bool Found=false;for(const auto& Q:Reverse.Points)if(P.WorldA.Equals(Q.WorldB,1.e-6) && P.WorldB.Equals(Q.WorldA,1.e-6))Found=true;TestTrue(TEXT("Swap witnesses"),Found);}
    const FQuat Q(FVector(1,2,3).GetSafeNormal(),.6);const auto RotA=Box(FVector(50,60,70),FVector(10),Q),RotB=Box(RotA.Position+Q.RotateVector(FVector(15,0,0)),FVector(10),Q);
    Check(RotA,RotB,4,5);
    const auto Oct=KartManifolds::Build(Pair,A,Box(FVector(15,0,0),FVector(10),FQuat(FVector(1,0,0),.7853981633974483)));
    TestTrue(TEXT("Eight clipped candidates reduced to four"),Oct.CandidateCount>4 && Oct.Points.Num()==4);
    FKartManifoldSettings Limit;Limit.MaxContacts=2;TestEqual(TEXT("Configured bound"),KartManifolds::Build(Pair,A,B,Limit).Points.Num(),2);
    Limit.Narrowphase.EPAIterations=0;const auto Failed=KartManifolds::Build(Pair,A,B,Limit);
    TestTrue(TEXT("EPA failure not promoted"),Failed.Status==EKartManifoldStatus::GeometryFailure && Failed.Points.IsEmpty());
    auto Hull=A;Hull.Kind=EKartConvexKind::Hull;for(double X:{-10.,10.})for(double Y:{-10.,10.})for(double Z:{-10.,10.})Hull.Vertices.Add(FVector(X,Y,Z));
    const auto HM=Check(Hull,B,1,5);TestEqual(TEXT("Hull witness limitation explicit"),HM.Method,EKartManifoldMethod::EPAWitness);
    auto Invalid=A;Invalid.Position.X=std::numeric_limits<double>::quiet_NaN();TestEqual(TEXT("Invalid rejected"),KartManifolds::Build(Pair,Invalid,B).Status,EKartManifoldStatus::InvalidInput);
    TestEqual(TEXT("Self pair rejected"),KartManifolds::Build(FKartPhysicsPair({1},{1}),A,B).Status,EKartManifoldStatus::InvalidInput);
    // A non-face normal identifies an edge interaction; confirm actual box surfaces.
    FRandomStream Random(40818);int32 EdgeCases=0;
    for(int32 I=0;I<60;++I)
    {
        const auto X=Box(FVector::ZeroVector,FVector(15,4,4),FQuat(FVector(1,2,3).GetSafeNormal(),Random.FRandRange(-2,2)));
        const auto Y=Box(FVector(4,3,2),FVector(15,4,4),FQuat(FVector(2,3,1).GetSafeNormal(),Random.FRandRange(-2,2)));
        const auto M=KartManifolds::Build(Pair,X,Y);
        TestTrue(TEXT("Random geometry resolves"),M.Status==EKartManifoldStatus::Valid || M.Status==EKartManifoldStatus::NoContact);
        if(M.Status==EKartManifoldStatus::Valid)
        {
            TestTrue(TEXT("Random finite anchors"),KartManifolds::ValidContact(M,X,Y));
            for(const auto& P:M.Points)TestTrue(TEXT("Random box surfaces"),Surface(X,P.WorldA) && Surface(Y,P.WorldB));
            if(M.Method==EKartManifoldMethod::EPAWitness){++EdgeCases;TestEqual(TEXT("Edge single point"),M.Points.Num(),1);}
        }
    }
    TestTrue(TEXT("Exercised edge contacts"),EdgeCases>0);

    FKartPhysicsBroadphase Tree;TMap<FKartPhysicsBodyId,FKartConvexShape> Shapes;Register(Tree,Shapes,1,A);Register(Tree,Shapes,2,B);
    TArray<FKartPhysicsPair> Pairs{Pair,Pair};FKartPhysicsManifoldCache Cache,Repeat;
    TestTrue(TEXT("Initial cache"),Cache.Update(Tree,Pairs,Shapes));TestEqual(TEXT("No duplicate pairs"),Cache.GetDiagnostics().Active,1);
    for(int32 I=0;I<8;++I)
    {
        Move(Tree,Shapes,2,Box(FVector(15,.02*I,0)));Cache.Update(Tree,Pairs,Shapes);Repeat.Update(Tree,Pairs,Shapes);
        if(I>0)TestTrue(TEXT("Small motion preserves contacts"),Cache.GetDiagnostics().Reused>=2);
    }
    Cache.Update(Tree,Pairs,Shapes);TestEqual(TEXT("Identical refresh reuses four"),Cache.GetDiagnostics().Reused,4);
    auto ChangedGeometry=Shapes.FindChecked({2});ChangedGeometry.HalfExtents.Z+=1;Move(Tree,Shapes,2,ChangedGeometry);Cache.Update(Tree,Pairs,Shapes);
    TestEqual(TEXT("Changed shape geometry invalidates identity"),Cache.GetDiagnostics().Reused,0);
    Tree.SetFilter({2},EKartProxyMobility::Dynamic,2,0xffffffff,true);Cache.Update(Tree,Pairs,Shapes);
    TestEqual(TEXT("Allowed filter change still resets contacts"),Cache.GetDiagnostics().Reused,0);
    FKartManifoldSettings BadSettings;BadSettings.DedupCm=std::numeric_limits<double>::infinity();const auto BeforeRejected=Cache.GetResults();
    TestFalse(TEXT("Invalid settings explicit rejection"),Cache.Update(Tree,Pairs,Shapes,BadSettings));TestTrue(TEXT("Rejected settings atomic"),Same(BeforeRejected,Cache.GetResults()));
    Cache.Update(Tree,{},Shapes);TestEqual(TEXT("Missing from complete candidate snapshot expires"),Cache.GetDiagnostics().Active,0);Cache.Update(Tree,Pairs,Shapes);
    auto Drift=Shapes.FindChecked({2});Drift.Position.Y=5;Move(Tree,Shapes,2,Drift);Cache.Update(Tree,Pairs,Shapes);TestTrue(TEXT("Large drift invalidates anchors"),Cache.GetDiagnostics().Invalidated>0);
    TMap<FKartPhysicsBodyId,uint64> Revisions;Revisions.Add({1},1);Cache.Update(Tree,Pairs,Shapes,{},&Revisions);TestEqual(TEXT("Replacement revision invalidates"),Cache.GetDiagnostics().Reused,0);
    Move(Tree,Shapes,2,Box(FVector(40,0,0)));Cache.Update(Tree,Pairs,Shapes);TestEqual(TEXT("Separated expires"),Cache.GetDiagnostics().Active,0);
    Move(Tree,Shapes,2,B);Cache.Update(Tree,Pairs,Shapes);Tree.SetFilter({2},EKartProxyMobility::Dynamic,1,0,true);Cache.Update(Tree,Pairs,Shapes);TestEqual(TEXT("Mask expires"),Cache.GetDiagnostics().Active,0);
    Tree.SetFilter({2},EKartProxyMobility::Dynamic,1,0xffffffff,true);Cache.Update(Tree,Pairs,Shapes);Tree.SetFilter({2},EKartProxyMobility::Dynamic,1,0xffffffff,false);Cache.Update(Tree,Pairs,Shapes);TestEqual(TEXT("Disabled expires"),Cache.GetDiagnostics().Active,0);
    Tree.SetFilter({2},EKartProxyMobility::Dynamic,1,0xffffffff,true);Cache.Update(Tree,Pairs,Shapes);Tree.RemoveProxy({2});Cache.Update(Tree,Pairs,Shapes);TestEqual(TEXT("Removed expires"),Cache.GetDiagnostics().Active,0);
    FKartPhysicsProxy Retired;Retired.Id={2};B.Bounds(Retired.Bounds);TestFalse(TEXT("Retired ID cannot recycle"),Tree.RegisterProxy(Retired));Cache.Reset();TestTrue(TEXT("Session reset empty"),Cache.GetResults().IsEmpty());

    // 200 bodies in100 isolated pairs. Full candidate snapshot, no live integration.
    FKartPhysicsBroadphase StressTree;TMap<FKartPhysicsBodyId,FKartConvexShape> StressShapes;
    for(uint64 I=1;I<=200;++I){const FVector P((I-1)/2*80,0,0);const auto Kind=((I-1)/2)%3;const bool IsSphere=Kind==1 || (Kind==2 && I%2==0);const auto S=IsSphere?Sphere(P+FVector(I%2?0:15,0,0)):Box(P+FVector(I%2?0:15,0,0));Register(StressTree,StressShapes,I,S,I%2==1);}
    FKartPhysicsManifoldCache One,Two;int32 Reused=0,New=0,Expired=0,Failures=0,Maximum=0,Active=0;
    for(int32 Step=0;Step<12;++Step)
    {
        for(uint64 I=2;I<=200;I+=2)
        {
            if(I==200 && Step>=6)continue;
            auto S=StressShapes.FindChecked({I});S.Position=FVector((I-1)/2*80+(Step%5==4?35:15),Step*.02,0);
            if(S.Kind==EKartConvexKind::Box)S.Orientation=FQuat(FVector(1,0,0),Step*.005);Move(StressTree,StressShapes,I,S);
        }
        if(Step==6){StressTree.RemoveProxy({200});StressShapes.Remove({200});}
        TArray<FKartPhysicsPair> Candidates;StressTree.GeneratePairs(Candidates);const auto BeforeTree=StressTree.GetDiagnostics();
        One.Update(StressTree,Candidates,StressShapes);Two.Update(StressTree,Candidates,StressShapes);
        TestTrue(TEXT("Deterministic cache sequence"),Same(One.GetResults(),Two.GetResults()));
        TestEqual(TEXT("Read-only tree"),StressTree.GetDiagnostics().PairNodeVisits,BeforeTree.PairNodeVisits);
        const auto D=One.GetDiagnostics();Reused+=D.Reused;New+=D.New;Expired+=D.Invalidated;Failures+=D.Failures;Maximum=FMath::Max(Maximum,D.MaxPoints);Active=D.Active;
        FKartPhysicsPair Previous;
        for(const auto& M:One.GetResults())
        {
            TestTrue(TEXT("Sorted unique pairs"),!Previous.IsValid() || Previous<M.Pair);Previous=M.Pair;
            TestTrue(TEXT("Removed absent"),Step<6 || M.Pair.Second().Value!=200);
            if(M.Status==EKartManifoldStatus::Valid)TestTrue(TEXT("Stress invariant"),KartManifolds::ValidContact(M,StressShapes.FindChecked(M.Pair.First()),StressShapes.FindChecked(M.Pair.Second())));
        }
    }
    TestEqual(TEXT("Stress no failures"),Failures,0);TestTrue(TEXT("Stress persistence exercised"),Reused>0 && New>0 && Expired>0 && Maximum<=4);
    AddInfo(FString::Printf(TEXT("STAGE4_STRESS bodies=200 steps=12 active=%d reused=%d new=%d invalidated=%d failures=%d max_points=%d edge_fixtures=%d"),Active,Reused,New,Expired,Failures,Maximum,EdgeCases));
    FKartRigidBodyState Body;Body.SetVelocities(FVector(1,2,3),FVector(4,5,6));Body.AddForce(FVector(7));Body.AddTorque(FVector(8));const auto Saved=Body;
    auto BodyShape=A;BodyShape.Position=Body.GetPosition();BodyShape.Orientation=Body.GetOrientation();KartManifolds::Build(Pair,BodyShape,B);
    TestTrue(TEXT("Read-only rigid body"),Body.GetPosition()==Saved.GetPosition() && Body.GetOrientation()==Saved.GetOrientation() && Body.GetLinearVelocity()==Saved.GetLinearVelocity()
        && Body.GetAngularVelocity()==Saved.GetAngularVelocity() && Body.GetForce()==Saved.GetForce() && Body.GetTorque()==Saved.GetTorque());
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Kart=World->SpawnActor<AKartPawn>(FVector(0,0,1000),FRotator::ZeroRotator);const auto Pose=Kart->GetActorTransform();
    KartManifolds::Build(Pair,A,B);TestTrue(TEXT("Live pawn unchanged"),Pose.Equals(Kart->GetActorTransform()) && Kart->GetCollisionCount()==0);
    TestFalse(TEXT("Custom rejected"),Kart->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));TestEqual(TEXT("Legacy only"),Kart->GetPhysicsBackend(),EKartPhysicsBackend::Legacy);Kart->Destroy();return true;
}
#endif
