#include "KartPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/SpringArmComponent.h"

UCameraComponent* AKartPawn::GetDrivingCamera() const { return bFirstPerson?EyeCamera:Camera; }
void AKartPawn::ToggleCamera() { if(!bOpponentRacer){bFirstPerson=!bFirstPerson;ApplyCameraMode();} }
void AKartPawn::ApplyCameraMode()
{
    EyeCamera->SetActive(!bOpponentRacer && bFirstPerson);
    Camera->SetActive(!bOpponentRacer && !bFirstPerson);
    Boom->SetComponentTickEnabled(!bOpponentRacer && !bFirstPerson);
    // Static arms cannot follow a rotating wheel. Hide the local unrigged driver
    // in cockpit view; other racers and the third-person driver remain complete.
    for(UStaticMeshComponent* Piece:DriverArt)Piece->SetOwnerNoSee(!bOpponentRacer && bFirstPerson);
    NumberShell->SetOwnerNoSee(!bOpponentRacer && bFirstPerson);
    NumberFace->SetOwnerNoSee(!bOpponentRacer && bFirstPerson);
    RacerNumber->SetOwnerNoSee(!bOpponentRacer && bFirstPerson);
    ResetDrivingView();
}
void AKartPawn::ResetDrivingView()
{
    ViewClock=ViewPitch=ViewRoll=KerbBlend=0;PreviousViewSpeed=Speed;
    EyeCamera->SetRelativeLocation(ViewSettings.EyePosition);
    EyeCamera->SetRelativeRotation(FRotator(FMath::Clamp(ViewSettings.EyePitch,-15.f,5.f),0,0));
    EyeCamera->SetFieldOfView(FMath::Clamp(ViewSettings.LowSpeedFOV,80.f,95.f));
    WheelSpin->SetRelativeRotation(FRotator(0,0,GetVisualSteeringAngle()));
}
void AKartPawn::UpdateDrivingView(float DeltaSeconds,bool Kerb)
{
    WheelSpin->SetRelativeRotation(FRotator(0,0,GetVisualSteeringAngle()));
    if(bOpponentRacer || !IsLocallyControlled() || !bFirstPerson){PreviousViewSpeed=Speed;return;}
    const float Dt=FMath::Clamp(DeltaSeconds,0.f,.1f);
    if(Dt<=SMALL_NUMBER)return;
    ViewClock=FMath::Fmod(ViewClock+Dt,60.f);
    const float Amount=FMath::Clamp(ViewSettings.MotionScale,0.f,1.f);
    const float Fraction=FMath::Clamp(FMath::Abs(Speed)/(SpeedLimitKmh/.036f),0.f,1.f);
    const float Accel=FMath::Clamp((Speed-PreviousViewSpeed)/Dt/1500.f,-1.f,1.f);
    PreviousViewSpeed=Speed;
    const float Blend=FMath::Clamp(ViewSettings.BlendSpeed,1.f,12.f);
    ViewPitch=FMath::FInterpTo(ViewPitch,Accel*1.2f*Amount,Dt,Blend);
    ViewRoll=FMath::FInterpTo(ViewRoll,-FMath::Clamp(YawRate/55.f,-1.f,1.f)*.7f*Amount,Dt,Blend);
    KerbBlend=FMath::FInterpTo(KerbBlend,Kerb?1.f:0.f,Dt,12.f);
    const float Vibration=Amount*(FMath::Clamp(ViewSettings.EngineVibrationCm,0.f,.2f)*(1-.4f*Fraction)*FMath::Sin(ViewClock*2*PI*14)
        +Fraction*FMath::Clamp(ViewSettings.RoadVibrationCm,0.f,.2f)*FMath::Sin(ViewClock*2*PI*19)
        +Fraction*KerbBlend*FMath::Clamp(ViewSettings.KerbVibrationCm,0.f,.4f)*FMath::Sin(ViewClock*2*PI*27));
    EyeCamera->SetRelativeLocation(ViewSettings.EyePosition+FVector(-Accel*.25f*Amount,ViewRoll*.4f,FMath::Clamp(Vibration,-.6f,.6f)));
    EyeCamera->SetRelativeRotation(FRotator(FMath::Clamp(ViewSettings.EyePitch,-15.f,5.f)+ViewPitch,0,ViewRoll));
    const float Low=FMath::Clamp(ViewSettings.LowSpeedFOV,80.f,95.f),High=FMath::Clamp(ViewSettings.HighSpeedFOV,Low,105.f);
    EyeCamera->SetFieldOfView(FMath::FInterpTo(EyeCamera->FieldOfView,FMath::Lerp(Low,High,Fraction),Dt,Blend));
}

