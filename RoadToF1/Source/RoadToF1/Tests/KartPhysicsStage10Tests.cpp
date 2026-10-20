#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPhysicsEvaluationDriver.h"
#include "KartRaceDirector.h"
#include "RaceLoopSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "RaceLoopWidget.h"

namespace
{
bool SavePhysicsRace(const FString& Name,const TSharedRef<FJsonObject>& Root)
{
    const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../TestResults/PhysicsStage10/Runs"));
    IFileManager::Get().MakeDirectory(*Folder,true);FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    const FString Path=Folder/(Name+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".json"));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if(Saved){UE_LOG(LogTemp,Display,TEXT("PHYSICS_RACE_REPORT %s"),*Path);}
    else {UE_LOG(LogTemp,Error,TEXT("Physics race report could not be saved: %s"),*Path);}
    return Saved;
}
struct FDriverRun
{
    double Moving=0,SpeedSum=0,Peak=0,Below20=0,Below25=0,Above30=0,Traffic=0,Corner=0,Obstruction=0,Finish=-1;
    int BrakeEvents=0,Contacts=0,PassAttempts=0,Passes=0,Lap=0,CP=0;bool Braking=false;FVector Position;FString LastControl;
    void Sample(const FKartOpponentState& E,double Dt,double Now)
    {
        const auto* K=E.Kart.Get();Contacts=K->GetCollisionCount();Lap=E.Progress.CompletedLaps;CP=E.Progress.NextCheckpoint;Position=K->GetActorLocation();PassAttempts=E.PassAttempts;Passes=E.CompletedPasses;
        LastControl=FString::Printf(TEXT("yaw%.9g pitch%.9g speed%.9g throttle%.9g steer%.9g brake%d target%.9g reason%s recovery%.9g stalled%.9g lane%.9g desired%.9g"),K->GetActorRotation().Yaw,K->GetActorRotation().Pitch,K->GetSpeedKmh(),K->GetThrottleInput(),K->GetSteeringInput(),K->IsBrakeHeld(),E.TargetSpeed,E.Pace.Reason,E.RecoverySeconds,E.StalledSeconds,E.ActualLane,E.DesiredLane);
        if(E.Progress.bFinished){Finish=E.Progress.TotalTime(Now);return;}
        const double Mph=K->GetVelocity().Size2D()*.0223693629;Peak=FMath::Max(Peak,Mph);
        if(Mph>=1){Moving+=Dt;SpeedSum+=Mph*Dt;Below20+=Mph<20?Dt:0;Below25+=Mph<25?Dt:0;Above30+=Mph>30?Dt:0;}
        if(K->IsBrakeHeld() && !Braking)++BrakeEvents;Braking=K->IsBrakeHeld();
        const FString Reason=E.Pace.Reason;Traffic+=Reason==TEXT("Traffic")?Dt:0;Corner+=Reason==TEXT("Corner")?Dt:0;Obstruction+=Reason==TEXT("Obstruction")?Dt:0;
    }
    TSharedRef<FJsonObject> Json() const
    {
        auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("finish_seconds"),Finish);O->SetNumberField(TEXT("moving_average_mph"),Moving>0?SpeedSum/Moving:0);O->SetNumberField(TEXT("peak_mph"),Peak);
        O->SetNumberField(TEXT("moving_seconds"),Moving);O->SetNumberField(TEXT("moving_pct_below20"),Moving>0?Below20/Moving*100:0);O->SetNumberField(TEXT("moving_pct_below25"),Moving>0?Below25/Moving*100:0);O->SetNumberField(TEXT("moving_pct_above30"),Moving>0?Above30/Moving*100:0);
        O->SetNumberField(TEXT("traffic_limited_seconds"),Traffic);O->SetNumberField(TEXT("corner_limited_seconds"),Corner);O->SetNumberField(TEXT("obstruction_limited_seconds"),Obstruction);
        O->SetNumberField(TEXT("brake_events"),BrakeEvents);O->SetNumberField(TEXT("gameplay_contact_events"),Contacts);O->SetNumberField(TEXT("pass_attempts"),PassAttempts);O->SetNumberField(TEXT("pass_completions"),Passes);
        O->SetNumberField(TEXT("completed_laps"),Lap);O->SetNumberField(TEXT("checkpoint"),CP);O->SetStringField(TEXT("last_position"),Position.ToString());O->SetStringField(TEXT("last_control"),LastControl);return O;
    }
};
struct FRaceRun
{
    bool Initialized=false,Restored=false,OldFixed=false;double OldDelta=0,Dt=1./60,Start=0,Last=0,Wall=0,LastLog=0,FinishedAt=0,Frozen=0,LastProgress=0;int Progress=-1,Phase=0;
    TArray<FDriverRun> Drivers;
    FString Name;
    ~FRaceRun(){Restore();}
    void Restore(){if(Initialized && !Restored){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);Restored=true;}}
    void Write(bool Success,const UKartPhysicsWorldSubsystem& Physics,const AKartPawn& Player) const
    {
        // This records race completion, not the later reset assertions. The
        // exported Unreal automation result is authoritative for overall PASS.
        auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("scenario"),Name);Root->SetBoolField(TEXT("race_completed"),Success);Root->SetStringField(TEXT("source"),TEXT("Native PIE ticks; scripted player controls;19 unmodified-style AI; no gate/pose shortcuts; moving>=1mph"));Root->SetNumberField(TEXT("frame_dt"),Dt);
        Root->SetStringField(TEXT("player_pose"),Player.GetActorTransform().ToString());Root->SetNumberField(TEXT("player_speed_kmh"),Player.GetSpeedKmh());Root->SetNumberField(TEXT("player_throttle"),Player.GetThrottleInput());Root->SetNumberField(TEXT("player_steering"),Player.GetSteeringInput());
        TArray<TSharedPtr<FJsonValue>> Rows;double Sum=0,LastFinish=0;int Finished=0,Contacts=Player.GetCollisionCount();
        for(int I=0;I<Drivers.Num();++I){auto O=Drivers[I].Json();O->SetNumberField(TEXT("kart_number"),I+1);Rows.Add(MakeShared<FJsonValueObject>(O));Contacts+=Drivers[I].Contacts;if(Drivers[I].Finish>=0){Sum+=Drivers[I].Finish;LastFinish=FMath::Max(LastFinish,Drivers[I].Finish);++Finished;}}
        Root->SetArrayField(TEXT("drivers"),Rows);Root->SetNumberField(TEXT("npc_finishers"),Finished);Root->SetNumberField(TEXT("mean_npc_official_finish_seconds"),Finished?Sum/Finished:0);Root->SetNumberField(TEXT("last_npc_official_finish_seconds"),LastFinish);Root->SetNumberField(TEXT("gameplay_contact_events_including_player"),Contacts);
        const auto& M=Physics.GetCustomFleet().GetDiagnostics();auto P=MakeShared<FJsonObject>();const double N=FMath::Max(1.,double(M.Steps));
        P->SetNumberField(TEXT("steps"),double(M.Steps));P->SetNumberField(TEXT("mean_step_ms"),M.TotalStepMilliseconds/N);P->SetNumberField(TEXT("max_step_ms"),M.MaxStepMilliseconds);
        P->SetNumberField(TEXT("prepare_proxy_mean_ms"),M.PrepareMilliseconds/N);P->SetNumberField(TEXT("geometry_mean_ms"),M.GeometryMilliseconds/N);P->SetNumberField(TEXT("solver_mean_ms"),M.SolverMilliseconds/N);P->SetNumberField(TEXT("mesh_motion_mean_ms"),M.MotionMilliseconds/N);
        auto Samples=Physics.GetCustomFleet().StepMilliseconds;Samples.Sort();for(const auto Percent:{50,95,99})if(!Samples.IsEmpty())P->SetNumberField(FString::Printf(TEXT("p%d_step_ms"),Percent),Samples[FMath::Clamp(int((Samples.Num()-1)*Percent/100.),0,Samples.Num()-1)]);
        P->SetNumberField(TEXT("max_current_penetration_cm"),M.MaxCurrentPenetrationCm);P->SetNumberField(TEXT("max_normal_residual_cm_s"),M.MaxNormalResidual);P->SetNumberField(TEXT("dynamic_pair_constraint_steps"),double(M.PairsSolved));P->SetNumberField(TEXT("static_pair_constraint_steps"),double(M.StaticPairsSolved));P->SetNumberField(TEXT("duplicate_pairs"),M.DuplicatePairs);P->SetNumberField(TEXT("dropped_simulation_seconds"),Physics.GetDroppedCustomTime());Root->SetObjectField(TEXT("physics_cpu_nullrhi"),P);
        P->SetNumberField(TEXT("max_actual_box_overlap_cm"),M.MaxActualBoxOverlapCm);P->SetStringField(TEXT("deepest_dynamic_pair"),M.DeepestDynamicPair);
        P->SetNumberField(TEXT("refined_steps"),double(M.RefinedSteps));P->SetNumberField(TEXT("max_refinement"),M.MaxRefinement);
        P->SetNumberField(TEXT("world_steps"),double(M.WorldSteps));P->SetNumberField(TEXT("mean_world_step_ms_including_retries"),M.TotalWorldStepMilliseconds/FMath::Max(1.,double(M.WorldSteps)));P->SetNumberField(TEXT("max_world_step_ms_including_retries"),M.MaxWorldStepMilliseconds);
        SavePhysicsRace(Name,Root);
    }
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FRunPhysicsRace,FAutomationTestBase*,Test,TSharedPtr<FRaceRun>,S);
bool FRunPhysicsRace::Update()
{
    auto* W=AutomationCommon::GetAnyGameWorld();auto* P=W?Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(W,0)):nullptr;
    auto* D=W?Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(W,AKartRaceDirector::StaticClass())):nullptr;
    auto* R=W?W->GetSubsystem<URaceLoopSubsystem>():nullptr;auto* F=W?W->GetSubsystem<UKartPhysicsWorldSubsystem>():nullptr;
    if(!P || !D || !R || !F){Test->AddError(TEXT("Stage10 live race components missing"));S->Restore();return true;}
    const double Now=W->GetTimeSeconds(),Wall=FPlatformTime::Seconds();
    if(!S->Initialized)
    {
        S->OldFixed=FApp::UseFixedTimeStep();S->OldDelta=FApp::GetFixedDeltaTime();S->Initialized=true;FApp::SetFixedDeltaTime(S->Dt);FApp::SetUseFixedTimeStep(true);
        R->Tick(0);R->RestartRace();TArray<AKartPawn*> Karts;for(TActorIterator<AKartPawn> It(W);It;++It)Karts.Add(*It);
        if(!Test->TestTrue(TEXT("Stage10 full fleet opt-in"),F->SelectFleet(Karts))){Test->AddInfo(F->GetFleetFailure());S->Restore();return true;}
        F->SetFleetProfiling(true);D->bPaceDiagnostics=true;S->Drivers.SetNum(19);S->Start=S->Last=S->LastProgress=Now;S->Wall=S->LastLog=Wall;
        TSet<FString> Styles;for(const auto& E:D->GetOpponents())Styles.Add(FString::Printf(TEXT("%.6f %.6f %.6f"),E.Aggression,E.CornerSkill,E.PreferredLane));Test->TestEqual(TEXT("Nineteen distinct styles retained"),Styles.Num(),19);
        return false;
    }
    auto Fail=[&](const FString& Why){Test->AddError(Why);S->Write(false,*F,*P);S->Restore();return true;};
    if(F->HasFleetFault())return Fail(FString(TEXT("Stage10 physics fault: "))+F->GetFleetFailure());
    const double Dt=FMath::Max(0.,Now-S->Last);S->Last=Now;int Finished=0,Fingerprint=0;
    for(int I=0;I<D->GetOpponents().Num();++I)
    {
        const auto& E=D->GetOpponents()[I];if(!E.Kart.IsValid())return Fail(TEXT("Stage10 lost NPC actor"));
        S->Drivers[I].Sample(E,Dt,Now);Finished+=E.Progress.bFinished?1:0;Fingerprint+=E.Progress.CompletedLaps*13+E.Progress.NextCheckpoint;
    }
    Fingerprint+=R->GetProgress().CompletedLaps*13+R->GetProgress().NextCheckpoint;
    if(Fingerprint!=S->Progress){S->Progress=Fingerprint;S->LastProgress=Now;}
    if(Now-S->Start<3)for(const auto& E:D->GetOpponents())if(E.Kart->GetCollisionCount()>0)return Fail(TEXT("Custom launch contact within first3s"));
    if(Now-S->Start<3 && F->GetCustomFleet().GetDiagnostics().MaxCurrentPenetrationCm>1.e-4)return Fail(TEXT("Custom launch physical overlap within first3s"));
    if(Wall-S->LastLog>20){UE_LOG(LogTemp,Display,TEXT("STAGE10 %s sim%.3f finishers%d playerLap%d CP%d meanStep%.4fms"),*S->Name,Now-S->Start,Finished,R->GetProgress().CompletedLaps,R->GetProgress().NextCheckpoint,F->GetCustomFleet().GetDiagnostics().TotalStepMilliseconds/FMath::Max(1.,double(F->GetCustomFleet().GetDiagnostics().Steps)));S->LastLog=Wall;}
    if(S->Phase==0)
    {
        KartPhysicsEvaluation::DriveRoute(*P,D->RoutePoints);
        if(Finished==19 && R->GetProgress().bFinished){S->FinishedAt=Now;S->Frozen=R->GetProgress().TotalTime(Now);S->Phase=1;}
        if(Now-S->Start>900 || (Now-S->Start>120 && Now-S->LastProgress>90) || Wall-S->Wall>900)return Fail(TEXT("Custom three-lap race stalled/timed out; inspect per-driver report"));
    }
    else if(S->Phase==1 && Now-S->FinishedAt>10)
    {
        Test->TestEqual(TEXT("Player official result remains frozen"),R->GetProgress().TotalTime(Now),S->Frozen);
        int Hidden=0;for(const auto& E:D->GetOpponents()){Hidden+=E.Kart->IsHidden()?1:0;Test->TestEqual(TEXT("Each NPC completed ordered three laps"),E.Progress.CompletedLaps,3);Test->TestEqual(TEXT("Three official lap times per NPC"),E.Progress.LapTimes.Num(),3);}
        Test->TestEqual(TEXT("NPC cooldown retirement avoids visible stacking"),Hidden,19);Test->TestTrue(TEXT("Finished NPC bodies retired"),F->GetCustomFleet().Bodies.Num()<=1);
        Test->TestEqual(TEXT("No duplicate pair response"),F->GetCustomFleet().GetDiagnostics().DuplicatePairs,0);Test->TestTrue(TEXT("Fixed clock retains simulation time"),F->GetDroppedCustomTime()<1.e-6);
        Test->TestTrue(TEXT("Full-race actual kart overlap below2mm"),F->GetCustomFleet().GetDiagnostics().MaxActualBoxOverlapCm<.2);
        Test->TestTrue(TEXT("Packed-race shared refinement actually exercised"),F->GetCustomFleet().GetDiagnostics().RefinedSteps>0);
        S->Write(true,*F,*P);auto* PC=UGameplayStatics::GetPlayerController(W,0);PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F5,IE_Pressed,FPlatformTime::Cycles64()));S->Phase=2;S->FinishedAt=Now;
    }
    else if(S->Phase==2 && Now-S->FinishedAt>.1)
    {
        Test->TestFalse(TEXT("F5 resets full race"),R->GetProgress().bStarted);Test->TestEqual(TEXT("F5 recreates all twenty physics bodies"),F->GetCustomFleet().Bodies.Num(),20);
        for(const auto& E:D->GetOpponents()){Test->TestFalse(TEXT("F5 restores racer presentation"),E.Kart->IsHidden());Test->TestFalse(TEXT("F5 clears finished state"),E.Kart->IsRaceFinished());Test->TestEqual(TEXT("F5 clears incident counters"),E.Kart->GetCollisionCount(),0);}
        Test->TestEqual(TEXT("Reset clears profiler samples"),F->GetCustomFleet().StepMilliseconds.Num(),int(F->GetCustomFleet().GetDiagnostics().Steps));
        auto* PC=UGameplayStatics::GetPlayerController(W,0);PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F5,IE_Released,0.f,false,FPlatformTime::Cycles64()));S->Restore();return true;
    }
    return false;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FKartStage10RaceTest,"RoadToF1.Physics.Stage10FullRace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FKartStage10RaceTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{Names.Add(TEXT("Custom60"));Commands.Add(TEXT("60"));Names.Add(TEXT("Custom30"));Commands.Add(TEXT("30"));}
