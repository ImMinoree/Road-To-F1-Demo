#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "KartPawn.h"
#include "KartRaceDirector.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "UnrealClient.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyDrivingView, FAutomationTestBase*, Test);
bool FVerifyDrivingView::Update()
{
    UWorld* W = AutomationCommon::GetAnyGameWorld();
    auto* P = W ? Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(W, 0)) : nullptr;
    auto* D = W ? Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(W, AKartRaceDirector::StaticClass())) : nullptr;
    if (!P || !D) { Test->AddError(TEXT("Race world missing")); return true; }
    Test->TestTrue(TEXT("First person is default and active"), P->IsFirstPerson() && P->GetDrivingCamera()->IsActive());
    Test->TestTrue(TEXT("Eye sits inside driver's height envelope"), P->GetDrivingCamera()->GetRelativeLocation().Z > 55 && P->GetDrivingCamera()->GetRelativeLocation().Z < 75);
    Test->TestEqual(TEXT("Player cap preserved"), P->GetSpeedLimitKmh(), 57.f);
    for (const auto& S : D->GetOpponents()) if (S.Kart.IsValid())
    {
        Test->TestEqual(TEXT("NPC cap preserved"), S.Kart->GetSpeedLimitKmh(), 55.f);
        TArray<UCameraComponent*> Cameras; S.Kart->GetComponents(Cameras);
        for (auto* C : Cameras) Test->TestFalse(TEXT("NPC cameras are inactive"), C->IsActive());
    }
    P->ToggleCamera();
    Test->TestTrue(TEXT("Third person retained and active"), !P->IsFirstPerson() && P->GetDrivingCamera()->IsActive() && P->GetDrivingCamera()->GetName() == TEXT("FollowCamera"));
    P->ResetKart(); Test->TestFalse(TEXT("F5 kart reset retains selected camera"), P->IsFirstPerson());
    P->ToggleCamera();
    TArray<UStaticMeshComponent*> Meshes; P->GetComponents(Meshes);
    int32 WheelPieces = 0;
    for (auto* M : Meshes) if (M->GetName().StartsWith(TEXT("Wheel_")))
    {
        ++WheelPieces;
        Test->TestTrue(TEXT("Separated wheel mesh loaded"), M->GetStaticMesh() != nullptr);
        Test->TestEqual(TEXT("Wheel art cannot change collision"), M->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    }
    Test->TestEqual(TEXT("All wheel materials present"), WheelPieces, 3);
    P->SetDriveInput(0, 1);
    for (int32 I = 0; I < 10; ++I) P->Tick(.05f);
    const float Right = P->GetVisualSteeringAngle();
    Test->TestTrue(TEXT("Wheel follows positive steering with bounded travel"), Right > 0 && Right <= 65);
    P->SetDriveInput(0, -1);
    for (int32 I = 0; I < 20; ++I) P->Tick(.05f);
    Test->TestTrue(TEXT("Wheel follows left steering"), P->GetVisualSteeringAngle() < 0);
    P->SetDriveInput(0, 0);
    for (int32 I = 0; I < 40; ++I) P->Tick(.05f);
    Test->TestTrue(TEXT("Wheel returns smoothly to center"), FMath::Abs(P->GetVisualSteeringAngle()) < .1f);
    const auto Original = P->ViewSettings;
    P->ViewSettings.MotionScale = 0;
    for (int32 I = 0; I < 60; ++I) P->Tick(.05f);
    Test->TestTrue(TEXT("Comfort setting removes camera motion"), P->GetDrivingCamera()->GetRelativeLocation().Equals(P->ViewSettings.EyePosition, .001));
    P->ViewSettings.LowSpeedFOV = 500; P->ResetKart();
    Test->TestEqual(TEXT("Runtime FOV clamp protects invalid overrides"), P->GetDrivingCamera()->FieldOfView, 95.f);
    P->ViewSettings = Original; P->ResetKart();
    Test->TestEqual(TEXT("Reset clears FOV response"), P->GetDrivingCamera()->FieldOfView, Original.LowSpeedFOV);
    Test->TestEqual(TEXT("Reset clears wheel steering"), P->GetVisualSteeringAngle(), 0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrivingViewPIE, "RoadToF1.Presentation.DrivingViewPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDrivingViewPIE::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyDrivingView(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

// Explicit opt-in rendered fixture; never registered in standalone/shipping builds.
static FAutoConsoleCommandWithWorldAndArgs VisualCaptureCommand(
    TEXT("RoadToF1.VisualCapture"), TEXT("Warmed camera comparison captures; exits when complete."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.IsEmpty()) return;
        const FString Directory = Args[0];
        TWeakObjectPtr<UWorld> WeakWorld(World);
        auto Later = [World](float Delay, TFunction<void()> Action)
        {
            FTimerHandle Handle;
            World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Action]() { Action(); }), Delay, false);
        };
        auto Player = [WeakWorld]() -> AKartPawn* { return WeakWorld.IsValid() ? Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(WeakWorld.Get(), 0)) : nullptr; };
        auto Shot = [Directory, Player](const TCHAR* Name)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, FString(Name) + TEXT(".png")), true, false);
            if (auto* P = Player())
            {
                const FString Data = FString::Printf(TEXT("name=%s\nspeed_kmh=%.3f\nfov=%.3f\nwheel_degrees=%.3f\nposition=%s\neye_relative=%s\n"), Name, P->GetSpeedKmh(), P->GetDrivingCamera()->FieldOfView, P->GetVisualSteeringAngle(), *P->GetActorLocation().ToString(), *P->GetDrivingCamera()->GetRelativeLocation().ToString());
                FFileHelper::SaveStringToFile(Data, *FPaths::Combine(Directory, FString(Name) + TEXT(".txt")));
            }
        };
        if (Args.Num() > 1 && Args[1] == TEXT("bend"))
        {
            // Controlled camera fixture, not an AI or human performance test.
            // Position the player before a real authored bend, then use ordinary
            // throttle/steering through the unchanged swept kart controller.
            Later(15, [WeakWorld, Player]()
            {
                auto* W = WeakWorld.Get(); auto* P = Player();
                auto* D = W ? Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(W, AKartRaceDirector::StaticClass())) : nullptr;
                if (!P || !D || D->RoutePoints.Num() < 100) return;
                const auto& R = D->RoutePoints; int32 Apex = 30; double Curvature = 0;
                for (int32 I = 30; I < R.Num() / 3; ++I)
                {
                    const double C = 1 - FVector::DotProduct((R[I] - R[I - 4]).GetSafeNormal2D(), (R[I + 4] - R[I]).GetSafeNormal2D());
                    if (C > Curvature) { Curvature = C; Apex = I; }
                }
                const int32 Start = Apex - 15;
                P->ResetKart(); P->SetActorLocation(R[Start] + FVector(0, 0, 24.5));
                P->SetActorRotation((R[Start + 1] - R[Start]).Rotation());
                FTimerHandle Guide;
                W->GetTimerManager().SetTimer(Guide, FTimerDelegate::CreateLambda([Player, WeakDirector = TWeakObjectPtr<AKartRaceDirector>(D)]()
                {
                    auto* Kart = Player(); auto* Director = WeakDirector.Get();
                    if (!Kart || !Director) return;
                    const auto& Route = Director->RoutePoints;
                    int32 Near = 0; double Best = TNumericLimits<double>::Max();
                    for (int32 I = 0; I < Route.Num(); ++I)
                    {
                        const double Distance = FVector::DistSquared2D(Kart->GetActorLocation(), Route[I]);
                        if (Distance < Best) { Best = Distance; Near = I; }
                    }
                    FVector Delta = Route[(Near + 5) % Route.Num()] - Kart->GetActorLocation(); Delta.Z = 0;
                    const double Side = FVector::DotProduct(Delta, Kart->GetActorRightVector());
                    const double WheelRadians = FMath::Atan2(208. * Side, FMath::Max(1., Delta.SizeSquared()));
                    const double Speed = FMath::Abs(Kart->GetSpeedKmh()) / .036;
                    const double Steer = WheelRadians / FMath::DegreesToRadians(22. / (1 + Speed / 1400.));
                    Kart->SetDriveInput(Speed < 900 ? 1 : 0, FMath::Clamp(Steer, -1., 1.));
                }), 1.f / 30.f, true);
            });
            Later(19, [Shot]() { Shot(TEXT("BendEntry")); });
            Later(21, [Shot]() { Shot(TEXT("BendApex")); });
            Later(23, [Shot]() { Shot(TEXT("BendExit")); });
            Later(25, []() { FPlatformMisc::RequestExit(false); });
            return;
        }
        Later(20, [Shot]() { Shot(TEXT("FirstPersonStationary")); });
        Later(22, [Player]() { if (auto* P = Player()) P->ToggleCamera(); });
        Later(24, [Shot]() { Shot(TEXT("ThirdPersonVisual")); });
        Later(26, [Player]() { if (auto* P = Player()) { P->ToggleCamera(); P->SetDriveInput(0, .7f); } });
        Later(28, [Shot]() { Shot(TEXT("CockpitSteering")); });
        Later(29, [Player]() { if (auto* P = Player()) P->SetDriveInput(1, 0); });
        Later(36, [Shot]() { Shot(TEXT("FirstPersonSpeed")); });
        Later(37, [Player]() { if (auto* P = Player()) P->SetDriveInput(.5f, .25f); });
        Later(38, [Shot]() { Shot(TEXT("FirstPersonCornering")); });
        Later(39, [Player]() { if (auto* P = Player()) P->SetDriveInput(0, 0, true); });
        Later(41, [WeakWorld]()
        {
            if (auto* W = WeakWorld.Get()) if (auto* PC = UGameplayStatics::GetPlayerController(W, 0))
            {
                const FVector Location(-20000, -23000, 18000);
                auto* C = W->SpawnActor<ACameraActor>(Location, (FVector::ZeroVector - Location).Rotation());
                C->GetCameraComponent()->SetFieldOfView(65); PC->SetViewTarget(C);
            }
        });
        Later(44, [Shot]() { Shot(TEXT("Environment")); });
        Later(47, []() { FPlatformMisc::RequestExit(false); });
    }));
#endif
