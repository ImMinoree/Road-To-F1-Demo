#include "KartRaceDirector.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "KartTrainingRecorder.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "KartPawn.h"
#include "KartPhysicsDrivingEnvelope.h"
#include "RaceLoopSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarBattleDebug(TEXT("RoadToF1.BattleDebug"),0,TEXT("Opt-in F9 evaluation sidecar; 0 off, 1 on."),ECVF_Cheat);
#endif
namespace Racecraft
{
constexpr float PredictionSeconds = .65f;
constexpr float LaunchHoldSeconds = 8.f;
constexpr float LaunchHoldDistance = 1200.f;
constexpr float LaunchMaxSeconds = 10.f;
constexpr float StuckDelay = 3.f;
constexpr float ReverseDuration = 1.2f;
constexpr float ReverseCooldown = 4.f;
constexpr float ReverseClearance = 550.f;
constexpr float LaneLimit = 300.f;
constexpr float PlanningBrakeDeceleration = 1200.f;
constexpr float ReactionSeconds = .1f;
constexpr float ObstructionLaneRate = 240.f;
constexpr float ObstructionSpeed = 400.f;
constexpr float TrafficReleaseRate = 800.f;
constexpr float PassLaneRate = 130.f;
constexpr float PassTimeout = 8.f;
constexpr float BrakeEnterError = 50.f;
constexpr float BrakeExitError = 10.f;
}

AKartRaceDirector::AKartRaceDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    CommentaryAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CommentaryAudio"));
    CommentaryAudio->bAutoActivate = false;
    CommentaryAudio->bIsUISound = true;
    CommentaryAudio->SetVolumeMultiplier(.8f);
    TrainingRecorder = CreateDefaultSubobject<UKartTrainingRecorder>(TEXT("TrainingRecorder"));
}

void AKartRaceDirector::BeginPlay()
{
    Super::BeginPlay();
    if (RoutePoints.Num() < 100 || Grid.Num() != 20) return;
    // Explicit approval file only. Never install a freshly fitted candidate automatically.
    FString ProfilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Training/approved_profiles.json"));
    if (!IFileManager::Get().FileExists(*ProfilePath))
        ProfilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Training/approved_profiles.json"));
    if (IFileManager::Get().FileExists(*ProfilePath))
    {
        FString Json;
        if (!FFileHelper::LoadFileToString(Json, *ProfilePath) || !FKartTrainingProfiles::Parse(Json, TEXT("SouthGarda_KartRace"), LearnedStyles))
            UE_LOG(LogTemp, Warning, TEXT("Training profiles rejected; retaining default racecraft: %s"), *ProfilePath);
    }
    for (int32 Index = 0; Index < 19; ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AKartPawn* Kart = GetWorld()->SpawnActor<AKartPawn>(AKartPawn::StaticClass(), Grid[Index], Params);
        Kart->ConfigureRacer(Index + 1, true);
        Kart->AddTickPrerequisiteActor(this);
        FKartOpponentState State;
        State.Kart = Kart;
        Opponents.Add(State);
    }
    ResetField();
}

void AKartRaceDirector::ResetField()
{
    FlushBattleDebug();
    bBattleCapture = false; BattleCapturePath.Empty(); BattleElapsed = BattleSampleClock = 0;
    bReleased = false;
    CommentaryAudio->Stop(); CommentaryCaption.Empty();
    CaptionUntil = CommentaryCooldownUntil = 0;
    AnnouncedLap = 0; PreviousPlayerPlace = 20;
    bAnnouncedFinish = bPreviousFire = false;
    FinishCount = PlayerFinishPlace = 0;
    const URaceLoopSubsystem* Race = GetWorld()->GetSubsystem<URaceLoopSubsystem>();
    const int32 Laps = Race ? Race->GetProgress().TargetLaps : 3;
    const int32 Checkpoints = Race && Race->IsRaceAvailable() ? Race->GetGates().Num() - 1 : 12;
    for (int32 Index = 0; Index < Opponents.Num(); ++Index)
    {
        auto& State = Opponents[Index];
        State.Progress.Reset(Laps, Checkpoints);
        State.FinishPlace = 0;

        State.Aggression = .25f + ((Index * 7) % 19) / 25.f;
        State.CornerSkill = .95f + ((Index * 11) % 19) / 360.f;
        State.PreferredLane = ((Index * 13) % 19 - 9) * 14.f;
        State.DecisionCooldown = 1.f + Index * .13f;
        State.DecisionSeconds = 3.5f - State.Aggression;
        if (HasLearnedStyles())
        {
            const auto& Style = LearnedStyles[Index];
            State.Aggression = Style.Aggression; State.CornerSkill = Style.CornerSkill;
            State.PreferredLane = Style.PreferredLane; State.DecisionSeconds = Style.DecisionSeconds;
        }
        State.StalledSeconds = State.RecoverySeconds = State.RecoveryCooldown = 0;
        State.LaunchSeconds = State.LaunchDistance = State.TargetSpeed = 0;
        State.bQueued = false; State.Pace = FKartPaceDebug();
        State.PassPhase = EKartPassPhase::Follow; State.PassTarget.Reset();
        State.Battle = FKartBattleState();
        State.ConstrainedSeconds = State.PassSeconds = 0;
        State.PassAttempts = State.CompletedPasses = 0; State.FilteredTrafficSpeed = 55.f/.036f;
        if (AKartPawn* Kart = State.Kart.Get())
        {
            Kart->SetActorTransform(Grid[Index], false, nullptr, ETeleportType::TeleportPhysics);
            Kart->ResetKart();
            State.Previous = Kart->GetActorLocation();
            State.RouteIndex = NearestPoint(State.Previous);
            const FVector Direction = (RoutePoints[(State.RouteIndex + 1) % RoutePoints.Num()] - RoutePoints[State.RouteIndex]).GetSafeNormal2D();
            State.ActualLane = FVector::DotProduct(State.Previous - RoutePoints[State.RouteIndex], FVector(-Direction.Y, Direction.X, 0));
            State.Lane = State.DesiredLane = State.ActualLane;
        }
    }
    TrainingRecorder->RestartSessionIfActive();
}

int32 AKartRaceDirector::NearestPoint(const FVector& P) const
{
    int32 Best = 0;
    double Distance = TNumericLimits<double>::Max();
    for (int32 Index = 0; Index < RoutePoints.Num(); ++Index)
    {
        const double D = FVector::DistSquared2D(P, RoutePoints[Index]);
        if (D < Distance) { Distance = D; Best = Index; }
    }
    return Best;
}

FVector AKartRaceDirector::LanePoint(int32 Index, float Lane) const
{
    const int32 N = RoutePoints.Num();
    Index = (Index + N) % N;
    const FVector Tangent = (RoutePoints[(Index + 1) % N] - RoutePoints[(Index + N - 1) % N]).GetSafeNormal2D();
    return RoutePoints[Index] + FVector(-Tangent.Y, Tangent.X, 0) * Lane;
}

void AKartRaceDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    AKartPawn* Player = Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    URaceLoopSubsystem* Race = GetWorld()->GetSubsystem<URaceLoopSubsystem>();
    if (!Player || !Race || !Race->IsRaceAvailable() || RoutePoints.IsEmpty() || Grid.Num() != 20) return;
    // The established start is deliberate forward throttle from the grid, not the first lap gate.
    const FVector GridForward = Grid.Last().GetUnitAxis(EAxis::X);
    if (!bReleased && Player->GetThrottleInput() > .1f && !Player->IsBrakeHeld()
        && !Player->IsRecoveringFromContact() && Player->GetSpeedKmh() > .5f
        && FVector::DotProduct(Player->GetActorForwardVector(), GridForward) > .7f
        && FVector::Dist2D(Player->GetActorLocation(), Grid.Last().GetLocation()) < 1500)
        bReleased = true;
    if (Race->GetProgress().bFinished && PlayerFinishPlace == 0)
    {
        PlayerFinishPlace = ++FinishCount;
        Player->SetRaceFinished();
    }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("RaceBattleDebug")) || CVarBattleDebug.GetValueOnGameThread()!=0) bPaceDiagnostics = true;
