#include "KartExperimentalVehicle.h"
#include "KartPhysicsIntegrator.h"
bool FKartVehicleSettings::IsValid() const
{
    auto Range=[](double V,double A,double B){return FMath::IsFinite(V) && V>=A && V<=B;};
    return Range(MassKg,80,250) && Range(SpeedCapKmh,1,57) && Range(ReverseCapKmh,1,20)
        && Range(WheelbaseCm,80,130) && Range(TrackCm,60,110) && Range(RestHeightCm,18,30)
        && Range(WheelRadiusCm,8,18) && Range(SteerLimitRadians,.1,.6) && Range(SteerRate,.2,3)
        && Range(DriveAcceleration,100,500) && Range(BrakeAcceleration,200,1000)
        && Range(FrontGrip,.5,1.5) && Range(RearGrip,.5,1.5);
}
bool FKartExperimentalVehicle::Reset(const FKartVehicleSettings& Config,const FVector& Position)
{
    if(!Config.IsValid())return false;
    FKartExperimentalVehicle V;V.Settings=Config;
    if(!V.Body.ConfigureBox(Config.MassKg,FVector(Config.WheelbaseCm,Config.TrackCm,30)) || !V.Body.SetPose(Position,FQuat::Identity))return false;
    *this=V;return true;
}
FVector FKartExperimentalVehicle::WheelPosition(int32 I) const
{
    return Body.GetPosition()+Body.GetOrientation().RotateVector(FVector(I<2?Settings.WheelbaseCm*.5:-Settings.WheelbaseCm*.5,
        I%2==0?-Settings.TrackCm*.5:Settings.TrackCm*.5,0));
}
bool FKartExperimentalVehicle::Step(const FKartVehicleInput& Input,const FKartWheelGround (&Ground)[4],double Dt)
{
    if(!Settings.IsValid() || !Body.IsValid() || Body.IsImmovable() || !FMath::IsFinite(Dt) || Dt<=0 || Dt>1./120+1.e-12
        || !FMath::IsFinite(Input.Throttle) || !FMath::IsFinite(Input.Steer) || !FMath::IsFinite(Input.Brake)
        || !FMath::IsFinite(Steering))return false;
    auto V=*this;auto& B=V.Body;const auto& C=Settings;
    const double Throttle=FMath::Clamp(Input.Throttle,-1.,1.),Brake=FMath::Clamp(Input.Brake,0.,1.);
    const auto Before=B.GetLinearVelocity();const auto Q=B.GetOrientation();
    const double ForwardSpeed=Q.UnrotateVector(Before).X;
    const double SteeringScale=1/(1+FMath::Square(ForwardSpeed/1300));
    const double TargetSteer=FMath::Clamp(Input.Steer,-1.,1.)*C.SteerLimitRadians*SteeringScale;
    V.Steering+=FMath::Clamp(TargetSteer-V.Steering,-C.SteerRate*Dt,C.SteerRate*Dt);
    B.ClearAccumulators();B.AddForce(FVector(0,0,-980*C.MassKg));
    // Small physical drag; no arbitrary per-frame velocity multiplier.
    B.AddForce(-Before*(C.MassKg*.015+Before.Size()*.0005));
    const double Spring=C.MassKg*980/8.,Damping=1.6*FMath::Sqrt(Spring*C.MassKg/4);
    const double Radius=C.WheelRadiusCm,WheelInertia=3*.5*Radius*Radius,Stiffness=C.MassKg*4;
    for(int I=0;I<4;++I)
    {
        const auto& G=Ground[I];auto& W=V.Wheels[I];const double OldOmega=W.AngularVelocity;W={};W.AngularVelocity=OldOmega;
        if(!FMath::IsFinite(OldOmega) || !FMath::IsFinite(G.HeightCm) || G.Normal.ContainsNaN()
            || FMath::Abs(G.Normal.SizeSquared()-1)>1.e-6 || G.Normal.Z<.5)return false;
        const FVector Mount=V.WheelPosition(I),Contact(Mount.X,Mount.Y,G.HeightCm);
        const auto R=Contact-B.GetPosition(),PointV=Before+FVector::CrossProduct(B.GetAngularVelocity(),R);
        const double Compression=C.RestHeightCm-(Mount.Z-G.HeightCm);
        const double Load=G.bHit && Compression>=0?FMath::Clamp(Spring*Compression-Damping*FVector::DotProduct(PointV,G.Normal),0.,C.MassKg*980):0;
        W.bSupported=Load>0;W.Load=Load;
        FVector Forward=Q.RotateVector(FVector(FMath::Cos(I<2?V.Steering:0),FMath::Sin(I<2?V.Steering:0),0));
        Forward=(Forward-G.Normal*FVector::DotProduct(Forward,G.Normal)).GetSafeNormal();
        const auto Right=FVector::CrossProduct(G.Normal,Forward);
        const double LongV=FVector::DotProduct(PointV,Forward),LatV=FVector::DotProduct(PointV,Right);
        const double Cap=(Throttle>=0?C.SpeedCapKmh:C.ReverseCapKmh)/.036;
        // Torque governor also watches wheel tread speed: low grip/airborne wheels must not
        // integrate unlimited engine energy merely because chassis speed remains low.
        const double GovernorSpeed=FMath::Max(FMath::Abs(ForwardSpeed),FMath::Abs(OldOmega)*Radius-100);
        const double Drive=I>=2?Throttle*C.MassKg*C.DriveAcceleration*.5*FMath::Clamp((Cap-GovernorSpeed)/100.,0.,1.):0;
        double Torque=Drive*Radius;
        const double BrakeTorque=Brake*C.MassKg*C.BrakeAcceleration*.25*Radius;
        const double PredictedOmega=OldOmega+Torque*Dt/WheelInertia;
        Torque-=FMath::Sign(PredictedOmega)*FMath::Min(BrakeTorque,FMath::Abs(PredictedOmega)*WheelInertia/Dt);
        W.SlipSpeed=OldOmega*Radius-LongV;
        double FX=Load>0?Stiffness*(W.SlipSpeed+Dt*Radius*Torque/WheelInertia)/(1+Dt*Stiffness*(Radius*Radius/WheelInertia+4/C.MassKg)):0;
        double FY=Load>0?-LatV*Stiffness/(1+Dt*Stiffness*4/C.MassKg):0;
        const double Mu=FKartContactMaterial::Surface(G.Surface).DynamicFriction*(I<2?C.FrontGrip:C.RearGrip);
        W.GripLimit=Mu*Load;const double ForceSize=FMath::Sqrt(FX*FX+FY*FY);
        if(ForceSize>W.GripLimit && ForceSize>0){FX*=W.GripLimit/ForceSize;FY*=W.GripLimit/ForceSize;}
        W.LongitudinalForce=FX;W.LateralForce=FY;
        W.AngularVelocity=OldOmega+(Torque-FX*Radius)*Dt/WheelInertia;
        if(!FMath::IsFinite(W.AngularVelocity) || FMath::Abs(W.AngularVelocity)>2000)return false;
        if(!B.AddForceAtPoint(G.Normal*Load+Forward*FX+Right*FY,Contact))return false;
    }
    if(!FKartPhysicsIntegrator::Step(B,Dt) || B.GetLinearVelocity().Size()>6000 || B.GetAngularVelocity().Size()>30)return false;
    V.Acceleration=(B.GetLinearVelocity()-Before)/Dt;
    if(V.Acceleration.ContainsNaN())return false;
    *this=V;return true;
}
