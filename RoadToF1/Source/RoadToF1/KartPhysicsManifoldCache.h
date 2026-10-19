#pragma once
#include "KartPhysicsManifoldBuilder.h"

struct FKartManifoldDiagnostics
{
    int32 Active=0,Reused=0,New=0,Invalidated=0,Failures=0,MaxPoints=0;
};
/** Explicit complete-snapshot update only. Missing candidates expire immediately.
 * One cache per world/session; Reset on world reset. Stage2 retired IDs cannot recycle.
 * Geometry revisions optionally invalidate even identical replacement geometry.
 * No mutation of shapes, bodies, tree, actors; only this cache changes. */
class ROADTOF1_API FKartPhysicsManifoldCache
{
public:
    bool Update(const FKartPhysicsBroadphase& Tree,const TArray<FKartPhysicsPair>& Candidates,
        const TMap<FKartPhysicsBodyId,FKartConvexShape>& Shapes,const FKartManifoldSettings& Settings={},
        const TMap<FKartPhysicsBodyId,uint64>* GeometryRevisions=nullptr);
    void Reset(){Entries.Reset();Results.Reset();Diagnostics={};NextId=1;}
    const TArray<FKartContactManifold>& GetResults() const {return Results;}
    const FKartManifoldDiagnostics& GetDiagnostics() const {return Diagnostics;}
private:
    struct FEntry
    {
        FKartContactManifold Manifold;
        FKartConvexShape A,B;
        FKartPhysicsProxy ProxyA,ProxyB;
        uint64 RevisionA=0,RevisionB=0;
    };
    TMap<FKartPhysicsPair,FEntry> Entries;
    TArray<FKartContactManifold> Results;
    FKartManifoldDiagnostics Diagnostics;
    uint64 NextId=1;
};