#endif
    UpdateCommentary();
    const auto& Gates = Race->GetGates();
    TArray<AKartPawn*, TInlineAllocator<20>> Traffic;
    Traffic.Add(Player);
    for (const auto& Other : Opponents) if (Other.Kart.IsValid()) Traffic.Add(Other.Kart.Get());
    for (int32 Agent = 0; Agent < Opponents.Num(); ++Agent)
    {
        auto& State = Opponents[Agent];
        AKartPawn* Kart = State.Kart.Get();
        if (!Kart) continue;
        const FVector P = Kart->GetActorLocation();
        if (bPaceDiagnostics) State.Pace = FKartPaceDebug();
        const FVector Movement = P - State.Previous;
        for (int32 Gate = 0; Gate < Gates.Num(); ++Gate)
        {
            double Fraction;
            if (Gates[Gate].ForwardCrossing(State.Previous, P, Fraction))
                State.Progress.CrossGate(Gate, GetWorld()->GetTimeSeconds());
        }
        State.LaunchDistance += bReleased ? FMath::Min(300.f, float(FVector::Dist2D(State.Previous, P))) : 0;
        State.Previous = P;
        if (State.Progress.bFinished)
        {
            if (State.FinishPlace == 0) { State.FinishPlace = ++FinishCount; Kart->SetRaceFinished(); }
            // Retain the result actor, coast first, then retire its presentation away from the finish instant.
            State.Battle.FinishSeconds += Dt;
            if (State.Battle.FinishSeconds >= 4 && FMath::Abs(Kart->GetSpeedKmh()) < 1) Kart->SetActorHiddenInGame(true);
            State.TargetSpeed = State.RecoverySeconds = State.StalledSeconds = 0;
            if (bPaceDiagnostics) { State.Pace.Recovery = 0; State.Pace.Reason = TEXT("Finished"); }
            Kart->SetDriveInput(0, 0);
            continue;
        }
        if (!bReleased) { if (bPaceDiagnostics) { State.Pace.Recovery = 0; State.Pace.Reason = TEXT("Grid"); } Kart->SetDriveInput(0, 0, true); continue; }
        State.LaunchSeconds += Dt;
        State.RecoveryCooldown = FMath::Max(0.f, State.RecoveryCooldown - Dt);
        const int32 NewContacts=FMath::Max(0,Kart->GetCollisionCount()-State.Battle.PreviousContacts);
        if(State.Battle.bIncidentActive)State.Battle.SecondaryContacts+=NewContacts;
        State.Battle.PreviousContacts=Kart->GetCollisionCount();
        if(Kart->IsRecoveringFromContact())
        {
            State.Battle.RejoinSeconds=2.5f;State.Battle.bRejoining=true;
            if(State.Battle.Defense==EKartDefense::Holding)State.Battle.EndDefense(EKartDefenseOutcome::ContactRisk);
            if(bPaceDiagnostics){State.Pace.Recovery=0;State.Pace.Reason=TEXT("Contact");}
            Kart->SetDriveInput(0,0,true);State.StalledSeconds=State.RecoverySeconds=0;State.RecoveryCooldown=Racecraft::ReverseCooldown;continue;
        }
        // Follow contiguous route samples, never the globally nearest adjoining
        // section at hairpins. The global search is used only for grid seeding.
        int32 Near = State.RouteIndex;
        double Closest = TNumericLimits<double>::Max();
        for (int32 Offset = -5; Offset <= 15; ++Offset)
        {
            const int32 Candidate = (State.RouteIndex + Offset + RoutePoints.Num()) % RoutePoints.Num();
            const double Distance = FVector::DistSquared2D(P, RoutePoints[Candidate]);
            if (Distance < Closest) { Closest = Distance; Near = Candidate; }
        }
        State.RouteIndex = Near;
        const float Speed = FMath::Abs(Kart->GetSpeedKmh()) / .036f;
        const float Spacing = FVector::Dist2D(RoutePoints[0], RoutePoints[1]);
        int32 Look = FMath::Clamp(FMath::RoundToInt((280 + Speed * .3f) / Spacing), 2, 5);
        State.DecisionCooldown = FMath::Max(0.f, State.DecisionCooldown - Dt);
        const FVector RoadDirection = (RoutePoints[(Near + 1) % RoutePoints.Num()] - RoutePoints[Near]).GetSafeNormal2D();
        const FVector RoadSide(-RoadDirection.Y, RoadDirection.X, 0);
        State.ActualLane = FVector::DotProduct(P - RoutePoints[Near], RoadSide);
        auto& Battle = State.Battle;
        const FVector PlayerDelta = Player->GetActorLocation() - P;
        Battle.PlayerAhead = FVector::DotProduct(PlayerDelta, RoadDirection);
        Battle.PlayerSide = FVector::DotProduct(PlayerDelta, RoadSide);
        Battle.PlayerDistance = PlayerDelta.Size2D();
        Battle.PlayerLane = State.ActualLane + Battle.PlayerSide;
        Battle.PlayerClosing = FVector::DotProduct(Player->GetVelocity()-Kart->GetVelocity(), RoadDirection);
        Battle.RepassSeconds = FMath::Max(0.f, Battle.RepassSeconds-Dt);
        Battle.DefenseCooldown = FMath::Max(0.f, Battle.DefenseCooldown-Dt);
        Battle.DefenseSeconds = FMath::Max(0.f, Battle.DefenseSeconds-Dt);
        Battle.Conflict = TEXT("None"); Battle.bYielding = false;
        if (Battle.PlayerDistance < 1500 && !Player->IsRaceFinished() && State.Progress.CompletedLaps==Race->GetProgress().CompletedLaps)
        {
            if (Battle.PlayerAhead < -250) { Battle.bPlayerSeen=true; Battle.bPlayerWasAhead=false; }
            else if (Battle.PlayerAhead > 250)
            {
                if (Battle.bPlayerSeen && !Battle.bPlayerWasAhead) { ++Battle.PositionsLost; Battle.RepassSeconds=6; }
                Battle.bPlayerSeen=true; Battle.bPlayerWasAhead=true;
            }
        }
        else { Battle.bPlayerSeen=false; Battle.RepassSeconds=0; }

        const bool bDisplaced = FVector::DistSquared2D(Movement, Kart->GetVelocity() * Dt) > FMath::Square(150.f);
        const bool bLaunching = State.LaunchSeconds < Racecraft::LaunchMaxSeconds &&
            (State.LaunchSeconds < Racecraft::LaunchHoldSeconds || State.LaunchDistance < Racecraft::LaunchHoldDistance);
        const FVector OwnExtent = Kart->GetCollisionHalfExtent();
        auto HalfSpan = [](const AKartPawn* K, const FVector& Axis)
        {
            const FVector E = K->GetCollisionHalfExtent();
            return FMath::Abs(FVector::DotProduct(K->GetActorForwardVector(), Axis)) * E.X
                + FMath::Abs(FVector::DotProduct(K->GetActorRightVector(), Axis)) * E.Y;
        };
        Battle.IncidentReservation=FMath::Max(0.f,Battle.IncidentReservation-Dt);
        if(bDisplaced){Battle.RejoinSeconds=2.5f;Battle.bRejoining=true;State.Lane=State.DesiredLane=State.ActualLane;}
        if(Battle.bRejoining)
        {
            Battle.RejoinSeconds=FMath::Max(0.f,Battle.RejoinSeconds-Dt);
        }
        bool bQueueAhead = false, bObstructionAhead = false;
        bool bIncidentAhead = false;
        AKartPawn* Leader = nullptr; float LeaderGap = TNumericLimits<float>::Max();
        const float ScanDistance = FMath::Max(1000.f, Speed * Speed / 2000.f + 450);
        auto SafeSpeed = [&](float Candidate, bool Capture = false)
        {
            float Limit = 55.f / .036f;
            for (const AKartPawn* Other : Traffic)
            {
                if (Other == Kart || Other->IsRaceFinished()) continue;
                const FVector Delta = Other->GetActorLocation() - P;
                const float ObjectScan = FMath::Abs(Other->GetSpeedKmh()) < 14 ? FMath::Max(2000.f, ScanDistance) : ScanDistance;
                if (Delta.SizeSquared2D() > FMath::Square(ObjectScan + 500)) continue;
                const float Ahead = FVector::DotProduct(Delta, RoadDirection);
                const float Side = FVector::DotProduct(Other->GetActorLocation() - RoutePoints[Near], RoadSide);
                if (FMath::Abs(Side) > 500) continue;
                const float Width = HalfSpan(Kart, RoadSide) + HalfSpan(Other, RoadSide) + FMath::Clamp(TrafficSafetyMargin, 10.f, 100.f) + (Other->IsBurning() ? 80.f : 0.f);
                const float Length = HalfSpan(Kart, RoadDirection) + HalfSpan(Other, RoadDirection) + FMath::Clamp(TrafficSafetyMargin, 10.f, 100.f);
                const FVector Predicted = Delta + (Other->GetVelocity() - Kart->GetVelocity()) * Racecraft::PredictionSeconds;
                const float PredictedAhead = FVector::DotProduct(Predicted, RoadDirection);
                if (Capture && Ahead > 0 && Ahead < LeaderGap && FMath::Abs(Side-State.ActualLane)<Width)
                { Leader = const_cast<AKartPawn*>(Other); LeaderGap = Ahead; }
                if(Ahead>0 && Ahead<ObjectScan && FMath::Abs(Side-State.ActualLane)<Width && FMath::Abs(Other->GetSpeedKmh())<14)
                    bObstructionAhead=true;
                if (Ahead > 0 && Ahead < 650 && FMath::Abs(Side - State.ActualLane) < Width)
                {
                    bQueueAhead = true;
                    if (FMath::Abs(Other->GetSpeedKmh()) < 14) bObstructionAhead = true;
                }
                if (Other->IsBurning() && Ahead > -200 && Ahead < ScanDistance && FMath::Abs(Side-State.ActualLane)<Width) bIncidentAhead = true;
                // A kart behind must not prevent forward escape from a pileup.
                // Reject crossing occupied lanes, not staying in the current lane.
                const FVector RelativeVelocity=Other->GetVelocity()-Kart->GetVelocity();
                const bool StableParallel=FMath::Abs(Ahead)<Length+150 && FMath::Abs(Side-State.ActualLane)>Width+20
                    && FVector::DotProduct(Other->GetActorForwardVector(),Kart->GetActorForwardVector())>.995f
                    && FMath::Abs(FVector::DotProduct(RelativeVelocity,RoadSide))<30;
                // Common lateral motion around a bend is not one kart crossing the other's corridor.
                // Actual footprint and any requested crossing remain checked below.
                const float PredictedSide = Side + FVector::DotProduct(StableParallel?RelativeVelocity:Other->GetVelocity(), RoadSide) * Racecraft::PredictionSeconds;
                // A rear kart in the same physical corridor must not veto an outward escape
                // into a genuinely clear corridor. This exception cannot cross an adjacent rear lane.
                const bool LeavingSharedRearLane=Ahead < -Length && FMath::Abs(Side-State.ActualLane)<OwnExtent.Y
                    && FMath::Abs(Candidate-Side)>Width && FMath::Abs(Candidate-PredictedSide)>Width;
                const bool bEscapingOverlap = LeavingSharedRearLane || ((Candidate - Side) * (State.ActualLane - Side) > 0
                    && FMath::Abs(Candidate - Side) > FMath::Abs(State.ActualLane - Side) + 20
                    && FMath::Abs(Candidate - PredictedSide) >= FMath::Abs(State.ActualLane - PredictedSide));
                if (!bEscapingOverlap && FMath::Abs(Candidate - State.ActualLane) > 20
                    && (FMath::Abs(Ahead) < Length || (Ahead < 0 && PredictedAhead > -Length && PredictedAhead < Length))
                    && FMath::Max(Side, PredictedSide) > FMath::Min(State.ActualLane, Candidate) - Width
                    && FMath::Min(Side, PredictedSide) < FMath::Max(State.ActualLane, Candidate) + Width) return -1.f;
                // Nearby committed destinations reserve room; physical occupancy above always wins.
                for (const auto& Peer : Opponents)
                    if (Peer.Kart.Get() == Other && Peer.DecisionCooldown > 0 && Ahead > -Length && Other->GetSpeedKmh() > 1 && FMath::Abs(Peer.DesiredLane - Peer.ActualLane) > 30 && FMath::Abs(Ahead) < Length * 1.5f
                        && FMath::Abs(Candidate - State.ActualLane) > 20 && FMath::Abs(Candidate - Peer.DesiredLane) < Width)
                        return -1.f;
                if (Ahead > 0 && Ahead < ObjectScan && FMath::Min(Side, PredictedSide) < Candidate + Width && FMath::Max(Side, PredictedSide) > Candidate - Width)
                {
                    const float Gap = FMath::Max(Length + 30, Other->IsBurning() ? 430.f : 330.f - State.Aggression * 90);
                    const float OtherSpeed = Other->IsBurning() ? 0 : FMath::Max(0.f, float(FVector::DotProduct(Other->GetVelocity(), RoadDirection)));
                    if (Capture && bPaceDiagnostics && (State.Pace.Ahead < 0 || Ahead < State.Pace.Ahead)) { State.Pace.Ahead = Ahead; State.Pace.RelativeSpeed = Speed - OtherSpeed; }
                    const float BrakingRoom = FMath::Max(0.f, Ahead - Length - 35 - Speed * Racecraft::ReactionSeconds);
                    const float BrakingLimit = FMath::Sqrt(OtherSpeed * OtherSpeed + 2 * Racecraft::PlanningBrakeDeceleration * BrakingRoom);
                    Limit = FMath::Min(Limit, FMath::Min(BrakingLimit, FMath::Max(0.f, OtherSpeed + (Ahead - Gap) * 1.6f)));
                }
            }
            return Limit;
        };
        const FVector UpcomingDirection = (RoutePoints[(Near + 8) % RoutePoints.Num()] - RoutePoints[(Near + 7) % RoutePoints.Num()]).GetSafeNormal2D();
        const bool bStraight = FVector::DotProduct(RoadDirection, UpcomingDirection) > .985;
        // The pawn has smoothed steering/yaw: a short high-speed pursuit target causes lateral oscillation.
        if (bStraight && Speed>600) Look=FMath::Clamp(FMath::RoundToInt((350+Speed*.6f)/Spacing),3,7);
        // Keep a physical corridor while overlapping, including through bends.
        // Entry/exit longitudinal hysteresis avoids toggling at wheel overlap.
        bool Alongside = false;
        AKartPawn* Attacker = nullptr;
        float ClosestAttacker = 1800;
        Battle.bDefenseEligible=false; Battle.DefenseGeometry=FVector::DotProduct(RoadDirection,UpcomingDirection);
        for (AKartPawn* Other : Traffic)
        {
            if(Other==Kart || Other->IsRaceFinished() || Other->IsBurning() || Other->GetSpeedKmh()<14)continue;
            const FVector Delta=Other->GetActorLocation()-P;
            const float Ahead=FVector::DotProduct(Delta,RoadDirection);
            const float Side=FVector::DotProduct(Delta,RoadSide);
            if(FVector::DotProduct(Other->GetActorForwardVector(),RoadDirection)<.7f || FMath::Abs(Side)>650)continue;
            const float Length=HalfSpan(Kart,RoadDirection)+HalfSpan(Other,RoadDirection);
            const float Width=HalfSpan(Kart,RoadSide)+HalfSpan(Other,RoadSide)+TrafficSafetyMargin;
            if(FMath::Abs(Ahead)<Length+(Battle.bAlongside?180:60) && FMath::Abs(Side)>Width-20)
                Alongside=true;
            const float Closing=FVector::DotProduct(Other->GetVelocity()-Kart->GetVelocity(),RoadDirection);
            if(Ahead < -Length-180 && -Ahead<ClosestAttacker && Closing>25 && ( -Ahead-Length)/Closing<8 && FMath::Abs(Side)<500)
            { Attacker=Other; ClosestAttacker=-Ahead; }
        }
        if(Alongside && !Battle.bAlongside) Battle.Corridor=SafeSpeed(State.DesiredLane)>=0 ? State.DesiredLane : FMath::Clamp(State.ActualLane,-300.f,300.f);
        Battle.bAlongside=Alongside;
        if(Alongside && !bLaunching)
        {
            Battle.AlongsideSeconds+=Dt;
            if(SafeSpeed(Battle.Corridor)>=0) State.DesiredLane=Battle.Corridor;
        }
        const float ActualCorridorSafety=SafeSpeed(State.ActualLane,true);
        if(Battle.Defense==EKartDefense::Holding)
        {
            Battle.DefenseElapsed+=Dt;
            Battle.DefenseMovement=FMath::Abs(State.ActualLane-Battle.DefenseStartLane);
            if(Battle.DefenseMoveDelay<0 && Battle.DefenseMovement>25)Battle.DefenseMoveDelay=Battle.DefenseElapsed;
            Battle.DefenseMinimumSpeed=FMath::Min(Battle.DefenseMinimumSpeed,Kart->GetSpeedKmh());
            AKartPawn* Threat=nullptr;
            for(auto* Other:Traffic)if(Other->GetKartNumber()==Battle.DefenseRival)Threat=Other;
            const float ThreatAhead=Threat?FVector::DotProduct(Threat->GetActorLocation()-P,RoadDirection):-3000;
            if(bObstructionAhead || bIncidentAhead || ActualCorridorSafety<Speed*.65f)Battle.EndDefense(EKartDefenseOutcome::LostSpeed);
            else if(!Threat || Threat->IsRaceFinished() || ThreatAhead < -1900) Battle.EndDefense(EKartDefenseOutcome::ThreatCleared);
            else if(Alongside) Battle.EndDefense(Battle.DefenseMovement<60?EKartDefenseOutcome::TooLate:EKartDefenseOutcome::Alongside);
            else if(SafeSpeed(Battle.DefenseLane)<0) { Battle.EndDefense(EKartDefenseOutcome::Occupied); Battle.Conflict=TEXT("DefenseOccupied"); Battle.bYielding=true; }
            else if(Battle.DefenseSeconds<=0)
                Battle.EndDefense(ThreatAhead<-250 && FMath::Abs(State.ActualLane-Battle.DefenseLane)<90 ? EKartDefenseOutcome::Held : EKartDefenseOutcome::TooLate);
            else State.DesiredLane=Battle.DefenseLane;
        }
        if(Battle.Defense==EKartDefense::Released && Battle.DefenseCooldown<=0)Battle.Defense=EKartDefense::Normal;
        const bool Passing=State.PassPhase==EKartPassPhase::Committed || State.PassPhase==EKartPassPhase::Passing;
        if(!bLaunching && !Alongside && !Passing && !bObstructionAhead && !bIncidentAhead && !Battle.bRejoining && Battle.DefenseGeometry>.95f && Attacker && Battle.Defense==EKartDefense::Normal
            && Speed>30.f/.036f && ActualCorridorSafety>FMath::Max(45.f/.036f,Speed*.9f))
        {
            // One chosen side per threat; a cooldown prevents weaving after release.
            Battle.bDefenseEligible=true;
            Battle.DefenseGap=ClosestAttacker;
            Battle.DefenseClosing=FVector::DotProduct(Attacker->GetVelocity()-Kart->GetVelocity(),RoadDirection);
            const FVector CornerDirection=(RoutePoints[(Near+15)%RoutePoints.Num()]-RoutePoints[(Near+14)%RoutePoints.Num()]).GetSafeNormal2D();
            const float Bend=FVector::CrossProduct(RoadDirection,CornerDirection).Z;
            const float ThreatSide=FVector::DotProduct(Attacker->GetActorLocation()-RoutePoints[Near],RoadSide);
            const float Sign=FMath::Abs(Bend)>.005f ? FMath::Sign(Bend) : (ThreatSide>=0?1.f:-1.f);
            // On a straight cover the attacker's corridor, rather than vacating it to an arbitrary side.
            // For an approaching bend the signed inside corridor remains the priority.
            float DefendLane=FMath::Abs(Bend)>.005f ? Sign*(245+State.Aggression*35) : FMath::Clamp(ThreatSide,-300.f,300.f);
            const float Closing=FMath::Max(0.f,float(FVector::DotProduct(Attacker->GetVelocity()-Kart->GetVelocity(),RoadDirection)));
            const float AvailableMove=FMath::Max(0.f,(ClosestAttacker-HalfSpan(Kart,RoadDirection)-HalfSpan(Attacker,RoadDirection)-170)/FMath::Max(Closing,25.f)-.7f)*(70+State.Aggression*25);
            DefendLane=State.ActualLane+FMath::Clamp(DefendLane-State.ActualLane,-AvailableMove,AvailableMove);
            const float MoveTime=FMath::Abs(DefendLane-State.ActualLane)/(70+State.Aggression*25)+.7f;
            const float RequiredGap=HalfSpan(Kart,RoadDirection)+HalfSpan(Attacker,RoadDirection)+150+Closing*MoveTime;
            if((FMath::Abs(DefendLane-State.ActualLane)>50 || FMath::Abs(ThreatSide-State.ActualLane)<80) && ClosestAttacker>RequiredGap && SafeSpeed(DefendLane)>=FMath::Max(25.f/.036f,Speed*.9f))
            {
                Battle.Defense=EKartDefense::Holding;Battle.DefenseLane=DefendLane;Battle.DefenseSeconds=FMath::Clamp((ClosestAttacker-200)/FMath::Max(Closing,25.f)+.5f,3.f,6.f);
                Battle.DefenseElapsed=0;Battle.DefenseStartLane=State.ActualLane;Battle.DefenseMoveDelay=-1;
                Battle.DefenseStartSpeed=Battle.DefenseMinimumSpeed=Kart->GetSpeedKmh();Battle.DefenseOutcome=EKartDefenseOutcome::None;
                ++Battle.Defenses;Battle.DefenseRival=Attacker->GetKartNumber();State.DesiredLane=DefendLane;
            }
        }
        const float CurrentSafety = SafeSpeed(State.DesiredLane, true);
        State.PassSeconds = FMath::Max(0.f, State.PassSeconds - Dt);
        bool bCommitted = State.PassPhase == EKartPassPhase::Committed || State.PassPhase == EKartPassPhase::Passing;
        if (bCommitted)
        {
            const AKartPawn* Rival = State.PassTarget.Get();
            const float RivalAhead = Rival ? FVector::DotProduct(Rival->GetActorLocation()-P,RoadDirection) : 0;
            if(State.PassSeconds>7.8f)Battle.PassExtension=0;
            if(Rival && State.PassSeconds<=0 && Battle.PassExtension<3 && Battle.bAlongside && CurrentSafety>=0
                && RivalAhead<150 && RivalAhead>-300 && FVector::DotProduct(Kart->GetVelocity()-Rival->GetVelocity(),RoadDirection)>25)
            { State.PassSeconds=.5f; Battle.PassExtension+=.5f; }
            if (Rival && RivalAhead < -300 && FVector::Dist2D(Rival->GetActorLocation(),P)<2000)
            { ++State.CompletedPasses; Battle.Outcome(EKartPassOutcome::Completed); if(Battle.bRepassAttempt) ++Battle.RepassCompletions; Battle.bRepassAttempt=false; State.PassPhase=EKartPassPhase::Clear; State.PassSeconds=1.f; bCommitted=false; }
            else if (!Rival || Rival->IsRaceFinished() || State.PassSeconds<=0 || CurrentSafety<0)
            { Battle.Outcome(CurrentSafety<0 ? EKartPassOutcome::Occupied : State.PassSeconds<=0 ? EKartPassOutcome::Timeout : EKartPassOutcome::RivalUnavailable); Battle.bRepassAttempt=false; State.PassPhase=EKartPassPhase::Follow; State.PassTarget.Reset(); bCommitted=false; }
            else if (FMath::Abs(RivalAhead)<300) State.PassPhase=EKartPassPhase::Passing;
        }
        if (!bCommitted && State.PassPhase!=EKartPassPhase::Clear)
        {
            const bool Constrained = Leader && SafeSpeed(State.ActualLane) < 50.f/.036f && Leader->GetSpeedKmh()>14 && Speed > 400;
            State.ConstrainedSeconds = Constrained ? State.ConstrainedSeconds + Dt : 0;
            State.PassPhase=Constrained ? EKartPassPhase::Consider : EKartPassPhase::Follow;
        }
        if (State.PassPhase==EKartPassPhase::Clear && State.PassSeconds<=0) {State.PassPhase=EKartPassPhase::Follow;State.PassTarget.Reset();}
        const int32 NextGate = State.Progress.NextCheckpoint == State.Progress.CheckpointCount ? 0 : State.Progress.NextCheckpoint + 1;
        const auto& Gate = Gates[NextGate];
        const FVector GateLocal = Gate.Transform.InverseTransformPosition(P);
        const bool bGateApproach = State.Progress.bStarted && GateLocal.X < 0 && GateLocal.X > -3000 && FMath::Abs(GateLocal.Y) < 2000;
        bool bGateLaneClear = true;
        if (!bLaunching && !Battle.bAlongside && Battle.Defense!=EKartDefense::Holding && !bCommitted && State.PassPhase!=EKartPassPhase::Clear && (bStraight || (bObstructionAhead && Speed < 250 && CurrentSafety < 250)) && (State.DecisionCooldown <= 0 || CurrentSafety < 0))
        {
            float BestLane = State.DesiredLane;
            float BestScore = -1e9f;
            const float PassLane = Racecraft::LaneLimit;
            const float Candidates[] = {State.DesiredLane, State.ActualLane, State.PreferredLane, -130.f, 130.f, -PassLane, PassLane};
            for (float Candidate : Candidates)
            {
                const float Allowed = SafeSpeed(Candidate);
                if (Allowed < 0) continue;
                bool BlockedThroughIncident=false;
                if(bObstructionAhead || bIncidentAhead)
                    for(const auto* Other:Traffic)
                    {
                        if(Other==Kart || Other->IsRaceFinished() || (!Other->IsBurning() && FMath::Abs(Other->GetSpeedKmh())>=14))continue;
                        const float Ahead=FVector::DotProduct(Other->GetActorLocation()-P,RoadDirection);
                        const float Side=FVector::DotProduct(Other->GetActorLocation()-RoutePoints[Near],RoadSide);
                        const float Width=HalfSpan(Kart,RoadSide)+HalfSpan(Other,RoadSide)+TrafficSafetyMargin+(Other->IsBurning()?80:0);
                        if(Ahead>0 && Ahead<FMath::Max(2000.f,ScanDistance)+600 && FMath::Abs(Candidate-Side)<Width)BlockedThroughIncident=true;
                    }
                if(BlockedThroughIncident)continue;
                float ReservationCost=0;
                if(bObstructionAhead || bIncidentAhead)
                    for(const auto& Peer:Opponents)
                    {
                        if(Peer.Kart.Get()==Kart || !Peer.Kart.IsValid() || Peer.Battle.IncidentReservation<=0)continue;
                        const float PeerAhead=FVector::DotProduct(Peer.Kart->GetActorLocation()-P,RoadDirection);
                        if(PeerAhead>0 && PeerAhead<1800 && FMath::Abs(Candidate-Peer.Battle.IncidentLane)<180)ReservationCost+=220;
                    }
                const float Score = Allowed - ReservationCost - FMath::Abs(Candidate - State.ActualLane) * .5f - FMath::Abs(Candidate - State.PreferredLane) * .25f - FMath::Max(0.f, FMath::Abs(Candidate) - 180);
                if (Score > BestScore) { BestScore = Score; BestLane = Candidate; }
            }
            if (FMath::Abs(BestLane - State.DesiredLane) > 20)
            {
                State.DesiredLane = BestLane;
                if(bObstructionAhead || bIncidentAhead){Battle.IncidentReservation=2.5f;Battle.IncidentLane=BestLane;Battle.IncidentTarget=Leader?Leader->GetKartNumber():0;}
                State.DecisionCooldown = State.DecisionSeconds;
                if (Leader && Leader->GetSpeedKmh()>14 && SafeSpeed(BestLane)>CurrentSafety+100 && State.ConstrainedSeconds>.3f+(1-State.Aggression)*.4f)
                {
                    State.PassTarget=Leader;State.PassPhase=EKartPassPhase::Committed;
                    State.PassSeconds=Racecraft::PassTimeout; ++State.PassAttempts; bCommitted=true;
                    if(Leader==Player && Battle.RepassSeconds>0) {++Battle.RepassAttempts;Battle.bRepassAttempt=true;}
                }
            }
            else State.DecisionCooldown = .6f + (1 - State.Aggression) * .6f;
        }
        // The planner can choose the passing lane before the consideration timer expires.
        // Promote that existing maneuver instead of requiring a second lane change to record intent.
        if(!bLaunching && !bCommitted && State.PassPhase!=EKartPassPhase::Clear && Leader && Leader->GetSpeedKmh()>14
            && bStraight && Battle.Defense!=EKartDefense::Holding && State.ConstrainedSeconds>.3f+(1-State.Aggression)*.4f
            && SafeSpeed(State.DesiredLane)>SafeSpeed(State.ActualLane)+100
            && FMath::Abs(State.DesiredLane-State.ActualLane)>40)
        {
            State.PassTarget=Leader;State.PassPhase=EKartPassPhase::Committed;State.PassSeconds=Racecraft::PassTimeout;
            ++State.PassAttempts;bCommitted=true;
            if(Leader==Player && Battle.RepassSeconds>0){++Battle.RepassAttempts;Battle.bRepassAttempt=true;}
        }
        // A recent rival is remembered, but a repass requires real speed advantage and a safe free corridor.
        if(!bLaunching && !bCommitted && !Battle.bAlongside && bStraight && Battle.RepassSeconds>0
            && Battle.PlayerAhead>250 && Battle.PlayerAhead<1100 && Battle.PlayerClosing < -60
            && Player->GetSpeedKmh()>14 && !Player->IsBurning() && !Player->IsRaceFinished())
        {
            float Best=State.DesiredLane, BestAllowed=CurrentSafety;
            for(float Candidate : {-300.f,300.f})
            {
                const float Allowed=SafeSpeed(Candidate);
                if(FMath::Abs(Candidate-Battle.PlayerLane)>200 && Allowed>BestAllowed+100) {Best=Candidate;BestAllowed=Allowed;}
            }
            if(FMath::Abs(Best-State.DesiredLane)>20)
            {
                State.DesiredLane=Best;State.PassTarget=Player;State.PassPhase=EKartPassPhase::Committed;
                State.PassSeconds=Racecraft::PassTimeout;State.DecisionCooldown=State.DecisionSeconds;
                ++State.PassAttempts;++Battle.RepassAttempts;Battle.bRepassAttempt=true;bCommitted=true;
            }
        }
        if (bGateApproach)
        {
            // Rejoin the validated corridor before its plane; never grant a missed checkpoint.
            const float GateLane = FMath::Clamp(State.DesiredLane, -Racecraft::LaneLimit, Racecraft::LaneLimit);
            bGateLaneClear = SafeSpeed(GateLane) >= 0;
            if (bGateLaneClear) State.DesiredLane = GateLane;
        }
        // Rebase an unsafe intermediate trajectory to measured occupancy. A safe final
        // corridor remains useful; discarding it as well can pin a queue behind an incident.
        const bool PlanUnsafe=SafeSpeed(State.DesiredLane)<0;
        if(PlanUnsafe || SafeSpeed(State.Lane)<0)
        {
            State.Lane=State.ActualLane;
            if(PlanUnsafe)
            {
                State.DesiredLane=State.ActualLane;State.DecisionCooldown=0;
                if(bCommitted)Battle.Outcome(EKartPassOutcome::ContactRisk);
                Battle.Conflict=TEXT("OccupiedCorridor");Battle.bYielding=true;Battle.bRepassAttempt=false;
                bCommitted=false;State.PassPhase=EKartPassPhase::Follow;State.PassTarget.Reset();
            }
            else Battle.Conflict=TEXT("RebasedIntermediateLane");
        }
        if (bDisplaced) State.Lane = State.ActualLane;
        const bool bPassingObstruction = !bLaunching && bStraight && bObstructionAhead && FMath::Abs(State.DesiredLane - State.ActualLane) > 60;
        State.Lane = FMath::FInterpConstantTo(State.Lane, State.DesiredLane, Dt,
            bPassingObstruction ? Racecraft::ObstructionLaneRate : bCommitted ? Racecraft::PassLaneRate : 70 + State.Aggression * 25);
        float Lane = State.Lane;
        float TargetSpeed = 55.f / .036f;
        // Anticipate curvature before reaching a bend. Limit lateral acceleration
        // and yaw demand to values the shared kart controller can actually turn.
        for (int32 Step = 2; Step <= 12; ++Step)
        {
            const FVector A = LanePoint(Near + Step - 1, Lane);
            const FVector B = LanePoint(Near + Step, Lane);
            const FVector C = LanePoint(Near + Step + 1, Lane);
            const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct((B - A).GetSafeNormal2D(), (C - B).GetSafeNormal2D()), -1., 1.));
            const float Radius = FVector::Dist2D(A, B) / FMath::Max(Angle, .0001);
            const float BendSpeed = FMath::Min(FMath::Sqrt(420.f * Radius), .7f * Radius) * State.CornerSkill;
            const float AllowedHere = FMath::Sqrt(BendSpeed * BendSpeed + 2 * 700.f * Spacing * FMath::Max(0, Step - 2));
            TargetSpeed = FMath::Min(TargetSpeed, AllowedHere);
        }
        // Keep a gap behind the kart ahead. Use the free alternate lane when
        // possible, rather than driving straight into a stopped racer.
        if(Kart->GetPhysicsBackend()==EKartPhysicsBackend::Custom)
            TargetSpeed=FMath::Min(TargetSpeed,float(KartPhysicsDriving::SpeedEnvelope(RoutePoints,Near,Lane,55./.036,State.CornerSkill)));
        if (bPaceDiagnostics) State.Pace.Corner = TargetSpeed;
        const float RawTrafficLimit = FMath::Max(0.f, SafeSpeed(Lane, true));
        const bool IncidentNow=bObstructionAhead || bIncidentAhead;
        if(IncidentNow)
        {
            if(!Battle.bIncidentActive){++Battle.IncidentEvents;Battle.IncidentSeconds=0;}
            Battle.IncidentSeconds+=Dt;
        }
        else if(Battle.bIncidentActive)
        {
            ++Battle.IncidentClears;Battle.IncidentClearSeconds+=Battle.IncidentSeconds;Battle.RecoveryClock=0;Battle.IncidentReservation=0;Battle.IncidentTarget=0;
        }
        Battle.bIncidentActive=IncidentNow;
        if(Battle.RecoveryClock>=0)
        {
            Battle.RecoveryClock+=Dt;
            if(Kart->GetSpeedKmh()>=49.5f){++Battle.SpeedRecoveries;Battle.SpeedRecoverySeconds+=Battle.RecoveryClock;Battle.RecoveryClock=-1;}
        }
        Battle.QueueSeconds=bQueueAhead && Speed<300 ? Battle.QueueSeconds+Dt : 0;
        Battle.LongestQueue=FMath::Max(Battle.LongestQueue,Battle.QueueSeconds);
        // Stabilize in the measured corridor and only merge when the original occupancy checks allow it.
        if(Battle.bRejoining)
        {
            if(SafeSpeed(State.DesiredLane)<0){State.Lane=State.DesiredLane=State.ActualLane;Battle.Conflict=TEXT("RejoinOccupied");}
            else if(Battle.RejoinSeconds<=0 && FMath::Abs(State.ActualLane-State.DesiredLane)<80)Battle.bRejoining=false;
        }
        // Reductions are immediate for safety; rising targets release smoothly and promptly.
        State.FilteredTrafficSpeed = RawTrafficLimit < State.FilteredTrafficSpeed ? RawTrafficLimit
            : FMath::FInterpConstantTo(State.FilteredTrafficSpeed, RawTrafficLimit, Dt, Racecraft::TrafficReleaseRate);
        const float TrafficLimit = State.FilteredTrafficSpeed;
        if (bPaceDiagnostics) State.Pace.Traffic = TrafficLimit;
        TargetSpeed = FMath::Min(TargetSpeed, TrafficLimit);
        if (bIncidentAhead) TargetSpeed = FMath::Min(TargetSpeed, 18.f / .036f);
        if (bPassingObstruction) TargetSpeed = FMath::Min(TargetSpeed, Racecraft::ObstructionSpeed);
        if (bPaceDiagnostics) State.Pace.Obstruction = bPassingObstruction ? Racecraft::ObstructionSpeed : bIncidentAhead ? 18.f/.036f : 55.f/.036f;
        const float BeforeGuidance = TargetSpeed;
        if (bGateApproach)
        {
            if (FMath::Abs(GateLocal.Y) > Gate.Extent.Y - OwnExtent.Y - TrafficSafetyMargin && GateLocal.X > -1400) TargetSpeed = FMath::Min(TargetSpeed, 400.f);
            if (!bGateLaneClear && FMath::Abs(GateLocal.Y) > Gate.Extent.Y - 100) TargetSpeed = FMath::Min(TargetSpeed,
                FMath::Sqrt(2 * Racecraft::PlanningBrakeDeceleration * FMath::Max(0.f, float(-GateLocal.X - 350))));
        }
        State.TargetSpeed = TargetSpeed;
        FVector Target = LanePoint(Near + (bPassingObstruction && Speed < 500 ? 1 : Look), Lane);
        if (bGateApproach && bGateLaneClear && (!bStraight || FMath::Abs(GateLocal.Y)>Gate.Extent.Y-OwnExtent.Y-TrafficSafetyMargin) && -GateLocal.X < Look * Spacing + 300)
            Target = Gate.Transform.TransformPosition(FVector(150, FMath::Clamp(State.Lane, -Racecraft::LaneLimit, Racecraft::LaneLimit), 0));
        const FVector LocalTarget = Kart->GetActorTransform().InverseTransformPosition(Target);
        const float Curvature = 2 * LocalTarget.Y / FMath::Max(1., LocalTarget.SizeSquared2D());
        if(Kart->GetPhysicsBackend()==EKartPhysicsBackend::Custom && FMath::Abs(Curvature)>1.e-7f)
            TargetSpeed=FMath::Min(TargetSpeed,FMath::Sqrt(350.f*FMath::Clamp(State.CornerSkill,.9f,1.1f)/FMath::Abs(Curvature)));
        const float Steering = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(Curvature * 104)) * (1 + Speed / 1400) / 22, -1.f, 1.f);
        if (LocalTarget.X < 100 || FMath::Abs(FMath::Atan2(LocalTarget.Y, LocalTarget.X)) > .65f) TargetSpeed = FMath::Min(TargetSpeed, 250.f);
        if (bPaceDiagnostics)
        {
            if (TargetSpeed < BeforeGuidance) State.Pace.Guidance = TargetSpeed;
            float Lowest = 55.f/.036f;
            auto Reason = [&](float Limit, const TCHAR* Name) { if (Limit < Lowest) { Lowest = Limit; State.Pace.Reason = Name; } };
            Reason(State.Pace.Corner, TEXT("Corner")); Reason(State.Pace.Traffic, TEXT("Traffic"));
            Reason(State.Pace.Obstruction, TEXT("Obstruction")); Reason(State.Pace.Guidance, TEXT("Guidance"));
        }
        State.bQueued = bQueueAhead;
        // A stationary obstruction can leave too little room to turn. This is not
        // a moving queue: allow the existing bounded reverse only after a sustained
        // stop, with every existing rear occupancy and static sweep check intact.
        const bool TrappedAtStaticObstacle = bObstructionAhead && Leader && FMath::Abs(Leader->GetSpeedKmh())<1
            && LeaderGap<(State.RecoverySeconds>0 ? 650.f : 400.f) && (Speed<30 || State.RecoverySeconds>0) && !bIncidentAhead;
        State.StalledSeconds = Speed < 30 && !bLaunching && ((!bQueueAhead && TargetSpeed > 100) || TrappedAtStaticObstacle) ? State.StalledSeconds + Dt : 0;
        auto RearClear = [&]()
        {
            const FVector Back = -Kart->GetActorForwardVector();
            for (const AKartPawn* Other : Traffic)
            {
                if (Other == Kart || Other->IsRaceFinished()) continue;
                const FVector Delta = Other->GetActorLocation() - P;
                const float Behind = FVector::DotProduct(Delta, Back);
                const float Side = FMath::Abs(FVector::DotProduct(Delta, Kart->GetActorRightVector()));
                const float Closing = FMath::Max(0.f, float(FVector::DotProduct(Other->GetVelocity(), Kart->GetActorForwardVector())));
                if (Behind > -OwnExtent.X && Behind < Racecraft::ReverseClearance + Closing * FMath::Max(Racecraft::ReverseDuration,State.RecoverySeconds)
                    && Side < OwnExtent.Y + HalfSpan(Other, Kart->GetActorRightVector()) + FMath::Clamp(TrafficSafetyMargin, 10.f, 100.f)) return false;
            }
            FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(KartReverseClearance), false, Kart);
            return !GetWorld()->SweepSingleByObjectType(Hit, P, P + Back * Racecraft::ReverseClearance, Kart->GetActorQuat(),
                FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(OwnExtent), Query);
        };
        if (State.StalledSeconds > (TrappedAtStaticObstacle ? 1.5f : Racecraft::StuckDelay) && State.RecoverySeconds <= 0 && State.RecoveryCooldown <= 0 && !bIncidentAhead && RearClear())
        {
            State.RecoverySeconds = TrappedAtStaticObstacle ? 2.f : Racecraft::ReverseDuration;
            State.StalledSeconds = 0;
        }
        if (State.RecoverySeconds > 0)
        {
            if (!RearClear() || (bQueueAhead && !TrappedAtStaticObstacle))
            {
                State.RecoverySeconds = 0;
                State.RecoveryCooldown = Racecraft::ReverseCooldown;
                Kart->SetDriveInput(0, 0, true);
                continue;
            }
            State.RecoverySeconds = FMath::Max(0.f, State.RecoverySeconds - Dt);
            State.RecoveryCooldown = Racecraft::ReverseCooldown;
            if (bPaceDiagnostics) { State.Pace.Recovery = 400; State.Pace.Reason = TEXT("Reverse"); }
            Kart->SetDriveInput(-1, TrappedAtStaticObstacle ? -FMath::Sign(Steering)*.8f : -Steering);
            continue;
        }
        State.TargetSpeed = TargetSpeed;
        const float Error = TargetSpeed-Speed;
        const bool Brake = Error < -(Kart->IsBrakeHeld() ? Racecraft::BrakeExitError : Racecraft::BrakeEnterError);
        const float Throttle = bLaunching ? (Speed < TargetSpeed-15 ? 1.f : 0.f)
            : Error>0 ? FMath::Clamp(Error/100.f,.08f,.96f+State.Aggression*.04f) : 0.f;
        Kart->SetDriveInput(Throttle, Steering, bLaunching ? Speed>TargetSpeed+15 : Brake);
    }
    CaptureBattleDebug(Dt);
}

