#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KartCollisionProvider.h"
#include "KartPhysicsBroadphase.h"
#include "KartPhysicsWorldSubsystem.generated.h"
class AKartPawn;

/** Option B ownership boundary (future custom backend):
 * Custom owns position/orientation/velocity/forces/impulses and final transform.
 * Unreal provides geometry queries only. Custom karts must not simulate in Unreal.
 * Never combine custom response with legacy swept movement/impact rotation.
 * This shared coordinator will solve each canonical kart pair once per step.
 * Incident consumers may display/count events, not add physical impulses/rotations.
 * Stage 2 adds dormant experimental proxy storage only, explicitly populated by callers.
 * No simulation tick, live body registration or solver. Custom activation is rejected;
 * existing Legacy movement and incidents remain entirely unchanged.
 */
UCLASS()
class ROADTOF1_API UKartPhysicsWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    const IKartCollisionProvider& GetCollisionProvider() const { return Provider; }
    bool ProbeKart(const AKartPawn& Kart, float Distance, bool bDraw, FHitResult& Hit) const;
    FKartPhysicsBodyId AllocateExperimentalBodyId() { return ExperimentalIds.Allocate(); }
    FKartPhysicsBroadphase& GetExperimentalBroadphase() { return ExperimentalBroadphase; }
    const FKartPhysicsBroadphase& GetExperimentalBroadphase() const { return ExperimentalBroadphase; }
private:
    FUnrealKartCollisionProvider Provider;
    FKartPhysicsBodyIdSource ExperimentalIds;
    FKartPhysicsBroadphase ExperimentalBroadphase;
};
