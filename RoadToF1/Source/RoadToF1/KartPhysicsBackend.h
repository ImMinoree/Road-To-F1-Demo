#pragma once
#include "CoreMinimal.h"
#include "KartPhysicsBackend.generated.h"

UENUM(BlueprintType)
enum class EKartPhysicsBackend : uint8
{
    Legacy,
    Custom
};
