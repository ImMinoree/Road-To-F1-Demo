#pragma once
#include "CoreMinimal.h"
#include "KartViewSettings.generated.h"

/** Presentation only. Runtime clamps also protect values set outside editor metadata. */
USTRUCT(BlueprintType)
struct FKartViewSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Camera") FVector EyePosition=FVector(-24,0,64);
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="-15",ClampMax="5")) float EyePitch=-8;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="80",ClampMax="95")) float LowSpeedFOV=88;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="85",ClampMax="105")) float HighSpeedFOV=98;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="1",ClampMax="12")) float BlendSpeed=5;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax="1")) float MotionScale=.65f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax=".2")) float EngineVibrationCm=.035f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax=".2")) float RoadVibrationCm=.07f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax=".4")) float KerbVibrationCm=.18f;
};
