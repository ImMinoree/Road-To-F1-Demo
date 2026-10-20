#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsBroadphase.h"
#include "KartRigidBodyState.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"
#include <limits>

namespace KartStage2Tests
{
FKartPhysicsProxy Proxy(uint64 Id,double X)
{
    FKartPhysicsProxy P;P.Id={Id};P.Bounds={FVector(X-10,-10,-10),FVector(X+10,10,10)};return P;
}
// Independent oracle: direct scalar bounds and independent category/mobility checks.
TArray<FKartPhysicsPair> Brute(const TArray<FKartPhysicsProxy>& Proxies)
{
    TArray<FKartPhysicsPair> Out;
    for(int32 I=0;I<Proxies.Num();++I)for(int32 J=I+1;J<Proxies.Num();++J)
    {
        const auto& A=Proxies[I];const auto& B=Proxies[J];
        if(!A.bEnabled || !B.bEnabled || A.Id==B.Id
            || (A.Mobility==EKartProxyMobility::Static && B.Mobility==EKartProxyMobility::Static)
            || !(A.Category&B.Mask) || !(B.Category&A.Mask))continue;
        bool Overlap=true;
        for(int32 Axis=0;Axis<3;++Axis)
            Overlap &= A.Bounds.Min[Axis]<=B.Bounds.Max[Axis] && B.Bounds.Min[Axis]<=A.Bounds.Max[Axis];
        if(Overlap)Out.Emplace(A.Id,B.Id);
    }
    Out.Sort();return Out;
}
bool ValidPairs(const TArray<FKartPhysicsPair>& Pairs)
{
    for(int32 I=0;I<Pairs.Num();++I)
        if(!Pairs[I].IsValid() || (I>0 && !(Pairs[I-1]<Pairs[I])))return false;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage2Test,"RoadToF1.Physics.Stage2Broadphase",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage2Test::RunTest(const FString& Parameters)
{
    using namespace KartStage2Tests;
    const double NaN=std::numeric_limits<double>::quiet_NaN(),Inf=std::numeric_limits<double>::infinity();
    FKartPhysicsBodyIdSource Source;
    const auto Id1=Source.Allocate(),Id2=Source.Allocate();
    TestFalse(TEXT("Zero ID invalid"),FKartPhysicsBodyId{}.IsValid());
    TestTrue(TEXT("Session IDs monotonic"),Id1.IsValid() && Id1<Id2);
    TestTrue(TEXT("Pair normalization"),FKartPhysicsPair(Id1,Id2)==FKartPhysicsPair(Id2,Id1));
    TestEqual(TEXT("Normalized pair hash"),GetTypeHash(FKartPhysicsPair(Id1,Id2)),GetTypeHash(FKartPhysicsPair(Id2,Id1)));
    TestFalse(TEXT("Self pair invalid"),FKartPhysicsPair(Id1,Id1).IsValid());

    const FKartPhysicsAABB A{FVector(0),FVector(2,4,6)};
    TestTrue(TEXT("Valid box"),A.IsValid());
    TestTrue(TEXT("Center/extents"),A.Center()==FVector(1,2,3) && A.Extents()==FVector(1,2,3));
    TestEqual(TEXT("Surface area"),A.SurfaceArea(),88.0);
    TestTrue(TEXT("Touching counts"),A.Overlaps({FVector(2,0,0),FVector(3,1,1)}));
    TestFalse(TEXT("Separated"),A.Overlaps({FVector(2.01,0,0),FVector(3,1,1)}));
    TestTrue(TEXT("Contained point/zero extent valid"),A.Contains({FVector(1),FVector(1)}));
    TestTrue(TEXT("Zero box touches itself"),FKartPhysicsAABB{}.Overlaps({}));
    FKartPhysicsAABB Expanded;
    TestTrue(TEXT("Expansion"),A.Expanded(2,Expanded) && Expanded.Min==FVector(-2) && Expanded.Max==FVector(4,6,8));
    const auto Union=FKartPhysicsAABB::Combine(A,{FVector(-1),FVector(1)});
    TestTrue(TEXT("Union"),Union.Min==FVector(-1) && Union.Max==A.Max);
    const FKartPhysicsAABB Invalid{FVector(1),FVector(-1)};
    TestFalse(TEXT("Inverted bounds invalid"),Invalid.IsValid());
    TestFalse(TEXT("NaN invalid"),(FKartPhysicsAABB{FVector(NaN),FVector(1)}).IsValid());
    TestFalse(TEXT("Infinity invalid"),(FKartPhysicsAABB{FVector(-1),FVector(Inf)}).IsValid());
    TestFalse(TEXT("Invalid union stays invalid"),FKartPhysicsAABB::Combine(A,Invalid).IsValid());
    const auto Previous=Expanded;
    TestFalse(TEXT("Negative expansion rejected"),A.Expanded(-1,Expanded));
    TestTrue(TEXT("Rejected expansion preserves output"),Expanded.Min==Previous.Min && Expanded.Max==Previous.Max);

    FKartRigidBodyState Body;Body.SetPose(FVector(100,200,300),FQuat::Identity);
    FKartPhysicsAABB Box;
    TestTrue(TEXT("Body AABB identity"),FKartPhysicsAABB::FromBody(Body,FVector(2,4,6),Box)
        && Box.Min==FVector(98,196,294) && Box.Max==FVector(102,204,306));
    // Use double precision for the analytical quarter-turn (PI is float precision).
    Body.SetPose(Body.GetPosition(),FQuat(FVector(0,0,1),3.14159265358979323846/2.0));
    TestTrue(TEXT("90 degree extent swap"),FKartPhysicsAABB::FromBody(Body,FVector(2,4,6),Box)
        && Box.Extents().Equals(FVector(4,2,6),1.e-9));
    Body.SetPose(Body.GetPosition(),FQuat(FVector(1,2,3).GetSafeNormal(),.73));
    TestTrue(TEXT("Arbitrary quaternion bounds"),FKartPhysicsAABB::FromBody(Body,FVector(2,4,6),Box));
    FVector CornerMin(1.e9),CornerMax(-1.e9);
    for(double X:{-2.0,2.0})for(double Y:{-4.0,4.0})for(double Z:{-6.0,6.0})
    {
        const FVector C=Body.GetPosition()+Body.GetOrientation().RotateVector(FVector(X,Y,Z));
        CornerMin=CornerMin.ComponentMin(C);CornerMax=CornerMax.ComponentMax(C);
    }
    TestTrue(TEXT("Oriented AABB equals eight-corner oracle"),Box.Min.Equals(CornerMin,1.e-9) && Box.Max.Equals(CornerMax,1.e-9));
    const auto BeforeBadBox=Box;
    TestFalse(TEXT("Bad half extents rejected"),FKartPhysicsAABB::FromBody(Body,FVector(-1),Box));
    TestFalse(TEXT("Bad quaternion rejected"),FKartPhysicsAABB::FromOrientedBox(FVector::ZeroVector,FQuat(0,0,0,0),FVector(1),Box));
    TestTrue(TEXT("Failed box creation atomic"),Box.Min==BeforeBadBox.Min && Box.Max==BeforeBadBox.Max);

    FKartPhysicsBroadphase Tree;TArray<FKartPhysicsPair> Pairs;
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Empty pairs"),Pairs.Num(),0);
    TestTrue(TEXT("Empty invariants"),Tree.Validate());
    TestFalse(TEXT("Invalid margin"),Tree.SetFatMargin(NaN));TestFalse(TEXT("Huge margin rejected"),Tree.SetFatMargin(101));
    TestTrue(TEXT("Configurable margin"),Tree.SetFatMargin(5));
    TestFalse(TEXT("Invalid ID rejected"),Tree.RegisterProxy(Proxy(0,0)));
    auto Bad=Proxy(99,0);Bad.Bounds=Invalid;
    TestFalse(TEXT("Bad AABB registration rejected"),Tree.RegisterProxy(Bad));
    TestTrue(TEXT("Register first"),Tree.RegisterProxy(Proxy(1,0)));
    TestFalse(TEXT("Duplicate registration rejected"),Tree.RegisterProxy(Proxy(1,0)));
    TestFalse(TEXT("Margin cannot change populated tree"),Tree.SetFatMargin(10));
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("One body no pairs"),Pairs.Num(),0);
    TestTrue(TEXT("Register separated with fat overlap"),Tree.RegisterProxy(Proxy(2,21)));
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Fat overlap alone is not an output candidate"),Pairs.Num(),0);
    TestTrue(TEXT("Move into overlap"),Tree.UpdateProxy({2},Proxy(2,15).Bounds));
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Two overlap one pair"),Pairs.Num(),1);
    const auto Reinserts=Tree.GetDiagnostics().Reinserts;
    TestTrue(TEXT("Tiny update"),Tree.UpdateProxy({2},Proxy(2,16).Bounds));
    TestEqual(TEXT("Contained movement avoids reinsertion"),Tree.GetDiagnostics().Reinserts,Reinserts);
    TestTrue(TEXT("Third overlap"),Tree.RegisterProxy(Proxy(3,8)));
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Three bodies three unique pairs"),Pairs.Num(),3);
    TestTrue(TEXT("Sorted unique non-self pairs"),ValidPairs(Pairs));
    TestTrue(TEXT("Static filter first"),Tree.SetFilter({1},EKartProxyMobility::Static,1,~0u,true));
    TestTrue(TEXT("Static filter second"),Tree.SetFilter({2},EKartProxyMobility::Static,1,~0u,true));
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Static-static excluded, dynamic-static included"),Pairs.Num(),2);
    Tree.SetFilter({3},EKartProxyMobility::Dynamic,2,1,true);
    Tree.SetFilter({1},EKartProxyMobility::Static,1,2,true);
    Tree.SetFilter({2},EKartProxyMobility::Static,1,1,true);
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Symmetric masks require both directions"),Pairs.Num(),1);
    Tree.SetFilter({3},EKartProxyMobility::Dynamic,2,1,false);
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Disabled excluded"),Pairs.Num(),0);
    Tree.SetFilter({3},EKartProxyMobility::Dynamic,1,~0u,true);
    Tree.SetFilter({1},EKartProxyMobility::Dynamic,1,~0u,true);
    Tree.SetFilter({2},EKartProxyMobility::Dynamic,1,~0u,true);
    TArray<FKartPhysicsBodyId> Hits;
    TestTrue(TEXT("AABB query valid"),Tree.QueryAABB({FVector(-10),FVector(10)},Hits));
    TestEqual(TEXT("Query finds three current bounds"),Hits.Num(),3);
    TestTrue(TEXT("Query IDs sorted"),Hits[0].Value==1 && Hits[1].Value==2 && Hits[2].Value==3);
    TestFalse(TEXT("Invalid query clears output"),Tree.QueryAABB(Invalid,Hits));TestEqual(TEXT("No stale results"),Hits.Num(),0);
    TestTrue(TEXT("Move out"),Tree.UpdateProxy({2},Proxy(2,200).Bounds));
    Tree.GeneratePairs(Pairs);TestEqual(TEXT("Moved out removes relevant pairs"),Pairs.Num(),1);
    TestTrue(TEXT("Large move reinserts"),Tree.GetDiagnostics().Reinserts>Reinserts);
    FKartPhysicsAABB Fat;Tree.GetFatBounds({2},Fat);
    TestTrue(TEXT("Fat follows large move"),Fat.Contains(Proxy(2,200).Bounds));
    TestTrue(TEXT("Remove"),Tree.RemoveProxy({3}));Tree.GeneratePairs(Pairs);TestEqual(TEXT("Removed pairs gone"),Pairs.Num(),0);
    TestFalse(TEXT("Double remove"),Tree.RemoveProxy({3}));TestFalse(TEXT("Invalid remove"),Tree.RemoveProxy({}));
    TestFalse(TEXT("Retired ID cannot alias a new body"),Tree.RegisterProxy(Proxy(3,0)));
    TestFalse(TEXT("Update removed body"),Tree.UpdateProxy({3},A));
    TestFalse(TEXT("Invalid update"),Tree.UpdateProxy({1},{FVector(-1),FVector(Inf)}));
    TestTrue(TEXT("Invalid updates do not corrupt tree"),Tree.Validate());
    Tree.RemoveProxy({1});Tree.RemoveProxy({2});TestTrue(TEXT("Remove entire tree"),Tree.Num()==0 && Tree.Validate());
    TestTrue(TEXT("Reuse free nodes with NEW ID"),Tree.RegisterProxy(Proxy(4,0)) && Tree.Validate());

    FKartPhysicsBroadphase RotationTree;RotationTree.SetFatMargin(.1);
    FKartPhysicsProxy Rot=Proxy(10,0);
    FKartPhysicsAABB::FromOrientedBox(FVector::ZeroVector,FQuat::Identity,FVector(1,5,1),Rot.Bounds);
    RotationTree.RegisterProxy(Rot);
    FKartPhysicsAABB::FromOrientedBox(FVector::ZeroVector,FQuat(FVector(0,0,1),PI/2),FVector(1,5,1),Rot.Bounds);
    RotationTree.UpdateProxy(Rot.Id,Rot.Bounds);
    TestTrue(TEXT("Rotating box reinserts and tree remains valid"),RotationTree.GetDiagnostics().Reinserts==1 && RotationTree.Validate());

    // Two independent trees, opposite insertion order, same IDs/updates. Oracle is O(n^2) only in this test.
    FRandomStream Random(240217);TArray<FKartPhysicsProxy> Proxies;
    FKartPhysicsBroadphase Forward,Reverse;
    auto Coordinate=[&](){return double(Random.FRandRange(-800,800));};
    for(int32 I=0;I<500;++I)
    {
        auto P=Proxy(1000+I,0);const FVector C(Coordinate(),Coordinate(),Coordinate());
        const FVector E(Random.FRandRange(10,90),Random.FRandRange(10,90),Random.FRandRange(10,90));
        P.Bounds={C-E,C+E};P.Mobility=I%4==0?EKartProxyMobility::Static:EKartProxyMobility::Dynamic;
        P.Category=1u<<(I%4);P.Mask=I%3==0?3u:~0u;P.bEnabled=I%11!=0;
        Proxies.Add(P);TestTrue(TEXT("Seeded proxy insertion"),Forward.RegisterProxy(P));
    }
    for(int32 I=Proxies.Num()-1;I>=0;--I)Reverse.RegisterProxy(Proxies[I]);
    int32 Comparisons=0;uint64 Visits=0;
    for(int32 Step=0;Step<13;++Step)
    {
        if(Step>0)
        {
            for(int32 I=0;I<Proxies.Num();++I)
            {
                auto& P=Proxies[I];const FVector Move(Coordinate()*.025,Coordinate()*.025,Coordinate()*.025);
                P.Bounds.Min+=Move;P.Bounds.Max+=Move;
                TestTrue(TEXT("Stress forward update"),Forward.UpdateProxy(P.Id,P.Bounds));
                TestTrue(TEXT("Stress reverse update"),Reverse.UpdateProxy(P.Id,P.Bounds));
            }
            if(Step%3==0)
            {
                const auto Removed=Proxies.Last().Id;
                TestTrue(TEXT("Stress remove both trees"),Forward.RemoveProxy(Removed) && Reverse.RemoveProxy(Removed));
                Proxies.Pop(EAllowShrinking::No);
                auto New=Proxy(2000+Step,Step*10);Proxies.Add(New);
                TestTrue(TEXT("Stress new ID reinsertion"),Forward.RegisterProxy(New) && Reverse.RegisterProxy(New));
            }
        }
        TArray<FKartPhysicsPair> F,R,Again;
        Forward.GeneratePairs(F);Reverse.GeneratePairs(R);Forward.GeneratePairs(Again);
        const auto Oracle=Brute(Proxies);
        TestTrue(TEXT("500-body exact brute-force equivalence"),F==Oracle && R==Oracle);
        TestTrue(TEXT("Repeat/order independence"),F==Again && F==R && ValidPairs(F));
        TestTrue(TEXT("Stress tree topology valid"),Forward.Validate() && Reverse.Validate());
        Visits+=Forward.GetDiagnostics().PairNodeVisits;++Comparisons;
    }
    const auto D=Forward.GetDiagnostics();
    AddInfo(FString::Printf(TEXT("STAGE2_STRESS seed=240217 proxies=%d snapshots=%d updates=6000 removals/replacements=4 exact_oracle_checks=%d opposite_order=PASS repeat=PASS height=%d nodes=%d reinserts=%llu total_pair_node_visits=%llu final_pairs=%d"),
        Forward.Num(),Comparisons,Comparisons*2,D.Height,D.NodeCount,D.Reinserts,Visits,D.PairCount));

    // Explicit world storage is empty until called; a proxy has no actor/body ownership.
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Sub=World->GetSubsystem<UKartPhysicsWorldSubsystem>();
    if(!TestNotNull(TEXT("World shell"),Sub))return false;
    TestEqual(TEXT("No automatic registration"),Sub->GetExperimentalBroadphase().Num(),0);
    auto* Kart=World->SpawnActor<AKartPawn>(FVector(0,0,1000),FRotator::ZeroRotator);
    const auto Pose=Kart->GetActorTransform();
    auto Experimental=Proxy(Sub->AllocateExperimentalBodyId().Value,0);
    TestTrue(TEXT("Explicit dormant registration"),Sub->GetExperimentalBroadphase().RegisterProxy(Experimental));
    Sub->GetExperimentalBroadphase().GeneratePairs(Pairs);
    TestTrue(TEXT("No live movement or incidents"),Pose.Equals(Kart->GetActorTransform()) && Kart->GetCollisionCount()==0);
    TestFalse(TEXT("Custom still unavailable"),Kart->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));
    TestEqual(TEXT("Legacy remains active"),Kart->GetPhysicsBackend(),EKartPhysicsBackend::Legacy);
    Sub->GetExperimentalBroadphase().RemoveProxy(Experimental.Id);Kart->Destroy();
    return true;
}
#endif
