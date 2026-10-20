#pragma once
#include "CoreMinimal.h"

/** Speed planning for the force-driven backend, not an extra physical speed cap.
 * The legacy controller assumes instantaneous bicycle yaw. Custom must reserve
 * tire grip and actual braking distance on the selected lateral route. Inputs
 * remain ordinary throttle/brake/steer; there are no hidden body forces. */
namespace KartPhysicsDriving
{
inline double SteeringTowardTarget(const FVector& LocalTarget,double Nominal)
{
    // Pure pursuit has zero curvature for a target exactly behind the chassis.
    // Reacquire the front hemisphere at the caller's low recovery speed using
    // the EXISTING rack limit. No additional yaw/force or global gain change.
    return LocalTarget.X<0 && LocalTarget.SizeSquared2D()>10000?(LocalTarget.Y>=0?1.:-1.):Nominal;
}
inline double SpeedEnvelope(const TArray<FVector>& Route,int Nearest,double Lane,double Maximum,double Skill=1)
{
    if(Route.Num()<4)return Maximum;
    auto Point=[&](int Index)
    {
        Index=(Index+Route.Num())%Route.Num();const auto Tangent=(Route[(Index+1)%Route.Num()]-Route[Index]).GetSafeNormal2D();
        return Route[Index]+FVector(-Tangent.Y,Tangent.X,0)*Lane;
    };
    double Limit=Maximum,Distance=0;const double Grip=350*FMath::Clamp(Skill,.9,1.1);
    for(int Step=0;Step<Route.Num()-2 && Distance<5000;++Step)
    {
        const auto A=Point(Nearest+Step-1),B=Point(Nearest+Step),C=Point(Nearest+Step+1);
        const auto U=B-A,V=C-B,W=C-A;
        const double Denominator=U.Size2D()*V.Size2D()*W.Size2D();
        if(Denominator>1)
        {
            const double Curvature=2*FMath::Abs(FVector::CrossProduct(U,V).Z)/Denominator;
            if(Curvature>1.e-7)
            {
                const double BendSquared=Grip/Curvature;
                //500cm/s^2 is below the tire-limited full brake response. Four
                // metres reserve control latency/rack settling; no pose edits.
                Limit=FMath::Min(Limit,FMath::Sqrt(BendSquared+2*500*FMath::Max(0.,Distance-400)));
            }
        }
        Distance+=V.Size2D();
    }
    return Limit;
}
}
