#include "KartPhysicsManifoldCache.h"

namespace KartManifoldCachePrivate
{
bool SameGeometry(const FKartConvexShape& A,const FKartConvexShape& B)
{return A.Kind==B.Kind && A.HalfExtents==B.HalfExtents && A.Radius==B.Radius && A.Vertices==B.Vertices;}
bool SameFilter(const FKartPhysicsProxy& A,const FKartPhysicsProxy& B)
{return A.Mobility==B.Mobility && A.Category==B.Category && A.Mask==B.Mask && A.bEnabled==B.bEnabled;}
bool SnapshotMatches(const FKartConvexShape& S,const FKartPhysicsProxy& P)
{FKartPhysicsAABB B;return S.Bounds(B) && B.Min.Equals(P.Bounds.Min,1.e-7) && B.Max.Equals(P.Bounds.Max,1.e-7);}
}
bool FKartPhysicsManifoldCache::Update(const FKartPhysicsBroadphase& Tree,const TArray<FKartPhysicsPair>& Candidates,
    const TMap<FKartPhysicsBodyId,FKartConvexShape>& Shapes,const FKartManifoldSettings& S,const TMap<FKartPhysicsBodyId,uint64>* Revisions)
{
    using namespace KartManifoldCachePrivate;
    if(!S.IsValid())return false; // Invalid settings leave prior cache intact; caller must handle false.
    TArray<FKartPhysicsPair> Sorted=Candidates;Sorted.Sort();Results.Reset();Diagnostics={};
    int32 OldPoints=0;for(const auto& E:Entries)OldPoints+=E.Value.Manifold.Points.Num(); // Count only, no ordering dependency.
    TMap<FKartPhysicsPair,FEntry> Next;FKartPhysicsPair Last;
    for(const auto Pair:Sorted)
    {
        if(!Pair.IsValid() || Pair==Last)continue;Last=Pair;FKartPhysicsProxy PA,PB;
        if(!Tree.GetProxy(Pair.First(),PA) || !Tree.GetProxy(Pair.Second(),PB) || !FKartPhysicsProxy::AllowsPair(PA,PB))continue;
        const auto* A=Shapes.Find(Pair.First());const auto* B=Shapes.Find(Pair.Second());
        if(!A || !B || !SnapshotMatches(*A,PA) || !SnapshotMatches(*B,PB))
        {FKartContactManifold Bad;Bad.Pair=Pair;Bad.Status=EKartManifoldStatus::InvalidInput;Results.Add(Bad);++Diagnostics.Failures;continue;}
        const uint64 RA=Revisions?Revisions->FindRef(Pair.First()):0,RB=Revisions?Revisions->FindRef(Pair.Second()):0;
        const FEntry* Old=Entries.Find(Pair);
        if(Old && (!SameGeometry(Old->A,*A) || !SameGeometry(Old->B,*B) || !SameFilter(Old->ProxyA,PA) || !SameFilter(Old->ProxyB,PB)
            || RA!=Old->RevisionA || RB!=Old->RevisionB))Old=nullptr;
        auto M=KartManifolds::Build(Pair,*A,*B,S,Old?&Old->Manifold:nullptr);
        if(M.Status==EKartManifoldStatus::Valid)
        {
            bool Used[4]={};bool MatchedAny=false;
            const FVector OldNormal=Old?A->Orientation.RotateVector(Old->A.Orientation.UnrotateVector(Old->Manifold.Normal)):FVector::ZeroVector;
            for(auto& P:M.Points)
            {
                int32 Best=INDEX_NONE;double BestScore=TNumericLimits<double>::Max();
                if(Old && FVector::DotProduct(OldNormal,M.Normal)>=S.NormalMatchCos)
                for(int32 I=0;I<Old->Manifold.Points.Num();++I)
                {
                    if(Used[I])continue;const auto& Q=Old->Manifold.Points[I];
                    const FVector Delta=KartManifolds::ToWorld(*A,Q.LocalA)-KartManifolds::ToWorld(*B,Q.LocalB);
                    const double Depth=FVector::DotProduct(Delta,M.Normal);
                    if(Depth < -S.SeparationCm || (Delta-M.Normal*Depth).Size()>S.AnchorDriftCm)continue;
                    const bool Feature=P.Feature!=0 && P.Feature==Q.Feature;
                    const double D=FMath::Max((P.LocalA-Q.LocalA).Size(),(P.LocalB-Q.LocalB).Size());
                    if(D>(Feature?S.FeatureMatchCm:S.AnchorDriftCm))continue;
                    const double Score=D+(Feature?0:100); // Feature match first, then deterministic local distance.
                    if(Score<BestScore){Best=I;BestScore=Score;}
                }
                if(Best!=INDEX_NONE)
                {
                    const auto& Q=Old->Manifold.Points[Best];Used[Best]=true;P.PersistentId=Q.PersistentId;
                    P.Age=Q.Age==MAX_uint32?MAX_uint32:Q.Age+1;++Diagnostics.Reused;MatchedAny=true;
                }
                else {P.PersistentId=NextId++;P.Age=0;++Diagnostics.New;}
            }
            M.Age=MatchedAny?(Old->Manifold.Age==MAX_uint32?MAX_uint32:Old->Manifold.Age+1):0;
            FEntry E;E.Manifold=M;E.A=*A;E.B=*B;E.ProxyA=PA;E.ProxyB=PB;E.RevisionA=RA;E.RevisionB=RB;Next.Add(Pair,MoveTemp(E));
            ++Diagnostics.Active;Diagnostics.MaxPoints=FMath::Max(Diagnostics.MaxPoints,M.Points.Num());
        }
        else if(M.Status!=EKartManifoldStatus::NoContact)++Diagnostics.Failures;
        Results.Add(M);
    }
    Diagnostics.Invalidated=OldPoints-Diagnostics.Reused;Entries=MoveTemp(Next);return true;
}
