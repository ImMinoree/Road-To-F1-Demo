#pragma once
#include "CoreMinimal.h"
class FKartRigidBodyState;

/** cm; inclusive overlap (touching and zero-size boxes allowed). No contact geometry.
 * Supported coordinates +/-1e9cm keep union/area cost arithmetic bounded.
 */
struct ROADTOF1_API FKartPhysicsAABB
{
    FVector Min = FVector::ZeroVector, Max = FVector::ZeroVector;
    bool IsValid() const;
    FVector Center() const { return Min*.5+Max*.5; }
    FVector Extents() const { return Max*.5-Min*.5; }
    double SurfaceArea() const;
    bool Overlaps(const FKartPhysicsAABB& Other) const;
    bool Contains(const FKartPhysicsAABB& Other) const;
    bool Expanded(double MarginCm, FKartPhysicsAABB& Out) const;
    static FKartPhysicsAABB Combine(const FKartPhysicsAABB& A, const FKartPhysicsAABB& B);
    static bool FromOrientedBox(const FVector& Position, const FQuat& Orientation,
        const FVector& HalfExtents, FKartPhysicsAABB& Out);
    static bool FromBody(const FKartRigidBodyState& Body, const FVector& HalfExtents, FKartPhysicsAABB& Out);
};