double AKartRaceDirector::RaceDistance(const FVector& P, const FRaceProgress& Progress) const
{
    const int32 Near = NearestPoint(P);
    const FVector Tangent = (RoutePoints[(Near + 1) % RoutePoints.Num()] - RoutePoints[Near]).GetSafeNormal2D();
    const double SubPoint = FVector::DotProduct(P - RoutePoints[Near], Tangent) / FVector::Dist2D(RoutePoints[Near], RoutePoints[(Near + 1) % RoutePoints.Num()]);
    double Fraction = (Near + SubPoint) / RoutePoints.Num();
    // The nearest sample wraps to zero just before the finish plane. Keep that
    // small negative projection at the end of a validated lap for standings.
    if (!Progress.bStarted) return Fraction > .5 ? Fraction - 1 : Fraction;
    if (Progress.NextCheckpoint == Progress.CheckpointCount && Fraction < .1) Fraction += 1;
    return Progress.CompletedLaps + Fraction;
}

int32 AKartRaceDirector::GetPlayerPlace() const
{
    for (const auto& Row : GetStandings()) if (Row.bPlayer) return Row.Position;
    return 20;
}

TArray<FKartStanding> AKartRaceDirector::GetStandings() const
{
    TArray<FKartStanding> Rows;
    const AKartPawn* Player = Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    const URaceLoopSubsystem* Race = GetWorld()->GetSubsystem<URaceLoopSubsystem>();
    if (!Player || !Race || RoutePoints.IsEmpty()) return Rows;
    auto Add = [&](const AKartPawn* Kart, const FRaceProgress& Progress, int32 Finish, bool bPlayer)
    {
        FKartStanding Row;
        Row.Number = Kart->GetKartNumber(); Row.Name = Kart->GetDriverName();
        Row.Lap = Progress.CurrentLap();
        Row.SpeedKmh = FMath::Abs(Kart->GetSpeedKmh());
        Row.bPlayer = bPlayer; Row.bIncident = Kart->IsBurning();
        Row.FinishPlace = Finish; Row.Color = Kart->GetSuitColor();
        Row.Distance = RaceDistance(Kart->GetActorLocation(), Progress);
        Rows.Add(Row);
    };
    Add(Player, Race->GetProgress(), PlayerFinishPlace, true);
    for (const auto& State : Opponents)
        if (const AKartPawn* Kart = State.Kart.Get())
            Add(Kart, State.Progress, State.FinishPlace, false);
    Rows.Sort([this](const FKartStanding& A, const FKartStanding& B)
    {
        if (!bReleased) return A.Number < B.Number;
        if (A.FinishPlace || B.FinishPlace)
            return A.FinishPlace && (!B.FinishPlace || A.FinishPlace < B.FinishPlace);
        if (!FMath::IsNearlyEqual(A.Distance, B.Distance, .0001)) return A.Distance > B.Distance;
        return A.Number < B.Number;
    });
    for (int32 Index = 0; Index < Rows.Num(); ++Index) Rows[Index].Position = Index + 1;
    return Rows;
}