bool FKartStage10RaceTest::RunTest(const FString& Parameters)
{
    auto State=MakeShared<FRaceRun>();State->Dt=Parameters==TEXT("30")?1./30:1./60;State->Name=Parameters==TEXT("30")?TEXT("Custom30"):TEXT("Custom60");
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));ADD_LATENT_AUTOMATION_COMMAND(FRunPhysicsRace(this,State));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage10Comparison,"RoadToF1.Physics.Stage10StraightComparison",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage10Comparison::RunTest(const FString&)
{
    if(!FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true))return false;
    auto* W=GEditor->GetEditorWorldContext().World();AKartRaceDirector* D=nullptr;for(TActorIterator<AKartRaceDirector> It(W);It;++It){D=*It;break;}if(!D || D->Grid.Num()!=20)return false;
    auto* F=W->GetSubsystem<UKartPhysicsWorldSubsystem>();auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("source"),TEXT("Same actual-grid straight, ordinary full throttle6s then full brake; supplied1/60s frame steps, no race/gate claims"));
    for(bool Custom:{false,true})
    {
        auto* K=W->SpawnActor<AKartPawn>(D->Grid.Last().GetLocation(),D->Grid.Last().Rotator());if(!K)return false;
        if(Custom && !F->SelectFleet({K}))return false;double T90=-1,Peak=0;K->SetDriveInput(1,0);
        for(int I=0;I<360;++I){if(Custom){if(!F->AdvanceCustomWorld(1./60)){AddError(F->GetFleetFailure());return false;}}else K->Tick(1.f/60.f);Peak=FMath::Max(Peak,double(K->GetSpeedKmh()));if(T90<0 && K->GetSpeedKmh()>=57*.9)T90=(I+1)/60.;}
        const auto BrakeStart=K->GetActorLocation();K->SetDriveInput(0,0,true);double Stop=-1;
        for(int I=0;I<240;++I){if(Custom){if(!F->AdvanceCustomWorld(1./60))return false;}else K->Tick(1.f/60.f);if(Stop<0 && FMath::Abs(K->GetSpeedKmh())<.1)Stop=(I+1)/60.;}
        TestTrue(TEXT("Existing cap approached without changing propulsion settings"),Peak>=57*.9 && Peak<=57.1);TestTrue(TEXT("Normal braking stops backend"),Stop>0);
        auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("time_to90pct_cap_seconds"),T90);O->SetNumberField(TEXT("peak_kmh"),Peak);O->SetNumberField(TEXT("braking_seconds"),Stop);O->SetNumberField(TEXT("braking_distance_cm"),FVector::Dist2D(BrakeStart,K->GetActorLocation()));Root->SetObjectField(Custom?TEXT("custom"):TEXT("legacy"),O);
        K->ResetKart();if(Custom)F->ReleaseFleet();K->Destroy();
    }
    return SavePhysicsRace(TEXT("StraightComparison"),Root);
}
namespace {struct FSelectionRun {int Phase=0;double At=-1;};}
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FRunPhysicsSelection,FAutomationTestBase*,Test,TSharedPtr<FSelectionRun>,S);
bool FRunPhysicsSelection::Update()
{
    auto* W=AutomationCommon::GetAnyGameWorld();auto* P=W?Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(W,0)):nullptr;
    auto* PC=W?UGameplayStatics::GetPlayerController(W,0):nullptr;
    if(!P || !PC){Test->AddError(TEXT("Physics selection PIE player missing"));return true;}
    auto* F=W->GetSubsystem<UKartPhysicsWorldSubsystem>();auto* R=W->GetSubsystem<URaceLoopSubsystem>();
    const double Now=W->GetTimeSeconds();if(S->At>=0 && Now-S->At<(S->Phase==2?.6:.2))return false;
    auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,E==IE_Pressed?1.f:0.f,false,FPlatformTime::Cycles64()));};
    auto Tap=[&](FKey K){Key(K,IE_Pressed);Key(K,IE_Released);};
    auto HasLabel=[&](const TCHAR* Label)
    {
        TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,URaceLoopWidget::StaticClass(),false);
        for(auto* Widget:Widgets){CastChecked<URaceLoopWidget>(Widget)->RefreshRaceData(FGeometry());TArray<UWidget*> All;Widget->WidgetTree->GetAllWidgets(All);for(auto* Item:All)if(auto* T=Cast<UTextBlock>(Item))if(T->GetText().ToString()==Label)return true;}return false;
    };
    if(S->Phase==0)
    {
        Test->TestEqual(TEXT("Normal Play defaults to Legacy"),P->GetPhysicsBackend(),EKartPhysicsBackend::Legacy);
        Test->TestTrue(TEXT("HUD identifies Legacy"),HasLabel(TEXT("mph / LEGACY")));
        bool RejectedImplicit=false;
        {
            TGuardValue<bool> NormalGameplay(GIsAutomationTesting,false);
            TArray<AKartPawn*> Karts;for(TActorIterator<AKartPawn> It(W);It;++It)Karts.Add(*It);
            RejectedImplicit=!F->SelectFleet(Karts,false);
            Tap(EKeys::F10);
        }
        Test->TestTrue(TEXT("Normal gameplay rejects implicit Custom admission"),RejectedImplicit);
    }
    else if(S->Phase==1)
    {
        Test->TestEqual(TEXT("F10 explicitly selects shared Custom fleet"),P->GetPhysicsBackend(),EKartPhysicsBackend::Custom);
        Test->TestEqual(TEXT("All20 karts selected atomically"),F->GetCustomFleet().Bodies.Num(),20);
        Test->TestTrue(TEXT("HUD identifies active Custom"),HasLabel(TEXT("mph / CUSTOM")));Key(EKeys::W,IE_Pressed);
    }
    else if(S->Phase==2){Test->TestTrue(TEXT("Player is moving"),P->GetVelocity().Size()>1);Tap(EKeys::F10);Key(EKeys::W,IE_Released);}
    else if(S->Phase==3)
    {
        Test->TestEqual(TEXT("Moving F10 cannot transfer response ownership"),P->GetPhysicsBackend(),EKartPhysicsBackend::Custom);
        R->RestartRace();Tap(EKeys::F10);
    }
    else
    {
        Test->TestEqual(TEXT("Reset then F10 restores Legacy"),P->GetPhysicsBackend(),EKartPhysicsBackend::Legacy);
        Test->TestTrue(TEXT("HUD restores Legacy label"),HasLabel(TEXT("mph / LEGACY")));
        Test->TestEqual(TEXT("Custom fleet released"),F->GetCustomFleet().Bodies.Num(),0);return true;
    }
    ++S->Phase;S->At=Now;return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage10Selection,"RoadToF1.Physics.Stage10KeyboardSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage10Selection::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
    ADD_LATENT_AUTOMATION_COMMAND(FRunPhysicsSelection(this,MakeShared<FSelectionRun>()));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
