#pragma once
#include "CoreMinimal.h"
enum class EKartContactSurface : uint8 { Bodywork,Asphalt,Kerb,Grass,Barrier,LowGrip };
/** Dimensionless coefficients. Pair policy: geometric mean friction, maximum restitution.
 * These are prototype contact values, not measured tires. Default preserves frictionless Stage5. */
struct FKartContactMaterial
{
    double StaticFriction=0,DynamicFriction=0,Restitution=0;
    bool IsValid() const
    {return FMath::IsFinite(StaticFriction) && FMath::IsFinite(DynamicFriction) && FMath::IsFinite(Restitution)
        && StaticFriction>=0 && StaticFriction<=2 && DynamicFriction>=0 && DynamicFriction<=StaticFriction
        && Restitution>=0 && Restitution<=1;}
    static FKartContactMaterial Combine(const FKartContactMaterial& A,const FKartContactMaterial& B)
    {return {FMath::Sqrt(A.StaticFriction*B.StaticFriction),FMath::Sqrt(A.DynamicFriction*B.DynamicFriction),FMath::Max(A.Restitution,B.Restitution)};}
    static FKartContactMaterial Surface(EKartContactSurface S)
    {
        switch(S){case EKartContactSurface::Asphalt:return {.9,.75,.02};case EKartContactSurface::Kerb:return {.8,.65,.04};
        case EKartContactSurface::Grass:return {.5,.35,.01};case EKartContactSurface::Barrier:return {.5,.4,.08};
        case EKartContactSurface::LowGrip:return {.15,.1,0};default:return {.6,.45,.05};}
    }
};