double AKartRaceDirector::DistanceToLapFinishCm(const FVector& P, const FRaceProgress& Progress) const
{
    if (Progress.bFinished || RoutePoints.Num() < 2) return 0;
    // Project onto the centreline, then sum forward segments to start/finish.
    // This is route distance, not a straight-line shortcut across the infield.
    const int32 Near = NearestPoint(P);
    double Remaining = 0;
    for (int32 I = Near; I < RoutePoints.Num(); ++I)
        Remaining += FVector::Dist2D(RoutePoints[I], RoutePoints[(I + 1) % RoutePoints.Num()]);
    const FVector Direction = (RoutePoints[(Near + 1) % RoutePoints.Num()] - RoutePoints[Near]).GetSafeNormal2D();
    Remaining -= FVector::DotProduct(P - RoutePoints[Near], Direction);
    if (Near == 0 && (!Progress.bStarted || Progress.NextCheckpoint == Progress.CheckpointCount))
        return FMath::Max(0., -FVector::DotProduct(P - RoutePoints[0], Direction));
    return FMath::Max(0., Remaining);
}

bool AKartRaceDirector::IsCommentaryPlaying() const { return CommentaryAudio->IsPlaying(); }
FString AKartRaceDirector::GetCommentaryCaption() const
{
    return GetWorld()->GetTimeSeconds() < CaptionUntil ? CommentaryCaption : FString();
}
void AKartRaceDirector::Announce(const TCHAR* Cue, const FString& Caption)
{
    const double Now = GetWorld()->GetTimeSeconds();
    CommentaryCaption = Caption; CaptionUntil = Now + 5;
    CommentaryCooldownUntil = Now + 8;
    CommentaryAudio->Stop();
    if (const auto* Clip = CommentaryClips.Find(FName(Cue)); Clip && *Clip)
    {
        CommentaryAudio->SetSound(*Clip); CommentaryAudio->Play();
    }
}
void AKartRaceDirector::UpdateCommentary()
{
    const URaceLoopSubsystem* Race = GetWorld()->GetSubsystem<URaceLoopSubsystem>();
    const AKartPawn* Player = Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Race || !Player) return;
    const auto& P = Race->GetProgress();
    const int32 Place = GetPlayerPlace();
    const double Now = GetWorld()->GetTimeSeconds();
    if (P.bFinished && !bAnnouncedFinish) { Announce(TEXT("Finish"), TEXT("Across the line! The race is complete.")); bAnnouncedFinish = true; }
    else if (!P.bFinished && Player->IsBurning() && !bPreviousFire && Now >= CommentaryCooldownUntil)
        Announce(TEXT("Incident"), TEXT("Contact on track! Give the recovering karts room."));
    else if (P.CurrentLap() > AnnouncedLap)
        Announce(P.CurrentLap() == 1 ? TEXT("Start") : TEXT("Lap"), P.CurrentLap() == 1 ? TEXT("We're racing at South Garda! Keep it clean into the first corner.") : FString::Printf(TEXT("Lap %d of %d. Keep the momentum going!"), P.CurrentLap(), P.TargetLaps));
    else if (!P.bFinished && P.bStarted && Place < PreviousPlayerPlace && Now >= CommentaryCooldownUntil)
        Announce(TEXT("Overtake"), TEXT("A position gained! Keep looking ahead."));
    AnnouncedLap = P.CurrentLap(); PreviousPlayerPlace = Place; bPreviousFire = Player->IsBurning();
}

void AKartRaceDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    FlushBattleDebug();
    Super::EndPlay(Reason);
}
void AKartRaceDirector::FlushBattleDebug()
{
#if !UE_BUILD_SHIPPING
    if (!BattleBuffer.IsEmpty() && !BattleCapturePath.IsEmpty())
    {
        if (FFileHelper::SaveStringToFile(BattleBuffer, *BattleCapturePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(), FILEWRITE_Append)) BattleBuffer.Empty();
    }
#endif
}
void AKartRaceDirector::CaptureBattleDebug(float Dt)
{
#if !UE_BUILD_SHIPPING
    const bool Enabled=FParse::Param(FCommandLine::Get(), TEXT("RaceBattleDebug")) || CVarBattleDebug.GetValueOnGameThread()!=0;
    if(!TrainingRecorder->IsRecording()){if(bBattleCapture)FlushBattleDebug();bBattleCapture=false;return;}
    const FString Path=TrainingRecorder->GetCapturePath()+TEXT(".battle-v2.csv");
    if(Path!=BattleCapturePath)
    {
        FlushBattleDebug();BattleCapturePath=Path;BattleElapsed=BattleSampleClock=0;bBattleCapture=false;
    }
    BattleElapsed+=Dt;
    if(!Enabled){if(bBattleCapture)FlushBattleDebug();bBattleCapture=false;return;}
    if(!bBattleCapture)
    {
        bBattleCapture=true;
        if(!IFileManager::Get().FileExists(*Path))
        BattleBuffer=TEXT("schema,time,player_position,player_contacts,npc,distance_cm,ahead_cm,side_cm,closing_cms,player_lane,actual_lane,desired_lane,speed_kmh,target_cms,throttle,brake,corner,traffic,guidance,obstruction,reason,pass,defend,alongside,repass_seconds,losses,defends,attempts,completed,repass_attempts,repass_completed,contacts,conflict,rival,yielding,recovery,pass_seconds,completed_laps,player_laps,outcome_completed,outcome_occupied,outcome_corner,outcome_guidance,outcome_speed_loss,outcome_timeout,outcome_contact_risk,outcome_rival_unavailable,defense_outcome,defense_eligible,defense_gap,defense_closing,defense_geometry,defense_elapsed,defense_move_delay,defense_movement,defense_start_speed,defense_min_speed,defense_lane,incident,rejoining,reservation_lane,reservation_seconds,incident_events,secondary_contacts,longest_queue,defense_held,defense_late,defense_alongside,defense_occupied,defense_cleared,defense_lost_speed,defense_wide,defense_contact_risk,burning\n");
    }
    BattleSampleClock+=Dt;
    if(BattleSampleClock<.2f) return;
    BattleSampleClock=FMath::Fmod(BattleSampleClock,.2f);
    const auto* Player=Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(),0));
    const int32 Place=GetPlayerPlace();
    for(int32 I=0;I<Opponents.Num();++I)
    {
        const auto& S=Opponents[I]; const auto& B=S.Battle; const auto* K=S.Kart.Get();
        if(!K)continue;
        BattleBuffer+=FString::Printf(TEXT("2,%.3f,%d,%d,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.1f,%.3f,%d,%.1f,%.1f,%.1f,%.1f,%s,%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%.1f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"),
            BattleElapsed,Place,Player?Player->GetCollisionCount():0,I+1,B.PlayerDistance,B.PlayerAhead,B.PlayerSide,B.PlayerClosing,B.PlayerLane,S.ActualLane,S.DesiredLane,
            K->GetSpeedKmh(),S.TargetSpeed,K->GetThrottleInput(),K->IsBrakeHeld(),S.Pace.Corner,S.Pace.Traffic,S.Pace.Guidance,S.Pace.Obstruction,S.Pace.Reason,
            int(S.PassPhase),int(B.Defense),B.bAlongside,B.RepassSeconds,B.PositionsLost,B.Defenses,S.PassAttempts,S.CompletedPasses,B.RepassAttempts,B.RepassCompletions,K->GetCollisionCount(),B.Conflict,S.PassTarget.IsValid()?S.PassTarget->GetKartNumber():(B.Defense==EKartDefense::Holding?B.DefenseRival:0),B.bYielding,S.Pace.Recovery,S.PassSeconds,S.Progress.CompletedLaps,GetWorld()->GetSubsystem<URaceLoopSubsystem>()->GetProgress().CompletedLaps,B.Outcomes[0],B.Outcomes[1],B.Outcomes[2],B.Outcomes[3],B.Outcomes[4],B.Outcomes[5],B.Outcomes[6],B.Outcomes[7]);
        BattleBuffer.RemoveAt(BattleBuffer.Len()-1);
        BattleBuffer+=FString::Printf(TEXT(",%d,%d,%.1f,%.1f,%.4f,%.2f,%.2f,%.1f,%.2f,%.2f,%.1f,%d,%d,%.1f,%.2f,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"),
            int(B.DefenseOutcome),B.bDefenseEligible,B.DefenseGap,B.DefenseClosing,B.DefenseGeometry,B.DefenseElapsed,B.DefenseMoveDelay,B.DefenseMovement,B.DefenseStartSpeed,B.DefenseMinimumSpeed,B.DefenseLane,
            B.bIncidentActive,B.bRejoining,B.IncidentLane,B.IncidentReservation,B.IncidentEvents,B.SecondaryContacts,B.LongestQueue,
            B.DefenseOutcomes[1],B.DefenseOutcomes[2],B.DefenseOutcomes[3],B.DefenseOutcomes[4],B.DefenseOutcomes[5],B.DefenseOutcomes[6],B.DefenseOutcomes[7],B.DefenseOutcomes[8],K->IsBurning());
    }
    if(BattleBuffer.Len()>65536)FlushBattleDebug();
#endif
}
