#pragma once
#include "KartPhysicsProxy.h"
#include "KartPhysicsPair.h"

struct FKartBroadphaseDiagnostics
{
    int32 ProxyCount=0, NodeCount=0, Height=0, PairCount=0;
    uint64 Reinserts=0, PairNodeVisits=0;
};
struct FKartBroadphaseDebugNode
{
    FKartPhysicsAABB Bounds;
    FKartPhysicsBodyId Id; // Invalid for internal nodes.
};

/** Dormant dynamic AABB tree over caller-registered experimental proxies only.
 * No automatic scene import, physics tick or collision response. Game-thread/external
 * serialization required: query scratch is reused. No UObject per proxy.
 * Fat nodes accelerate queries; outputs are filtered against current tight AABBs,
 * so pair sets do not depend on insertion order/fat history. Not CCD/prediction.
 */
class ROADTOF1_API FKartPhysicsBroadphase
{
public:
    // Empty-tree configuration only. Modest bounded margin, cm. Default 5cm.
    bool SetFatMargin(double Cm);
    bool RegisterProxy(const FKartPhysicsProxy& Proxy);
    bool RemoveProxy(FKartPhysicsBodyId Id);
    bool UpdateProxy(FKartPhysicsBodyId Id, const FKartPhysicsAABB& Bounds);
    bool SetFilter(FKartPhysicsBodyId Id, EKartProxyMobility Mobility, uint32 Category, uint32 Mask, bool bEnabled);
    int32 Num() const { return Leaves.Num(); }
    bool GetProxy(FKartPhysicsBodyId Id, FKartPhysicsProxy& Out) const;
    bool GetFatBounds(FKartPhysicsBodyId Id, FKartPhysicsAABB& Out) const;
    // Output reset on each call; disabled proxies omitted. Sorted IDs. Optional fat-leaf inspection.
    bool QueryAABB(const FKartPhysicsAABB& Bounds, TArray<FKartPhysicsBodyId>& Out, bool bFatLeaves=false);
    void GeneratePairs(TArray<FKartPhysicsPair>& Out);
    FKartBroadphaseDiagnostics GetDiagnostics() const;
    void GetDebugNodes(TArray<FKartBroadphaseDebugNode>& Out) const;
    bool Validate() const; // Explicit development/test invariant check, not per-frame work.
private:
    struct FNode
    {
        FKartPhysicsAABB Bounds;
        FKartPhysicsProxy Proxy;
        int32 Parent=INDEX_NONE, Left=INDEX_NONE, Right=INDEX_NONE, Height=0;
        bool bUsed=false;
        bool IsLeaf() const { return Left==INDEX_NONE; }
    };
    TArray<FNode> Nodes;
    TArray<int32> FreeNodes, Stack;
    TArray<FKartPhysicsBodyId> SortedIds, QueryScratch;
    TMap<FKartPhysicsBodyId,int32> Leaves;
    TSet<FKartPhysicsBodyId> RetiredIds; // Never allow a stale removed handle to identify a new proxy.
    int32 Root=INDEX_NONE;
    double FatMargin=5;
    uint64 Reinserts=0, QueryVisits=0, PairVisits=0;
    int32 LastPairCount=0;
    int32 AllocateNode();
    void ReleaseNode(int32 Index);
    void InsertLeaf(int32 Leaf);
    void DetachLeaf(int32 Leaf);
    void Refit(int32 Index);
};
