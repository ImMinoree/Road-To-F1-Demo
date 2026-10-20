#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "KartPawn.h"
#include "KartPhysicsWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage0QueryTest, "RoadToF1.Physics.Stage0Ownership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKartStage0QueryTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    auto* Kart = World->SpawnActor<AKartPawn>(FVector(0,0,1000), FRotator::ZeroRotator);
    auto* Obstacle = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Box); Box->SetBoxExtent(FVector(50));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent(); Obstacle->SetActorLocation(FVector(400,0,1000));
    auto* Subsystem = World->GetSubsystem<UKartPhysicsWorldSubsystem>();
    if (!TestNotNull(TEXT("World query coordinator"), Subsystem)) return false;
    const FTransform Before = Kart->GetActorTransform();
    TestEqual(TEXT("Legacy default"), Kart->GetPhysicsBackend(), EKartPhysicsBackend::Legacy);
    TestFalse(TEXT("Custom cannot activate before solver exists"), Kart->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));
    TestTrue(TEXT("Legacy request accepted"), Kart->TrySelectPhysicsBackend(EKartPhysicsBackend::Legacy));
    TestFalse(TEXT("Kart not engine simulated"), CastChecked<UPrimitiveComponent>(Kart->GetRootComponent())->IsSimulatingPhysics());
    FHitResult Hit;
    TestTrue(TEXT("Query detects static box"), Subsystem->ProbeKart(*Kart, 600, false, Hit));
    TestTrue(TEXT("Correct hit body"), Hit.GetComponent() == Box);
    TestTrue(TEXT("Normal opposes travel"), Hit.ImpactNormal.X < -.9);
    TestTrue(TEXT("Hit before end"), Hit.Time > 0 && Hit.Time < 1);
    TestTrue(TEXT("Query leaves transform unchanged"), Before.Equals(Kart->GetActorTransform()));
    TestEqual(TEXT("Query leaves speed unchanged"), Kart->GetSpeedKmh(), 0.f);
    TestEqual(TEXT("Query creates no incident"), Kart->GetCollisionCount(), 0);
    TestFalse(TEXT("Short probe misses"), Subsystem->ProbeKart(*Kart, 100, false, Hit));
    TestFalse(TEXT("Miss clears previous hit"), Hit.bBlockingHit);
    Box->SetCollisionObjectType(ECC_Pawn);
    TestTrue(TEXT("Pawn geometry is queryable"), Subsystem->ProbeKart(*Kart, 600, false, Hit));
    Obstacle->SetActorLocation(Kart->GetActorLocation());
    TestTrue(TEXT("Initial overlap detected"), Subsystem->ProbeKart(*Kart, 100, false, Hit));
    TestTrue(TEXT("Initial overlap flagged"), Hit.bStartPenetrating);
    TestTrue(TEXT("Overlap query does not depenetrate"), Before.Equals(Kart->GetActorTransform()));
    Obstacle->Destroy(); Kart->Destroy();
    return true;
}
#endif
