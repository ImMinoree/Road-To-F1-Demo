#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartExperimentalVehicle.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage7Test,"RoadToF1.Physics.Stage7Vehicle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage7Test::RunTest(const FString&)
{
    auto Run=[&](double Throttle,double Steer,EKartContactSurface Surface,int Steps)
    {
        FKartExperimentalVehicle V;V.Reset({});FKartWheelGround Ground[4];for(auto& G:Ground)G.Surface=Surface;
        bool Valid=true,Grip=true;double MaxAngular=0,MaxSpeed=0;
        for(int I=0;I<Steps;++I)
        {
            Valid&=V.Step({Throttle,Steer,0},Ground,1./120);
            for(const auto& W:V.Wheels)Grip&=FMath::Sqrt(W.LongitudinalForce*W.LongitudinalForce+W.LateralForce*W.LateralForce)<=W.GripLimit+1.e-7;
            MaxAngular=FMath::Max(MaxAngular,V.Body.GetAngularVelocity().Size());MaxSpeed=FMath::Max(MaxSpeed,V.Body.GetLinearVelocity().Size()*.036);
        }
        TestTrue(TEXT("Vehicle steps valid"),Valid);TestTrue(TEXT("Combined grip saturated safely"),Grip);
        TestTrue(TEXT("Angular velocity bounded"),MaxAngular<5);
        TestTrue(TEXT("No runaway speed"),MaxSpeed<57.1);
        AddInfo(FString::Printf(TEXT("Vehicle throttle %.1f steer %.2f surface %d steps %d peak %.4f km/h final %.4f angular %.4f"),Throttle,Steer,int(Surface),Steps,MaxSpeed,V.Body.GetLinearVelocity().Size()*.036,MaxAngular));
        return V;
    };
    auto A=Run(1,0,EKartContactSurface::Asphalt,2400),B=Run(1,0,EKartContactSurface::Asphalt,2400);
    TestTrue(TEXT("Uses speed capability"),A.Body.GetLinearVelocity().X*.036>54);
    TestTrue(TEXT("Exact input replay"),A.Body.GetPosition()==B.Body.GetPosition() && A.Body.GetOrientation()==B.Body.GetOrientation());
    auto Reverse=Run(-1,0,EKartContactSurface::Asphalt,1200);TestTrue(TEXT("Reverse propulsion"),Reverse.Body.GetLinearVelocity().X<-200);
    auto Rest=Run(0,0,EKartContactSurface::Asphalt,1200);TestTrue(TEXT("Stationary stable"),Rest.Body.GetLinearVelocity().Size()<.001 && FMath::Abs(Rest.Body.GetPosition().Z-22)<.01);
    auto Turn=Run(.6,.3,EKartContactSurface::Asphalt,1200);TestTrue(TEXT("Steering turns chassis"),FMath::Abs(Turn.Body.GetOrientation().Z)>.01);
    auto Grass=Run(1,0,EKartContactSurface::Grass,240),Asphalt=Run(1,0,EKartContactSurface::Asphalt,240);
    TestTrue(TEXT("Grass reduces acceleration"),Grass.Body.GetLinearVelocity().X<Asphalt.Body.GetLinearVelocity().X);
    FKartWheelGround Ground[4];bool BrakeValid=true;
    const double Before=A.Body.GetLinearVelocity().Size();
    for(int I=0;I<600;++I)BrakeValid&=A.Step({0,0,1},Ground,1./120);
    TestTrue(TEXT("Braking stops kart"),BrakeValid && A.Body.GetLinearVelocity().Size()<Before*.05);
    bool Transition=true;
    for(int I=0;I<600;++I)
    {
        for(auto& G:Ground){G.Surface=I<60?EKartContactSurface::Kerb:EKartContactSurface::Asphalt;G.HeightCm=I<60?2:0;G.bHit=I<100 || I>=110;}
        Transition&=Asphalt.Step({.3,.1,0},Ground,1./120);
    }
    TestTrue(TEXT("Kerb landing contact recovery"),Transition && Asphalt.Wheels[0].bSupported && Asphalt.Body.GetAngularVelocity().Size()<5);
    // Speed-dependent steering and configurable front/rear grip balance, no trajectory oracle fabricated.
    auto Balance=[&](double Front,double Rear,double InitialSpeed)
    {
        FKartVehicleSettings C;C.FrontGrip=Front;C.RearGrip=Rear;
        FKartExperimentalVehicle V;V.Reset(C);V.Body.SetVelocities(FVector(InitialSpeed,0,0),FVector::ZeroVector);
        for(auto& W:V.Wheels)W.AngularVelocity=InitialSpeed/C.WheelRadiusCm;
        FKartWheelGround Flat[4];bool Valid=true;
        for(int I=0;I<240;++I)Valid&=V.Step({.3,.8,0},Flat,1./120);
        TestTrue(TEXT("Low/high speed steering stable"),Valid && V.Body.GetAngularVelocity().Size()<5);
        const double Yaw=V.Body.GetAngularVelocity().Z;
        AddInfo(FString::Printf(TEXT("Grip balance front %.2f rear %.2f initial %.1f yaw %.6f rad/s steer %.6f"),Front,Rear,InitialSpeed,Yaw,V.Steering));
        return V;
    };
    auto Low=Balance(1,1,100),High=Balance(1,1,1400),FrontLow=Balance(.5,1,1400),RearLow=Balance(1,.5,1400);
    TestTrue(TEXT("High speed steering range reduced"),FMath::Abs(High.Steering)<FMath::Abs(Low.Steering));
    TestTrue(TEXT("Grip balance changes response"),FMath::Abs(FrontLow.Body.GetAngularVelocity().Z-RearLow.Body.GetAngularVelocity().Z)>.01);
    FKartExperimentalVehicle NPC;FKartVehicleSettings NC;NC.SpeedCapKmh=55;NPC.Reset(NC);FKartWheelGround Flat[4];bool NPCValid=true;
    for(int I=0;I<2400;++I)NPCValid&=NPC.Step({1,0,0},Flat,1./120);
    TestTrue(TEXT("NPC existing cap retained"),NPCValid && NPC.Body.GetLinearVelocity().X*.036>52 && NPC.Body.GetLinearVelocity().X*.036<=55);
    Run(1,.1,EKartContactSurface::LowGrip,600);
    const auto Saved=A.Body.GetPosition();TestFalse(TEXT("Oversized step rejected"),A.Step({},Ground,.1));TestTrue(TEXT("Failure atomic"),A.Body.GetPosition()==Saved);
    return true;
}
#endif
