#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceHUDVisibility.h"
#include "RaceLoopWidget.generated.h"

class UTextBlock;

UCLASS()
class ROADTOF1_API URaceLoopWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ResetPresentation();
    void RefreshRaceData(const FGeometry& Geometry);
    void UpdatePresentation(double Now, bool bStarted, int32 Place, bool bTab, bool bFinished);
    float GetLeaderboardOpacity() const { return Presentation.LeaderAlpha; }
    float GetBriefingOpacity() const { return Presentation.BriefAlpha; }
    int32 GetVisibleStandingCount() const;
    int32 GetPresentationResetSerial() const { return PresentationResetSerial; }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    void InitializeKartHUD();
    void InitializePrototypeHUD();
    void RefreshStandings(bool bFull);
    FRaceHUDVisibility Presentation;
    bool bKartHUD = false, bFullStandings = false;
    double LastStandingsUpdate = -1;
    int32 CachedPlace = 20, PresentationResetSerial = 0;
    TWeakObjectPtr<class AKartRaceDirector> CachedDirector;
    UPROPERTY(Transient) TObjectPtr<class UBorder> LeaderPanel;
    UPROPERTY(Transient) TObjectPtr<class UBorder> BriefPanel;
    UPROPERTY(Transient) TObjectPtr<class UBorder> PermanentPanel;
    UPROPERTY(Transient) TObjectPtr<class UBorder> SpeedPanel;
    UPROPERTY(Transient) TObjectPtr<class USizeBox> LeaderWidth;
    UPROPERTY(Transient) TObjectPtr<class USizeBox> BriefWidth;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> BriefLap;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StandingHint;
    UTextBlock* AddLine(class UVerticalBox* Box, const FString& Text, int32 FontSize);
    UFUNCTION()
    void OnRestartClicked();
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Lap;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Checkpoints;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LapTimer;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RaceTimer;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LastLap;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Speed;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeedUnit;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Position;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TrainingStatus;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TowerLap;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TowerDistance;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Commentary;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> StandingLaps;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> StandingNames;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> StandingNumbers;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> StandingSpeeds;
    UPROPERTY(Transient) TArray<TObjectPtr<class UBorder>> StandingRows;
};
