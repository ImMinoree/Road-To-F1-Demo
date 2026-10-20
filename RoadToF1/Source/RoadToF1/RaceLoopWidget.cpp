#include "RaceLoopWidget.h"
#include "RaceLoopSubsystem.h"
#include "KartPawn.h"
#include "KartRaceDirector.h"
#include "KartTrainingRecorder.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/World.h"
#include "Components/SizeBox.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
FString FormatRaceTime(double Seconds)
{
    const int64 Milliseconds = FMath::FloorToInt64(Seconds * 1000.0);
    return FString::Printf(TEXT("%02lld:%02lld.%03lld"), Milliseconds / 60000, (Milliseconds / 1000) % 60, Milliseconds % 1000);
}
}

UTextBlock* URaceLoopWidget::AddLine(UVerticalBox* Box, const FString& Text, int32 FontSize)
{
    UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
    Line->SetText(FText::FromString(Text));
    FSlateFontInfo Font = Line->GetFont();
    Font.Size = FontSize;
    if (FontSize >= 18) Font.TypefaceFontName = TEXT("Bold");
    Line->SetFont(Font);
    Line->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Line->SetAutoWrapText(true);
    Box->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 3));
    return Line;
}


void URaceLoopWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bKartHUD = GetWorld()->GetMapName().EndsWith(TEXT("SouthGarda_KartRace"));
    if (bKartHUD) InitializeKartHUD(); else InitializePrototypeHUD();
    ResetPresentation();
}

void URaceLoopWidget::InitializePrototypeHUD()
{
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(.015f, .025f, .045f, .92f)); Panel->SetPadding(FMargin(18));
    WidgetTree->RootWidget = Panel;
    UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(Lines);
    AddLine(Lines, TEXT("ROAD TO F1 | RACE PROTOTYPE"), 18);
    Status = AddLine(Lines, TEXT("Cross START to begin"), 20);
    Lap = AddLine(Lines, TEXT("Lap 0 / 3"), 24);
    Checkpoints = AddLine(Lines, TEXT("Checkpoints 0 / 3"), 18);
    LapTimer = AddLine(Lines, TEXT("Lap 00:00.000"), 22);
    RaceTimer = AddLine(Lines, TEXT("Total 00:00.000"), 22);
    LastLap = AddLine(Lines, TEXT("Last lap --:--.---"), 18);
    AddLine(Lines, TEXT("Pass checkpoints in order. Follow the green gate."), 14);
    UButton* Restart = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(TEXT("Restart race [F5]")));
    Label->SetColorAndOpacity(FLinearColor::Black);
    Restart->SetContent(Label); Restart->OnClicked.AddDynamic(this, &URaceLoopWidget::OnRestartClicked);
    Lines->AddChildToVerticalBox(Restart);
}

