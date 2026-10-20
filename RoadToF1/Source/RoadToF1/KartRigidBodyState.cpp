#include "KartRigidBodyState.h"

bool FKartRigidBodyState::ConfigureBox(double MassKg, const FVector& Size)
{
    if (!FMath::IsFinite(MassKg) || MassKg <= 0 || Size.ContainsNaN() || Size.GetMin() <= 0) return false;
    const double InvMass = 1.0/MassKg;
    const FVector S2 = Size*Size;
    const FVector D = (MassKg/12.0)*FVector(S2.Y+S2.Z, S2.X+S2.Z, S2.X+S2.Y);
    if (!FMath::IsFinite(InvMass) || InvMass <= 0 || D.ContainsNaN() || D.GetMin() <= 0) return false;
    const FVector InvD(1.0/D.X, 1.0/D.Y, 1.0/D.Z);
    if (InvD.ContainsNaN() || InvD.GetMin() <= 0) return false;
    Mass = MassKg;
    InverseMass = bImmovable ? 0 : InvMass;
    LocalInertia = FKartTensor3::Diagonal(D);
    LocalInverseInertia = FKartTensor3::Diagonal(bImmovable ? FVector::ZeroVector : InvD);
    return true;
}

bool FKartRigidBodyState::SetPose(const FVector& P, const FQuat& Q)
{
    const double Norm2 = Q.SizeSquared();
    if (P.ContainsNaN() || Q.ContainsNaN() || !FMath::IsFinite(Norm2) || Norm2 < 1.e-12) return false;
    FQuat Normalized = Q;
    Normalized.Normalize();
    if (Normalized.ContainsNaN() || !Normalized.IsNormalized()) return false;
    Position = P; Orientation = Normalized;
    return true;
}

bool FKartRigidBodyState::SetVelocities(const FVector& V, const FVector& W)
{
    if (V.ContainsNaN() || W.ContainsNaN() || (bImmovable && (!V.IsZero() || !W.IsZero()))) return false;
    LinearVelocity = V; AngularVelocity = W;
    return true;
}

void FKartRigidBodyState::SetImmovable(bool bValue)
{
    bImmovable = bValue;
    InverseMass = bValue ? 0 : 1.0/Mass;
    LocalInverseInertia = FKartTensor3::Diagonal(bValue ? FVector::ZeroVector :
        FVector(1.0/LocalInertia.X.X, 1.0/LocalInertia.Y.Y, 1.0/LocalInertia.Z.Z));
    if (bValue) LinearVelocity = AngularVelocity = FVector::ZeroVector;
    ClearAccumulators();
}

bool FKartRigidBodyState::AddForce(const FVector& F)
{
    const FVector Sum = Force+F;
    if (F.ContainsNaN() || Sum.ContainsNaN()) return false;
    Force = Sum; return true;
}

bool FKartRigidBodyState::AddTorque(const FVector& T)
{
    const FVector Sum = Torque+T;
    if (T.ContainsNaN() || Sum.ContainsNaN()) return false;
    Torque = Sum; return true;
}

bool FKartRigidBodyState::AddForceAtPoint(const FVector& F, const FVector& Point)
{
    if (F.ContainsNaN() || Point.ContainsNaN()) return false;
    const FVector NewForce = Force+F;
    const FVector NewTorque = Torque+FVector::CrossProduct(Point-Position, F);
    if (NewForce.ContainsNaN() || NewTorque.ContainsNaN()) return false;
    Force = NewForce; Torque = NewTorque; return true;
}

FKartTensor3 FKartRigidBodyState::GetWorldInverseInertia() const
{
    // R * I_local^-1 * R^T, evaluated by columns to avoid FMatrix row/column ambiguity.
    auto Apply = [this](const FVector& V)
    { return Orientation.RotateVector(LocalInverseInertia.TransformVector(Orientation.UnrotateVector(V))); };
    return {Apply(FVector(1,0,0)), Apply(FVector(0,1,0)), Apply(FVector(0,0,1))};
}

bool FKartRigidBodyState::IsValid() const
{
    return !Position.ContainsNaN() && !Orientation.ContainsNaN() && Orientation.IsNormalized()
        && !LinearVelocity.ContainsNaN() && !AngularVelocity.ContainsNaN()
        && !Force.ContainsNaN() && !Torque.ContainsNaN()
        && FMath::IsFinite(Mass) && Mass > 0 && FMath::IsFinite(InverseMass)
        && (bImmovable ? InverseMass == 0 : InverseMass > 0)
        && LocalInertia.IsFinite() && LocalInverseInertia.IsFinite() && GetWorldInverseInertia().IsFinite();
}
