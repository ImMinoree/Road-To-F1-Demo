#pragma once
#include "CoreMinimal.h"

/** Column-major 3x3 linear map; no translation/homogeneous component. */
struct ROADTOF1_API FKartTensor3
{
    FVector X = FVector::ZeroVector, Y = FVector::ZeroVector, Z = FVector::ZeroVector;
    static FKartTensor3 Diagonal(const FVector& D) { return {FVector(D.X,0,0), FVector(0,D.Y,0), FVector(0,0,D.Z)}; }
    FVector TransformVector(const FVector& V) const { return X*V.X + Y*V.Y + Z*V.Z; }
    bool IsFinite() const { return !X.ContainsNaN() && !Y.ContainsNaN() && !Z.ContainsNaN(); }
};

/** Isolated Stage 1 value type. Never attached to, or advanced by, a live kart.
 * Units: cm, s, kg; velocity cm/s; angular velocity rad/s (world axes);
 * force kg*cm/s^2 (100 units/N); torque kg*cm^2/s^2 (10000 units/Nm);
 * inertia kg*cm^2. Position IS world centre of mass; orientation maps local to world.
 * Mass model: one uniform box with principal axes aligned to local XYZ, full dimensions.
 * All fallible operations return false without changing state, including accumulators.
 */
class ROADTOF1_API FKartRigidBodyState
{
public:
    bool ConfigureBox(double MassKg, const FVector& FullSizeCm);
    bool SetPose(const FVector& PositionCm, const FQuat& LocalToWorld);
    bool SetVelocities(const FVector& LinearCmPerSecond, const FVector& AngularRadiansPerSecond);
    // Explicit immovable mode, never represented by invalid/zero mass. Stops motion and clears forces.
    void SetImmovable(bool bValue);
    bool AddForce(const FVector& WorldForce);
    bool AddTorque(const FVector& WorldTorque);
    bool AddForceAtPoint(const FVector& WorldForce, const FVector& WorldPointCm);
    void ClearAccumulators() { Force = Torque = FVector::ZeroVector; }
    bool IsValid() const;

    const FVector& GetPosition() const { return Position; }
    const FQuat& GetOrientation() const { return Orientation; }
    const FVector& GetLinearVelocity() const { return LinearVelocity; }
    const FVector& GetAngularVelocity() const { return AngularVelocity; }
    const FVector& GetForce() const { return Force; }
    const FVector& GetTorque() const { return Torque; }
    double GetMass() const { return Mass; }
    double GetInverseMass() const { return InverseMass; }
    bool IsImmovable() const { return bImmovable; }
    const FKartTensor3& GetLocalInertia() const { return LocalInertia; }
    const FKartTensor3& GetLocalInverseInertia() const { return LocalInverseInertia; }
    FKartTensor3 GetWorldInverseInertia() const;

private:
    friend class FKartPhysicsIntegrator;
    FVector Position = FVector::ZeroVector;
    FQuat Orientation = FQuat::Identity;
    FVector LinearVelocity = FVector::ZeroVector, AngularVelocity = FVector::ZeroVector;
    FVector Force = FVector::ZeroVector, Torque = FVector::ZeroVector;
    double Mass = 1, InverseMass = 1;
    // Valid default: 1 kg, 1 cm cube. Not a production kart calibration.
    FKartTensor3 LocalInertia = FKartTensor3::Diagonal(FVector(1.0/6.0));
    FKartTensor3 LocalInverseInertia = FKartTensor3::Diagonal(FVector(6));
    bool bImmovable = false;
};
