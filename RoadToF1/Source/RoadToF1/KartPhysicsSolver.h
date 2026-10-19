#pragma once
#include "KartRigidBodyState.h"
#include "KartPhysicsContactManifold.h"

struct FKartSolverSettings
{
    int32 Iterations=24;
    double SlopCm=.02, BiasFraction=.15, MaxBiasCmPerSecond=20;
    double MaxImpulse=1.e8; // kg*cm/s; exceeding this fails atomically, never silently clips impacts.
    bool IsValid() const;
};
struct FKartSolvedContact
{
    FKartPhysicsPair Pair;
    uint64 PersistentId=0;
    FVector Point=FVector::ZeroVector, Normal=FVector::ZeroVector;
    double NormalImpulse=0, ResidualClosingSpeed=0;
};
struct FKartSolverResult
{
    bool bSuccess=false;
    FString Failure;
    int32 Iterations=0;
    TArray<FKartSolvedContact> Contacts;
};
/** Isolated velocity-only PGS. Stage4 Normal points A->B; positive impulse pushes A away.
 * No actor/query/incident access, transform writes, gravity, or integration. Caller owns fixed-step order:
 * integrate forces into velocities -> current geometry -> Solve -> integrate corrected velocities into poses.
 * Bounded Baumgarte velocity bias (not teleportation) can add small energy; zero BiasFraction disables it.
 * A failed batch leaves ALL input bodies untouched. Complete validated manifolds only, unique pairs.
 * No warm start yet: every batch starts at zero, preventing stale impulse reuse across cache resets.
 */
class ROADTOF1_API FKartPhysicsSolver
{
public:
    static FKartSolverResult Solve(TMap<FKartPhysicsBodyId,FKartRigidBodyState>& Bodies,
        const TArray<FKartContactManifold>& Manifolds,double Dt,const FKartSolverSettings& Settings={});
};
