#pragma once
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
class UWorld;
class AActor;

/** Geometric query only: never moves bodies, applies impulses or triggers incidents.
 * Hit results are query observations, NOT a solver-ready manifold.
 * Initial-overlap penetration data may be incomplete; Stage 7 must validate it.
 */
class ROADTOF1_API IKartCollisionProvider
{
public:
    virtual ~IKartCollisionProvider() = default;
    virtual bool SweepBox(const UWorld& World, const FVector& Start, const FVector& End,
        const FQuat& Rotation, const FVector& HalfExtent, const AActor* Ignore, FHitResult& Hit) const = 0;
};

class ROADTOF1_API FUnrealKartCollisionProvider final : public IKartCollisionProvider
{
public:
    virtual bool SweepBox(const UWorld& World, const FVector& Start, const FVector& End,
        const FQuat& Rotation, const FVector& HalfExtent, const AActor* Ignore, FHitResult& Hit) const override;
};
