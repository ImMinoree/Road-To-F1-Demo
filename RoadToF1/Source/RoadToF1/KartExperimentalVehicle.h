#pragma once
#include "KartRigidBodyState.h"
#include "KartContactMaterial.h"

struct FKartVehicleInput { double Throttle=0,Steer=0,Brake=0; };
struct FKartWheelGround
{
    bool bHit=true;
    double HeightCm=0;
    FVector Normal=FVector(0,0,1);
    EKartContactSurface Surface=EKartContactSurface::Asphalt;
};
struct FKartWheelTelemetry
{
    bool bSupported=false;
    double Load=0,LongitudinalForce=0,LateralForce=0,SlipSpeed=0,AngularVelocity=0,GripLimit=0;
};
struct FKartVehicleSettings
{
    double MassKg=160,SpeedCapKmh=57,ReverseCapKmh=15;
    double WheelbaseCm=100,TrackCm=80,RestHeightCm=24,WheelRadiusCm=12;
    double SteerLimitRadians=.45,SteerRate=1.8,DriveAcceleration=320,BrakeAcceleration=800;
    double FrontGrip=1,RearGrip=1;
    bool IsValid() const;
};
/** Isolated four support-point kart. No actors, collision queries, race logic or backend selection.
 * Ground samples are explicit caller-owned inputs, not automatically world collision.
 * Stiff damped supports approximate tire/chassis compliance (2cm static sag); rigid chassis, no flex joints.
 * Load transfer emerges from contact-force pitch/roll moments and spring load redistribution.
 * Combined slip-speed brush approximation, NOT Pacejka. Rear drive, four-wheel braking.
 * Public value state facilitates deterministic test fixtures; Step validates and commits atomically.
 */
struct ROADTOF1_API FKartExperimentalVehicle
{
    FKartRigidBodyState Body;
    FKartVehicleSettings Settings;
    FKartWheelTelemetry Wheels[4];
    double Steering=0;
    FVector Acceleration=FVector::ZeroVector;
    bool Reset(const FKartVehicleSettings& Config,const FVector& Position=FVector(0,0,22));
    bool Step(const FKartVehicleInput& Input,const FKartWheelGround (&Ground)[4],double Dt);
    FVector WheelPosition(int32 Index) const;
};
