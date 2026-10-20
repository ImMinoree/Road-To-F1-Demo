#include "KartPhysicsViewFeedback.h"
bool FKartPhysicsViewSettings::IsValid() const
{
    auto Range=[](double V,double A,double B){return FMath::IsFinite(V) && V>=A && V<=B;};
    return Range(Amount,0,1) && Range(BlendRate,1,20) && Range(ImpactDecay,1,20) && Range(MaxOffsetCm,0,3)
        && Range(MaxAngleDegrees,0,3) && Range(BaseFOV,80,100) && Range(ExtraFOV,0,8);
}
bool FKartPhysicsViewFeedback::Step(const FKartPhysicsViewInput& I,double Dt,const FKartPhysicsViewSettings& S)
{
    if(!S.IsValid() || !FMath::IsFinite(Dt) || Dt<=0 || Dt>.25 || I.LocalAcceleration.ContainsNaN()
        || I.LocalAngularVelocity.ContainsNaN() || I.LocalImpactImpulse.ContainsNaN() || !FMath::IsFinite(I.MassKg) || I.MassKg<1
        || !FMath::IsFinite(I.SpeedCmPerSecond) || !FMath::IsFinite(I.RoadSeverity) || !FMath::IsFinite(I.KerbSeverity)
        || !FMath::IsFinite(I.EngineActivity))return false;
    auto V=*this;const double Blend=1-FMath::Exp(-S.BlendRate*Dt);
    const FVector AG=I.LocalAcceleration.GetClampedToMaxSize(3920)/980;
    V.AccelBlend+=(AG-V.AccelBlend)*Blend;
    V.AngularBlend+=(I.LocalAngularVelocity.GetClampedToMaxSize(5)-V.AngularBlend)*Blend;
    V.Impact=(V.Impact+I.LocalImpactImpulse/I.MassKg/300.).GetClampedToMaxSize(1.5)*FMath::Exp(-S.ImpactDecay*Dt);
    V.SpeedBlend+=(FMath::Clamp(FMath::Abs(I.SpeedCmPerSecond)/1583.333333,0.,1.)-V.SpeedBlend)*Blend;
    V.Clock=FMath::Fmod(V.Clock+Dt,1000.);
    const double Vibration=.025*FMath::Clamp(I.EngineActivity,0.,1.)*FMath::Sin(V.Clock*87.9645943)
        +.06*FMath::Clamp(I.RoadSeverity,0.,1.)*FMath::Sin(V.Clock*119.380521)
        +.15*FMath::Clamp(I.KerbSeverity,0.,1.)*FMath::Sin(V.Clock*169.646003);
    V.Offset=(FVector(-V.AccelBlend.X*.25,-V.AccelBlend.Y*.3,-V.AccelBlend.Z*.15+Vibration)-V.Impact*.6)*S.Amount;
    V.Offset=V.Offset.GetClampedToMaxSize(S.MaxOffsetCm);
    V.Rotation=FRotator(FMath::Clamp((V.AccelBlend.X*.8+V.Impact.X)*S.Amount,-S.MaxAngleDegrees,S.MaxAngleDegrees),0,
        FMath::Clamp((-V.AccelBlend.Y*.6-V.AngularBlend.Z*.1-V.Impact.Y)*S.Amount,-S.MaxAngleDegrees,S.MaxAngleDegrees));
    V.FOV=S.BaseFOV+S.ExtraFOV*V.SpeedBlend*S.Amount;
    if(V.Offset.ContainsNaN() || V.Rotation.ContainsNaN() || !FMath::IsFinite(V.FOV))return false;
    *this=V;return true;
}
