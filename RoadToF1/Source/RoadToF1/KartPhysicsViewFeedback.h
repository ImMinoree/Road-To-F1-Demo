#pragma once
#include "CoreMinimal.h"
struct FKartPhysicsViewInput
{
    FVector LocalAcceleration=FVector::ZeroVector; // cm/s^2, gravity excluded by producer
    FVector LocalAngularVelocity=FVector::ZeroVector; // rad/s
    FVector LocalImpactImpulse=FVector::ZeroVector; // kg*cm/s; incremental events, once only
    double MassKg=160,SpeedCmPerSecond=0,RoadSeverity=0,KerbSeverity=0,EngineActivity=0;
};
struct FKartPhysicsViewSettings
{
    double Amount=.65,BlendRate=8,ImpactDecay=9,MaxOffsetCm=1.5,MaxAngleDegrees=2;
    double BaseFOV=88,ExtraFOV=6;
    bool IsValid() const;
};
/** Pure presentation value model. No actor/body writes. Caller rotates telemetry into cockpit axes.
 * Constant-target response and impulse decay are analytic exponentials. Vibration is a bounded
 * analytic phase function, not random per-frame noise. Reset on camera/race switch.
 * Experimental only; existing Legacy cameras continue unchanged. */
struct ROADTOF1_API FKartPhysicsViewFeedback
{
    FVector Offset=FVector::ZeroVector;
    FRotator Rotation=FRotator::ZeroRotator;
    double FOV=88;
    bool Step(const FKartPhysicsViewInput& Input,double Dt,const FKartPhysicsViewSettings& Settings={});
    void Reset(){*this=FKartPhysicsViewFeedback();}
private:
    FVector AccelBlend=FVector::ZeroVector,Impact=FVector::ZeroVector,AngularBlend=FVector::ZeroVector;
    double Clock=0,SpeedBlend=0;
};
