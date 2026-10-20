#pragma once
#include "KartRigidBodyState.h"

/** Direct-call isolated integrator, NOT a world scheduler or gameplay backend.
 * Simplification: alpha=I_world^-1*tau; gyroscopic omega cross (I*omega) is omitted.
 * No automatic gravity, damping, collisions, speed caps or vehicle forces.
 */
class ROADTOF1_API FKartPhysicsIntegrator
{
public:
    static constexpr double MaxStepSeconds = 0.1;
    // Accepts only finite 0 < dt <= 0.1s. Failure/overflow leaves ALL state unchanged.
    // Success clears accumulators, including for immovable bodies.
    static bool Step(FKartRigidBodyState& Body, double Dt);
};
