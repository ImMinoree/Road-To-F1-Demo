#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

bool UKartPhysicsWorldSubsystem::ProbeKart(const AKartPawn& Kart, float Distance, bool bDraw, FHitResult& Hit) const
{
    Hit = FHitResult();
    if (!GetWorld() || Kart.GetWorld() != GetWorld() || !FMath::IsFinite(Distance)) return false;
    const FVector Start = Kart.GetActorLocation();
    const FVector End = Start + Kart.GetActorForwardVector() * FMath::Clamp(Distance, 0.f, 5000.f);
    const bool Found = Provider.SweepBox(*GetWorld(), Start, End, Kart.GetActorQuat(), Kart.GetCollisionHalfExtent(), &Kart, Hit);
#if !UE_BUILD_SHIPPING
    if (bDraw)
    {
        DrawDebugBox(GetWorld(), Start, Kart.GetCollisionHalfExtent(), Kart.GetActorQuat(), FColor::Cyan, false, 5);
        DrawDebugBox(GetWorld(), End, Kart.GetCollisionHalfExtent(), Kart.GetActorQuat(), FColor::Yellow, false, 5);
        DrawDebugLine(GetWorld(), Start, End, FColor::Cyan, false, 5);
        const UPrimitiveComponent* Other = Hit.GetComponent();
        const uint32 A = Kart.GetRootComponent()->GetUniqueID(), B = Other ? Other->GetUniqueID() : 0;
        DrawDebugString(GetWorld(), Start + FVector(0,0,150), FString::Printf(
            TEXT("Stage0 QUERY ONLY | %s | body=%u pair=%u:%u\n%s | initial overlap=%d depth=%.2f"),
            Kart.GetPhysicsBackend() == EKartPhysicsBackend::Legacy ? TEXT("Legacy") : TEXT("Custom"),
            A, B ? FMath::Min(A,B) : A, B ? FMath::Max(A,B) : 0,
            *GetNameSafe(Other), Hit.bStartPenetrating, Hit.PenetrationDepth), nullptr, FColor::White, 5);
        if (Found) DrawDebugDirectionalArrow(GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 100,
            20, FColor::Red, false, 5, 0, 2);
    }
#endif
    return Found;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs ProbeCommand(
    TEXT("RoadToF1.Physics.Probe"), TEXT("Read-only Stage 0 player box sweep. Optional distance in cm (default 600)."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || !World->IsGameWorld()) return;
        const AKartPawn* Kart = Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(World, 0));
        if (!Kart) return;
        FHitResult Hit;
        World->GetSubsystem<UKartPhysicsWorldSubsystem>()->ProbeKart(*Kart,
            Args.IsEmpty() ? 600.f : FCString::Atof(*Args[0]), true, Hit);
    }));
#endif
