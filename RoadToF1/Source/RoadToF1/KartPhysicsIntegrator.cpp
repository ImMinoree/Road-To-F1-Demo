#include "KartPhysicsIntegrator.h"

bool FKartPhysicsIntegrator::Step(FKartRigidBodyState& Body, double Dt)
{
    if (!FMath::IsFinite(Dt) || Dt <= 0 || Dt > MaxStepSeconds || !Body.IsValid()) return false;
    FKartRigidBodyState Next = Body;
    if (!Next.bImmovable)
    {
        Next.LinearVelocity += (Body.Force*Body.InverseMass)*Dt;
        Next.Position += Next.LinearVelocity*Dt; // Semi-implicit: use UPDATED velocity.
        Next.AngularVelocity += Body.GetWorldInverseInertia().TransformVector(Body.Torque)*Dt;
        const double Rate = Next.AngularVelocity.Size();
        if (!FMath::IsFinite(Rate)) return false;
        if (Rate > 0)
        {
            // World-space angular velocity: left multiply local-to-world orientation.
            const FQuat Delta(Next.AngularVelocity/Rate, Rate*Dt);
            Next.Orientation = Delta*Body.Orientation;
            Next.Orientation.Normalize();
        }
    }
    Next.ClearAccumulators();
    if (!Next.IsValid()) return false;
    Body = Next;
    return true;
}
