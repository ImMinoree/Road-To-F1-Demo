#include "KartCollisionProvider.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

bool FUnrealKartCollisionProvider::SweepBox(const UWorld& World, const FVector& Start, const FVector& End,
    const FQuat& Rotation, const FVector& HalfExtent, const AActor* Ignore, FHitResult& Hit) const
{
    Hit = FHitResult();
    if (Start.ContainsNaN() || End.ContainsNaN() || Rotation.ContainsNaN() || !Rotation.IsNormalized()
        || HalfExtent.ContainsNaN() || HalfExtent.GetMin() <= 0) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(KartStage0Query), false, Ignore);
    Params.bFindInitialOverlaps = true;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    // Deliberately excludes dynamic props. This is not a replacement for legacy filtering.
    return World.SweepSingleByObjectType(Hit, Start, End, Rotation, Objects,
        FCollisionShape::MakeBox(HalfExtent), Params);
}