void URaceLoopWidget::InitializeKartHUD()
{
    const FLinearColor Accent(.22f, .92f, .73f, 1);
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto Panel = [&](FName Name, float Width, FAnchors Anchors, FVector2D Alignment, FVector2D Offset, USizeBox*& Size, UVerticalBox*& Lines)
    {
        Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(Width);
        auto* Slot = Canvas->AddChildToCanvas(Size); Slot->SetAnchors(Anchors); Slot->SetAlignment(Alignment); Slot->SetPosition(Offset); Slot->SetAutoSize(true);
        UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
        Border->SetBrushColor(FLinearColor(.025f, .035f, .05f, .82f)); Border->SetPadding(FMargin(12, 10)); Size->SetContent(Border);
        Lines = WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Lines);
        USizeBox* RuleSize = WidgetTree->ConstructWidget<USizeBox>(); RuleSize->SetHeightOverride(2);
        UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(); Rule->SetBrushColor(Accent); RuleSize->SetContent(Rule);
        Lines->AddChildToVerticalBox(RuleSize)->SetPadding(FMargin(0, 0, 0, 7));
        return Border;
    };
    USizeBox* Width = nullptr; UVerticalBox* Lines = nullptr;
    LeaderPanel = Panel(TEXT("Leaderboard"), 284, FAnchors(0, 0), FVector2D(0, 0), FVector2D(24, 24), Width, Lines); LeaderWidth = Width;
    AddLine(Lines, TEXT("CLASSIFICATION"), 13)->SetColorAndOpacity(Accent);
    TowerLap = AddLine(Lines, TEXT("LAP 0 / 3"), 18);
    UBorder* ColumnHeader = WidgetTree->ConstructWidget<UBorder>(); ColumnHeader->SetBrushColor(FLinearColor::Transparent); ColumnHeader->SetPadding(FMargin(4, 0));
    UHorizontalBox* HeaderCells = WidgetTree->ConstructWidget<UHorizontalBox>(); ColumnHeader->SetContent(HeaderCells); Lines->AddChildToVerticalBox(ColumnHeader);
    auto HeaderCell = [&](const TCHAR* Label, float Width, bool bFill)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(); Text->SetText(FText::FromString(Label));
        auto Font = Text->GetFont(); Font.Size = 9; Text->SetFont(Font); Text->SetColorAndOpacity(FLinearColor(.62f, .69f, .75f));
        auto* Slot = HeaderCells->AddChildToHorizontalBox(Text); Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
        if (!bFill) Text->SetMinDesiredWidth(Width);
    };
    HeaderCell(TEXT("POS"), 32, false); HeaderCell(TEXT("DRIVER"), 0, true); HeaderCell(TEXT("LAP"), 30, false); HeaderCell(TEXT("MPH"), 38, false);
    for (int32 I = 0; I < 20; ++I)
    {
        UBorder* Row = WidgetTree->ConstructWidget<UBorder>(); Row->SetPadding(FMargin(4, 4));
        UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>(); Row->SetContent(Cells);
        auto Cell = [&](float MinWidth, bool bFill, int32 FontSize)
        {
            auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
            auto Font = Text->GetFont(); Font.Size = FontSize; Text->SetFont(Font); Text->SetColorAndOpacity(FLinearColor::White);
            auto* CellSlot = Cells->AddChildToHorizontalBox(Text);
            CellSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
            if (!bFill) Text->SetMinDesiredWidth(MinWidth);
            return Text;
        };
        StandingNumbers.Add(Cell(32, false, 12)); StandingNames.Add(Cell(0, true, 12));
        StandingLaps.Add(Cell(30, false, 11)); StandingSpeeds.Add(Cell(38, false, 11));
        StandingRows.Add(Row); Lines->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 1));
    }
    TowerDistance = AddLine(Lines, TEXT("TO LAP LINE: -- ft"), 10);
    StandingHint = AddLine(Lines, TEXT("HOLD TAB  /  FULL FIELD"), 10); StandingHint->SetColorAndOpacity(FLinearColor(.62f, .69f, .75f));

    BriefPanel = Panel(TEXT("RaceBriefing"), 300, FAnchors(1, 0), FVector2D(1, 0), FVector2D(-24, 24), Width, Lines); BriefWidth = Width;
    AddLine(Lines, TEXT("ROAD TO F1"), 22);
    AddLine(Lines, TEXT("SOUTH GARDA RACING"), 12)->SetColorAndOpacity(Accent);
    BriefLap = AddLine(Lines, TEXT("3 LAPS  /  GRID 20 OF 20"), 14);
    AddLine(Lines, TEXT("W / S     Accelerate / brake & reverse\nA / D     Steer\nSPACE   Brake"), 12);
    AddLine(Lines, TEXT("F5  Restart     TAB  Standings"), 12);
    AddLine(Lines, TEXT("Follow the green checkpoint."), 11)->SetColorAndOpacity(FLinearColor(.7f, .77f, .81f));

    // Separate bottom corners leave the central kart and racing line unobstructed.
    PermanentPanel = Panel(TEXT("PermanentRaceStats"), 460, FAnchors(0, 1), FVector2D(0, 1), FVector2D(24, -24), Width, Lines);
    Status = AddLine(Lines, TEXT("READY  /  CROSS START"), 10); Status->SetColorAndOpacity(Accent);
    UHorizontalBox* Stats = WidgetTree->ConstructWidget<UHorizontalBox>(); Lines->AddChildToVerticalBox(Stats);
    auto Metric = [&](const TCHAR* Title, const TCHAR* Value)
    {
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        Stats->AddChildToHorizontalBox(Column)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        AddLine(Column, Title, 10)->SetColorAndOpacity(FLinearColor(.62f, .69f, .75f));
        return AddLine(Column, Value, 23);
    };
    Position = Metric(TEXT("POSITION"), TEXT("20 / 20"));
    Lap = Metric(TEXT("LAP"), TEXT("0 / 3"));
    LapTimer = Metric(TEXT("LAP TIME"), TEXT("00:00.000"));
    SpeedPanel = Panel(TEXT("PermanentSpeed"), 126, FAnchors(1, 1), FVector2D(1, 1), FVector2D(-24, -24), Width, Lines);
    Speed = AddLine(Lines, TEXT("0.0"), 30); SpeedUnit=AddLine(Lines, TEXT("mph / LEGACY"), 10);SpeedUnit->SetColorAndOpacity(Accent);

    USizeBox* CaptionWidth = WidgetTree->ConstructWidget<USizeBox>(); CaptionWidth->SetWidthOverride(480);
    auto* CaptionSlot = Canvas->AddChildToCanvas(CaptionWidth); CaptionSlot->SetAnchors(FAnchors(.5f, 0)); CaptionSlot->SetAlignment(FVector2D(.5f, 0)); CaptionSlot->SetPosition(FVector2D(0, 24)); CaptionSlot->SetAutoSize(true);
    UVerticalBox* Captions = WidgetTree->ConstructWidget<UVerticalBox>(); CaptionWidth->SetContent(Captions);
    Commentary = AddLine(Captions, TEXT(""), 14); Commentary->SetJustification(ETextJustify::Center);
    TrainingStatus = AddLine(Captions, TEXT(""), 10); TrainingStatus->SetJustification(ETextJustify::Center);
}

