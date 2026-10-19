#pragma once
#include "KartPhysicsAABB.h"

enum class EKartConvexKind : uint8 { Box, Sphere, Hull };
/** Value geometry. Hull means convex closure of supplied vertices, not a render mesh.
 * Full-dimensional shapes only for penetration; lower-dimensional hulls are rejected.
 * No scale/shear. Quaternions must already be normalized. cm units. */
struct ROADTOF1_API FKartConvexShape
{
    EKartConvexKind Kind=EKartConvexKind::Box;
    FVector Position=FVector::ZeroVector, HalfExtents=FVector(1);
    FQuat Orientation=FQuat::Identity;
    double Radius=1;
    TArray<FVector> Vertices;
    static constexpr int32 MaxVertices=128;
    bool IsValid() const;
    double Scale() const;
    bool Bounds(FKartPhysicsAABB& Out) const;
    bool Support(const FVector& Direction,FVector& Out) const;
    // Internal hot path: caller has validated shape/direction once.
    FVector SupportOffset(const FVector& Direction) const;
};
