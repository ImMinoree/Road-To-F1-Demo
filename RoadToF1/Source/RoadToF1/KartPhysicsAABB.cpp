#include "KartPhysicsAABB.h"
#include "KartRigidBodyState.h"

bool FKartPhysicsAABB::IsValid() const
{
    return !Min.ContainsNaN() && !Max.ContainsNaN() && Min.GetAbsMax() <= 1.e9 && Max.GetAbsMax() <= 1.e9
        && Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}
double FKartPhysicsAABB::SurfaceArea() const
{
    if (!IsValid()) return 0;
    const FVector D=Max-Min;
    return 2*(D.X*D.Y+D.Y*D.Z+D.Z*D.X);
}
bool FKartPhysicsAABB::Overlaps(const FKartPhysicsAABB& B) const
{
    return IsValid() && B.IsValid() && Min.X<=B.Max.X && Max.X>=B.Min.X
        && Min.Y<=B.Max.Y && Max.Y>=B.Min.Y && Min.Z<=B.Max.Z && Max.Z>=B.Min.Z;
}
bool FKartPhysicsAABB::Contains(const FKartPhysicsAABB& B) const
{
    return IsValid() && B.IsValid() && Min.X<=B.Min.X && Min.Y<=B.Min.Y && Min.Z<=B.Min.Z
        && Max.X>=B.Max.X && Max.Y>=B.Max.Y && Max.Z>=B.Max.Z;
}
bool FKartPhysicsAABB::Expanded(double Margin, FKartPhysicsAABB& Out) const
{
    if (!IsValid() || !FMath::IsFinite(Margin) || Margin<0) return false;
    const FKartPhysicsAABB Result{Min-FVector(Margin),Max+FVector(Margin)};
    if (!Result.IsValid()) return false;
    Out=Result; return true;
}
FKartPhysicsAABB FKartPhysicsAABB::Combine(const FKartPhysicsAABB& A, const FKartPhysicsAABB& B)
{
    if (!A.IsValid() || !B.IsValid()) return {FVector(1),FVector(-1)};
    return {A.Min.ComponentMin(B.Min),A.Max.ComponentMax(B.Max)};
}
bool FKartPhysicsAABB::FromOrientedBox(const FVector& P, const FQuat& Q, const FVector& E, FKartPhysicsAABB& Out)
{
    if (P.ContainsNaN() || Q.ContainsNaN() || !Q.IsNormalized() || E.ContainsNaN() || E.GetMin()<0) return false;
    const FVector WorldE=Q.RotateVector(FVector(1,0,0)).GetAbs()*E.X
        +Q.RotateVector(FVector(0,1,0)).GetAbs()*E.Y+Q.RotateVector(FVector(0,0,1)).GetAbs()*E.Z;
    const FKartPhysicsAABB Result{P-WorldE,P+WorldE};
    if (!Result.IsValid()) return false;
    Out=Result; return true;
}
bool FKartPhysicsAABB::FromBody(const FKartRigidBodyState& Body, const FVector& E, FKartPhysicsAABB& Out)
{
    return Body.IsValid() && FromOrientedBox(Body.GetPosition(),Body.GetOrientation(),E,Out);
}
