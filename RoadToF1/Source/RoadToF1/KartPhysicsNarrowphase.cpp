#include "KartPhysicsNarrowphase.h"

FKartContactGeometry KartNarrowphase::Query(const FKartConvexShape& A,const FKartConvexShape& B,const FKartNarrowphaseSettings& Settings)
{
    const auto G=GJK(A,B,Settings);
    if(G.Status==EKartGJKStatus::Intersecting)return EPA(A,B,G,Settings);
    FKartContactGeometry R;R.GJK=G.Status;R.GJKIterations=G.Iterations;return R;
}
void KartNarrowphase::Dispatch(const FKartPhysicsBroadphase& Tree,const TArray<FKartPhysicsPair>& Candidates,
    const TMap<FKartPhysicsBodyId,FKartConvexShape>& Shapes,TArray<FKartContactGeometry>& Out)
{
    Out.Reset();TArray<FKartPhysicsPair> Sorted=Candidates;Sorted.Sort();FKartPhysicsPair Last;
    for(const auto& Pair:Sorted)
    {
        if(!Pair.IsValid() || Pair==Last)continue;Last=Pair;
        FKartPhysicsProxy A,B;
        if(!Tree.GetProxy(Pair.First(),A) || !Tree.GetProxy(Pair.Second(),B) || !FKartPhysicsProxy::AllowsPair(A,B))continue;
        const auto* SA=Shapes.Find(Pair.First());const auto* SB=Shapes.Find(Pair.Second());FKartContactGeometry R;
        FKartPhysicsAABB BA,BB;
        // Snapshot contract: bounds must match registered tight bounds. Fail stale mappings explicitly.
        if(SA && SB && SA->Bounds(BA) && SB->Bounds(BB) && A.Bounds.Min.Equals(BA.Min,1.e-7)
            && A.Bounds.Max.Equals(BA.Max,1.e-7) && B.Bounds.Min.Equals(BB.Min,1.e-7) && B.Bounds.Max.Equals(BB.Max,1.e-7))R=Query(*SA,*SB);
        R.Pair=Pair;Out.Add(R);
    }
}
