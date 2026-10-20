#pragma once
#include "KartPhysicsManifoldBuilder.h"
#include "UObject/WeakObjectPtr.h"
class UWorld;
class UPrimitiveComponent;
class UBodySetup;

struct FKartStaticTriangle
{
    FVector V[3];
    FKartPhysicsAABB Bounds;
    int32 Source=INDEX_NONE;
};
struct FKartStaticGeometrySource
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    TWeakObjectPtr<UBodySetup> Geometry;
    FGuid GeometryRevision;
    FTransform Transform;
    FString Path;
    int32 Triangles=0,SkippedDegenerate=0;
    bool bBox=false;
    FVector BoxExtent=FVector::ZeroVector;
};
/** Immutable snapshot of actual cooked triangle collision, not render bounds.
 * Unreal owns cooking/scene discovery. These values only feed custom contacts.
 * Rebuild after source transforms/geometry change. No Chaos simulation/response. */
class ROADTOF1_API FKartStaticMeshGeometry
{
public:
    bool Build(const UWorld& World,FString& Failure);
    bool IsCurrent() const;
    void Query(const FKartPhysicsAABB& Bounds,TArray<int32>& Out) const;
    const TArray<FKartStaticTriangle>& GetTriangles() const {return Triangles;}
    const TArray<FKartStaticGeometrySource>& GetSources() const {return Sources;}
    int32 GetHeight() const {return Height;}
    // Two-sided finite triangles, exact OBB SAT and clipped geometric witnesses.
    static double Separation(const FKartConvexShape& Box,const FKartStaticTriangle& T,FVector& Normal);
    static FKartContactManifold Contact(FKartPhysicsPair Pair,const FKartConvexShape& Box,const FKartStaticTriangle& T);
private:
    struct FNode {FKartPhysicsAABB Bounds;int32 Left=INDEX_NONE,Right=INDEX_NONE,Start=0,Count=0;};
    TArray<FKartStaticTriangle> Triangles;
    TArray<FKartStaticGeometrySource> Sources;
    TArray<int32> Order;
    TArray<FNode> Nodes;
    int32 Height=0;
    int32 BuildNode(int32 Start,int32 Count,int32 Depth);
};
