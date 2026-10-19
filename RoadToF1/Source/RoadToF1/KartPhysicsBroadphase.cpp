#include "KartPhysicsBroadphase.h"

bool FKartPhysicsBroadphase::SetFatMargin(double Cm)
{
    if (Num()!=0 || !FMath::IsFinite(Cm) || Cm<0 || Cm>100) return false;
    FatMargin=Cm; return true;
}
int32 FKartPhysicsBroadphase::AllocateNode()
{
    const int32 I=FreeNodes.IsEmpty()?Nodes.AddDefaulted():FreeNodes.Pop(EAllowShrinking::No);
    Nodes[I]=FNode(); Nodes[I].bUsed=true; return I;
}
void FKartPhysicsBroadphase::ReleaseNode(int32 I)
{
    Nodes[I]=FNode(); FreeNodes.Add(I);
}
void FKartPhysicsBroadphase::Refit(int32 I)
{
    while(I!=INDEX_NONE)
    {
        FNode& N=Nodes[I];
        if(!N.IsLeaf())
        {
            N.Bounds=FKartPhysicsAABB::Combine(Nodes[N.Left].Bounds,Nodes[N.Right].Bounds);
            N.Height=1+FMath::Max(Nodes[N.Left].Height,Nodes[N.Right].Height);
        }
        I=N.Parent;
    }
}
void FKartPhysicsBroadphase::InsertLeaf(int32 Leaf)
{
    if(Root==INDEX_NONE){Root=Leaf;return;}
    int32 Sibling=Root;
    // Greedy surface-area increase. Deterministic tie-break; balancing is deferred.
    while(!Nodes[Sibling].IsLeaf())
    {
        const int32 L=Nodes[Sibling].Left,R=Nodes[Sibling].Right;
        const double LC=FKartPhysicsAABB::Combine(Nodes[L].Bounds,Nodes[Leaf].Bounds).SurfaceArea()-Nodes[L].Bounds.SurfaceArea();
        const double RC=FKartPhysicsAABB::Combine(Nodes[R].Bounds,Nodes[Leaf].Bounds).SurfaceArea()-Nodes[R].Bounds.SurfaceArea();
        Sibling=LC<=RC?L:R;
    }
    const int32 OldParent=Nodes[Sibling].Parent;
    const int32 Parent=AllocateNode(); // May reallocate Nodes; retain indices only.
    Nodes[Parent].Parent=OldParent; Nodes[Parent].Left=Sibling; Nodes[Parent].Right=Leaf;
    Nodes[Sibling].Parent=Nodes[Leaf].Parent=Parent;
    if(OldParent==INDEX_NONE) Root=Parent;
    else if(Nodes[OldParent].Left==Sibling) Nodes[OldParent].Left=Parent;
    else Nodes[OldParent].Right=Parent;
    Refit(Parent);
}
void FKartPhysicsBroadphase::DetachLeaf(int32 Leaf)
{
    if(Leaf==Root){Root=INDEX_NONE;Nodes[Leaf].Parent=INDEX_NONE;return;}
    const int32 Parent=Nodes[Leaf].Parent, Grand=Nodes[Parent].Parent;
    const int32 Sibling=Nodes[Parent].Left==Leaf?Nodes[Parent].Right:Nodes[Parent].Left;
    if(Grand==INDEX_NONE) Root=Sibling;
    else if(Nodes[Grand].Left==Parent) Nodes[Grand].Left=Sibling;
    else Nodes[Grand].Right=Sibling;
    Nodes[Sibling].Parent=Grand; Nodes[Leaf].Parent=INDEX_NONE;
    ReleaseNode(Parent); Refit(Grand);
}
bool FKartPhysicsBroadphase::RegisterProxy(const FKartPhysicsProxy& P)
{
    FKartPhysicsAABB Fat;
    if(!P.IsValid() || Leaves.Contains(P.Id) || RetiredIds.Contains(P.Id) || !P.Bounds.Expanded(FatMargin,Fat)) return false;
    const int32 Leaf=AllocateNode(); Nodes[Leaf].Proxy=P; Nodes[Leaf].Bounds=Fat;
    Leaves.Add(P.Id,Leaf); SortedIds.Add(P.Id); SortedIds.Sort(); InsertLeaf(Leaf);
    LastPairCount=0; return true;
}
bool FKartPhysicsBroadphase::RemoveProxy(FKartPhysicsBodyId Id)
{
    const int32* Found=Leaves.Find(Id); if(!Found)return false;
    const int32 Leaf=*Found;
    DetachLeaf(Leaf); ReleaseNode(Leaf); Leaves.Remove(Id); SortedIds.Remove(Id); RetiredIds.Add(Id);
    LastPairCount=0; return true;
}
bool FKartPhysicsBroadphase::UpdateProxy(FKartPhysicsBodyId Id,const FKartPhysicsAABB& Bounds)
{
    const int32* Found=Leaves.Find(Id); FKartPhysicsAABB Fat;
    if(!Found || !Bounds.Expanded(FatMargin,Fat)) return false;
    const int32 Leaf=*Found;
    if(!Nodes[Leaf].Bounds.Contains(Bounds))
    {
        DetachLeaf(Leaf); Nodes[Leaf].Bounds=Fat; InsertLeaf(Leaf); ++Reinserts;
    }
    Nodes[Leaf].Proxy.Bounds=Bounds; LastPairCount=0; return true;
}
bool FKartPhysicsBroadphase::SetFilter(FKartPhysicsBodyId Id, EKartProxyMobility Mobility,uint32 Category,uint32 Mask,bool bEnabled)
{
    const int32* Found=Leaves.Find(Id);
    if(!Found || (Mobility!=EKartProxyMobility::Dynamic && Mobility!=EKartProxyMobility::Static))return false;
    auto& P=Nodes[*Found].Proxy; P.Mobility=Mobility; P.Category=Category; P.Mask=Mask; P.bEnabled=bEnabled;
    LastPairCount=0; return true;
}
bool FKartPhysicsBroadphase::GetProxy(FKartPhysicsBodyId Id,FKartPhysicsProxy& Out) const
{
    const int32* Found=Leaves.Find(Id); if(!Found)return false;
    Out=Nodes[*Found].Proxy; return true;
}
bool FKartPhysicsBroadphase::GetFatBounds(FKartPhysicsBodyId Id,FKartPhysicsAABB& Out) const
{
    const int32* Found=Leaves.Find(Id); if(!Found)return false;
    Out=Nodes[*Found].Bounds; return true;
}
bool FKartPhysicsBroadphase::QueryAABB(const FKartPhysicsAABB& Bounds,TArray<FKartPhysicsBodyId>& Out,bool bFatLeaves)
{
    Out.Reset(); Stack.Reset(); QueryVisits=0;
    if(!Bounds.IsValid())return false;
    if(Root!=INDEX_NONE)Stack.Add(Root);
    while(!Stack.IsEmpty())
    {
        const FNode& N=Nodes[Stack.Pop(EAllowShrinking::No)]; ++QueryVisits;
        if(!N.Bounds.Overlaps(Bounds))continue;
        if(N.IsLeaf())
        {
            if(N.Proxy.bEnabled && (bFatLeaves || N.Proxy.Bounds.Overlaps(Bounds)))Out.Add(N.Proxy.Id);
        }
        else {Stack.Add(N.Right);Stack.Add(N.Left);}
    }
    Out.Sort(); return true;
}
void FKartPhysicsBroadphase::GeneratePairs(TArray<FKartPhysicsPair>& Out)
{
    Out.Reset(); PairVisits=0;
    for(const auto Id:SortedIds)
    {
        const FKartPhysicsProxy& A=Nodes[Leaves.FindChecked(Id)].Proxy;
        if(!A.bEnabled)continue;
        QueryAABB(A.Bounds,QueryScratch); PairVisits+=QueryVisits;
        for(const auto Other:QueryScratch)
        {
            // Only the smaller ID emits a pair; every leaf is visited once per query.
            if(!(Id<Other))continue;
            const auto& B=Nodes[Leaves.FindChecked(Other)].Proxy;
            if(FKartPhysicsProxy::AllowsPair(A,B))Out.Emplace(Id,Other);
        }
    }
    Out.Sort(); LastPairCount=Out.Num();
}
FKartBroadphaseDiagnostics FKartPhysicsBroadphase::GetDiagnostics() const
{
    return {Num(),Nodes.Num()-FreeNodes.Num(),Root==INDEX_NONE?0:Nodes[Root].Height,LastPairCount,Reinserts,PairVisits};
}
void FKartPhysicsBroadphase::GetDebugNodes(TArray<FKartBroadphaseDebugNode>& Out) const
{
    Out.Reset();
    for(const auto& N:Nodes)if(N.bUsed)Out.Add({N.Bounds,N.IsLeaf()?N.Proxy.Id:FKartPhysicsBodyId{}});
}
bool FKartPhysicsBroadphase::Validate() const
{
    if(Root==INDEX_NONE)return Leaves.IsEmpty() && SortedIds.IsEmpty() && Nodes.Num()==FreeNodes.Num();
    if(!Nodes.IsValidIndex(Root) || Nodes[Root].Parent!=INDEX_NONE)return false;
    TSet<int32> Seen; TArray<int32> Pending{Root}; int32 LeafCount=0;
    while(!Pending.IsEmpty())
    {
        const int32 I=Pending.Pop(EAllowShrinking::No);
        if(!Nodes.IsValidIndex(I) || Seen.Contains(I))return false;
        Seen.Add(I); const auto& N=Nodes[I];
        if(!N.bUsed || !N.Bounds.IsValid())return false;
        if(N.IsLeaf())
        {
            const int32* Mapped=Leaves.Find(N.Proxy.Id);
            if(N.Right!=INDEX_NONE || N.Height!=0 || !N.Proxy.IsValid() || !Mapped || *Mapped!=I
                || RetiredIds.Contains(N.Proxy.Id) || !N.Bounds.Contains(N.Proxy.Bounds))return false;
            ++LeafCount;
        }
        else
        {
            if(!Nodes.IsValidIndex(N.Left) || !Nodes.IsValidIndex(N.Right))return false;
            const auto& L=Nodes[N.Left]; const auto& R=Nodes[N.Right];
            const auto Union=FKartPhysicsAABB::Combine(L.Bounds,R.Bounds);
            if(L.Parent!=I || R.Parent!=I || N.Bounds.Min!=Union.Min || N.Bounds.Max!=Union.Max
                || N.Height!=1+FMath::Max(L.Height,R.Height))return false;
            Pending.Add(N.Left);Pending.Add(N.Right);
        }
    }
    if(LeafCount!=Leaves.Num() || SortedIds.Num()!=LeafCount || Seen.Num()!=Nodes.Num()-FreeNodes.Num())return false;
    for(int32 I=0;I<SortedIds.Num();++I)
        if(!Leaves.Contains(SortedIds[I]) || (I>0 && !(SortedIds[I-1]<SortedIds[I])))return false;
    TSet<int32> FreeSeen;
    for(int32 I:FreeNodes)
    {
        if(!Nodes.IsValidIndex(I) || Nodes[I].bUsed || Seen.Contains(I) || FreeSeen.Contains(I))return false;
        FreeSeen.Add(I);
    }
    return true;
}