void URaceLoopWidget::ResetPresentation()
{
    ++PresentationResetSerial;
    Presentation.Reset(GetWorld()->GetTimeSeconds());
    LastStandingsUpdate = -1; bFullStandings = false; CachedPlace = 20;
    if (LeaderPanel) { LeaderPanel->SetRenderOpacity(0); LeaderPanel->SetVisibility(ESlateVisibility::HitTestInvisible); }
    if (BriefPanel) { BriefPanel->SetRenderOpacity(0); BriefPanel->SetVisibility(ESlateVisibility::HitTestInvisible); }
}

void URaceLoopWidget::UpdatePresentation(double Now, bool bStarted, int32 Place, bool bTab, bool bFinished)
{
    Presentation.Update(Now, bStarted, Place, bTab, bFinished);
    // Native UMG opacity envelopes share one game clock; no competing latent animations/timers.
    if (LeaderPanel)
    {
        LeaderPanel->SetRenderOpacity(Presentation.LeaderAlpha);
        LeaderPanel->SetVisibility(Presentation.LeaderAlpha > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (BriefPanel)
    {
        BriefPanel->SetRenderOpacity(Presentation.BriefAlpha);
        BriefPanel->SetVisibility(Presentation.BriefAlpha > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (bFullStandings != bTab) { bFullStandings = bTab; LastStandingsUpdate = -1; }
}

int32 URaceLoopWidget::GetVisibleStandingCount() const
{
    int32 Count = 0;
    for (const auto& Row : StandingRows) if (Row->GetVisibility() != ESlateVisibility::Collapsed) ++Count;
    return Count;
}

void URaceLoopWidget::RefreshStandings(bool bFull)
{
    if (!CachedDirector.IsValid()) return;
    const auto Rows = CachedDirector->GetStandings();
    for (const auto& Row : Rows) if (Row.bPlayer) CachedPlace = Row.Position;
    const int32 First = FRaceHUDVisibility::FirstRow(CachedPlace, Rows.Num(), bFull);
    const int32 Count = bFull ? Rows.Num() : FMath::Min(7, Rows.Num());
    for (int32 I = 0; I < StandingRows.Num(); ++I)
    {
        const bool bShow = I >= First && I < First + Count && Rows.IsValidIndex(I);
        StandingRows[I]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShow) continue;
        const auto& Row = Rows[I];
        StandingNumbers[I]->SetText(FText::AsNumber(Row.Position));
        FString FirstName, LastName;
        Row.Name.Split(TEXT(" "), &FirstName, &LastName);
        const FString Name = Row.bPlayer ? TEXT("YOU") : LastName.IsEmpty() ? Row.Name : FirstName.Left(1) + TEXT(". ") + LastName;
        StandingNames[I]->SetText(FText::FromString(FString::Printf(TEXT("%s  #%02d"), *Name, Row.Number)));
        StandingLaps[I]->SetText(FText::AsNumber(Row.Lap));
        StandingSpeeds[I]->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Row.SpeedKmh * .621371f)));
        StandingRows[I]->SetBrushColor(Row.bPlayer ? FLinearColor(.06f, .35f, .28f, .95f) : Row.bIncident ? FLinearColor(.30f, .08f, .025f, .8f) : FLinearColor(.065f, .085f, .105f, .45f));
    }
    if (StandingHint) StandingHint->SetText(FText::FromString(bFull ? TEXT("FULL FIELD  /  RELEASE TAB") : TEXT("HOLD TAB  /  FULL FIELD")));
}

void URaceLoopWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshRaceData(Geometry);
}

void URaceLoopWidget::RefreshRaceData(const FGeometry& Geometry)
{
    const URaceLoopSubsystem* Race = GetWorld()->GetSubsystem<URaceLoopSubsystem>();
    if (!Race || !Status) return;
    const FRaceProgress& P = Race->GetProgress(); const double Now = GetWorld()->GetTimeSeconds();
    if (!bKartHUD)
    {
        Status->SetText(FText::FromString(!Race->IsRaceAvailable() ? Race->GetStatusMessage() : P.bFinished ? TEXT("FINISHED - F5 to race again") : P.bStarted ? TEXT("RACING") : TEXT("Cross START to begin")));
        Lap->SetText(FText::FromString(FString::Printf(TEXT("Lap %d / %d"), P.CurrentLap(), P.TargetLaps)));
        Checkpoints->SetText(FText::FromString(P.bFinished ? TEXT("All laps complete") : FString::Printf(TEXT("Checkpoints %d / %d | Next: %s"), P.NextCheckpoint, P.CheckpointCount, P.bStarted && P.NextCheckpoint < P.CheckpointCount ? *FString::Printf(TEXT("CP%d"), P.NextCheckpoint + 1) : TEXT("START / FINISH"))));
        LapTimer->SetText(FText::FromString(TEXT("Lap ") + FormatRaceTime(P.LapTime(Now))));
        RaceTimer->SetText(FText::FromString(TEXT("Total ") + FormatRaceTime(P.TotalTime(Now))));
        LastLap->SetText(FText::FromString(TEXT("Last lap ") + (P.LapTimes.IsEmpty() ? FString(TEXT("--:--.---")) : FormatRaceTime(P.LapTimes.Last()))));
        return;
    }
    if (!CachedDirector.IsValid()) CachedDirector = Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AKartRaceDirector::StaticClass()));
    const APlayerController* PC = GetOwningPlayer();
    const bool bTab = PC && PC->IsInputKeyDown(EKeys::Tab);
    UpdatePresentation(Now, P.bStarted, CachedPlace, bTab, P.bFinished);
    if (LastStandingsUpdate < 0 || Now - LastStandingsUpdate >= .05)
    {
        RefreshStandings(bTab); LastStandingsUpdate = Now;
        // Apply a position-change trigger immediately after the refreshed ordering.
        UpdatePresentation(Now, P.bStarted, CachedPlace, bTab, P.bFinished);
    }
    if (PermanentPanel) { PermanentPanel->SetRenderTransformPivot(FVector2D(0, 1)); PermanentPanel->SetRenderScale(FVector2D(FMath::Clamp(float(Geometry.GetLocalSize().X) / 1100.f, .6f, 1.f))); }
    if (SpeedPanel) { SpeedPanel->SetRenderTransformPivot(FVector2D(1, 1)); SpeedPanel->SetRenderScale(FVector2D(FMath::Clamp(float(Geometry.GetLocalSize().X) / 1000.f, .8f, 1.f))); }
    if (LeaderWidth) LeaderWidth->SetWidthOverride(FMath::Clamp(float(Geometry.GetLocalSize().X) * .24f, 254.f, 284.f));
    if (BriefWidth) BriefWidth->SetWidthOverride(FMath::Clamp(float(Geometry.GetLocalSize().X) * .25f, 260.f, 300.f));
    Position->SetText(FText::FromString(FString::Printf(TEXT("%d / 20"), CachedPlace)));
    Lap->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), P.CurrentLap(), P.TargetLaps)));
    LapTimer->SetText(FText::FromString(FormatRaceTime(P.LapTime(Now))));
    BriefLap->SetText(FText::FromString(FString::Printf(TEXT("LAP %d / %d\nSTART GRID 20 / 20"), P.CurrentLap(), P.TargetLaps)));
    TowerLap->SetText(FText::FromString(FString::Printf(TEXT("LAP %d / %d"), P.CurrentLap(), P.TargetLaps)));
    const AKartPawn* Kart = Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (Kart)
    {
        Speed->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Abs(Kart->GetSpeedKmh()) * .621371f)));
        if(SpeedUnit)SpeedUnit->SetText(FText::FromString(Kart->GetPhysicsBackend()==EKartPhysicsBackend::Custom?TEXT("mph / CUSTOM"):TEXT("mph / LEGACY")));
    }
    FString State = P.bFinished ? TEXT("FINISHED  /  TOTAL ") + FormatRaceTime(P.TotalTime(Now)) : !P.bStarted ? TEXT("READY  /  CROSS START") : FString::Printf(TEXT("NEXT %s  /  F5 RESTART"), P.NextCheckpoint < P.CheckpointCount ? *FString::Printf(TEXT("CP%d"), P.NextCheckpoint + 1) : TEXT("LAP LINE"));
    if (!P.bFinished && Kart && Kart->IsBurning()) State = TEXT("RECOVERING  /  GIVE TRAFFIC ROOM");
    Status->SetText(FText::FromString(!Race->IsRaceAvailable() ? Race->GetStatusMessage() : State));
    if (CachedDirector.IsValid())
    {
        Commentary->SetText(FText::FromString(CachedDirector->GetCommentaryCaption()));
        if (Kart) TowerDistance->SetText(FText::FromString(P.bFinished ? TEXT("RACE COMPLETE") : FString::Printf(TEXT("TO LAP LINE: %.0f ft"), CachedDirector->DistanceToLapFinishCm(Kart->GetActorLocation(), P) / 30.48)));
        const UKartTrainingRecorder* Recorder = CachedDirector->GetTrainingRecorder();
        TrainingStatus->SetText(FText::FromString(Recorder && Recorder->IsRecording() ? TEXT("DRIVE RECORDING  /  F9 STOP") : TEXT("")));
    }
}

void URaceLoopWidget::OnRestartClicked()
{
    if (URaceLoopSubsystem* Race = GetWorld()->GetSubsystem<URaceLoopSubsystem>()) Race->RestartRace();
}
