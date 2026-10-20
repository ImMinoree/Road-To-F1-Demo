#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "KartRaceDirector.h"
#include "KartTrainingRecorder.h"
#include "RaceLoopSubsystem.h"
#include "RaceLoopWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "KartPhysicsEvaluationDriver.h"

namespace
{
struct FLiveState
{
    bool Initialized=false,OldFixed=false,Restored=false;double OldDelta=0,Start=0,RunStart=0;
    int Phase=0;double Peak=0,Reverse=0,WallStart=0,LastWall=0,StallStart=0;FVector Initial;float InitialYaw=0;
    ~FLiveState(){Restore();}
    void Restore(){if(Initialized && !Restored){FApp::SetUseFixedTimeStep(OldFixed);FApp::SetFixedDeltaTime(OldDelta);Restored=true;}}
};
}
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FRunCustomLive,FAutomationTestBase*,Test,TSharedPtr<FLiveState>,S);
bool FRunCustomLive::Update()
{
    UWorld* W=AutomationCommon::GetAnyGameWorld();if(!W){Test->AddError(TEXT("Custom PIE world missing"));S->Restore();return true;}
    auto* P=Cast<AKartPawn>(UGameplayStatics::GetPlayerPawn(W,0));auto* D=Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(W,AKartRaceDirector::StaticClass()));
    auto* R=W->GetSubsystem<URaceLoopSubsystem>();auto* F=W->GetSubsystem<UKartPhysicsWorldSubsystem>();auto* PC=UGameplayStatics::GetPlayerController(W,0);
    if(!P || !D || !R || !F || !PC){Test->AddError(TEXT("Custom PIE race components missing"));S->Restore();return true;}
    auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,E==IE_Pressed?1.f:0.f,false,FPlatformTime::Cycles64()));};
    if(!S->Initialized)
    {
        S->OldFixed=FApp::UseFixedTimeStep();S->OldDelta=FApp::GetFixedDeltaTime();S->Initialized=true;
        S->WallStart=S->LastWall=FPlatformTime::Seconds();
        FApp::SetFixedDeltaTime(1./60);FApp::SetUseFixedTimeStep(true);
        R->Tick(0);R->RestartRace();TArray<AKartPawn*> Karts;for(TActorIterator<AKartPawn> It(W);It;++It)Karts.Add(*It);
        if(!Test->TestEqual(TEXT("Live twenty kart field"),Karts.Num(),20) || !Test->TestTrue(TEXT("Live atomic fleet selection"),F->SelectFleet(Karts)))
        {Test->AddInfo(F->GetFleetFailure());S->Restore();return true;}
        F->SetFleetContactDiagnostics(true);
        TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,URaceLoopWidget::StaticClass(),false);
        Test->TestEqual(TEXT("Existing HUD present"),Widgets.Num(),1);S->Start=W->GetTimeSeconds();S->Initial=P->GetActorLocation();S->InitialYaw=P->GetActorRotation().Yaw;
        Key(EKeys::W,IE_Pressed);Key(EKeys::F9,IE_Pressed);Key(EKeys::V,IE_Pressed);return false;
    }
    if(F->HasFleetFault()){Test->AddError(FString::Printf(TEXT("Live fleet fault phase%d elapsed%.3f: %s"),S->Phase,W->GetTimeSeconds()-S->Start,*F->GetFleetFailure()));S->Restore();return true;}
    const double Wall=FPlatformTime::Seconds();
    if(Wall-S->LastWall>10)
    {
        const auto& M=F->GetCustomFleet().GetDiagnostics();
        UE_LOG(LogTemp,Display,TEXT("CUSTOM_LIVE phase%d sim%.3f wall%.3f steps%llu mean%.4fms max%.4fms pairs%llu static%llu rows%llu residual%.5f penetration%.5f playerCP%d speed%.3f pos%s"),S->Phase,W->GetTimeSeconds()-S->Start,Wall-S->WallStart,M.Steps,M.Steps?M.TotalStepMilliseconds/M.Steps:0,M.MaxStepMilliseconds,M.PairsSolved,M.StaticPairsSolved,M.ContactRows,M.MaxNormalResidual,M.MaxCurrentPenetrationCm,R->GetProgress().NextCheckpoint,P->GetSpeedKmh(),*P->GetActorLocation().ToString());S->LastWall=Wall;
        int Printed=0;for(const auto& B:F->GetCustomFleet().Bodies)if(B.Ignore==P || (Printed<2 && B.Vehicle.Body.GetLinearVelocity().Size()<30))
        {
            const auto* K=Cast<AKartPawn>(B.Ignore);const auto& V=B.Vehicle;
            UE_LOG(LogTemp,Display,TEXT("CUSTOM_BODY kart%d throttle%.3f brake%.3f steer%.3f q%s omega%s impulse%s contacts%d loads%.3f,%.3f,%.3f,%.3f fx%.3f,%.3f,%.3f,%.3f wheels%.3f,%.3f,%.3f,%.3f"),K?K->GetKartNumber():-1,B.Input.Throttle,B.Input.Brake,B.Input.Steer,*V.Body.GetOrientation().ToString(),*V.Body.GetAngularVelocity().ToString(),*B.Impulse.ToString(),K?K->GetCollisionCount():-1,V.Wheels[0].Load,V.Wheels[1].Load,V.Wheels[2].Load,V.Wheels[3].Load,V.Wheels[0].LongitudinalForce,V.Wheels[1].LongitudinalForce,V.Wheels[2].LongitudinalForce,V.Wheels[3].LongitudinalForce,V.Wheels[0].AngularVelocity,V.Wheels[1].AngularVelocity,V.Wheels[2].AngularVelocity,V.Wheels[3].AngularVelocity);if(B.Ignore!=P)++Printed;
            UE_LOG(LogTemp,Display,TEXT("CUSTOM_RESPONSE kart%d shared%s scene%s"),K?K->GetKartNumber():-1,*B.SharedImpulse.ToString(),*B.SceneImpulse.ToString());
            int Rows=0;for(const auto& C:F->GetCustomFleet().LastContacts)if((C.Contact.Pair.First()==B.Id || C.Contact.Pair.Second()==B.Id) && Rows++<8)
                UE_LOG(LogTemp,Display,TEXT("CUSTOM_CONTACT kart%d pair%llu:%llu source%s normal%s point%s depth%.9f allowed%.6f Jn%.6f Jt%s"),K?K->GetKartNumber():-1,C.Contact.Pair.First().Value,C.Contact.Pair.Second().Value,*C.Source,*C.Contact.Normal.ToString(),*C.Contact.Point.ToString(),C.Depth,C.AllowedClosingSpeed,C.Contact.NormalImpulse,*C.Contact.TangentImpulse.ToString());
        }
    }
    if(Wall-S->WallStart>600){Test->AddError(TEXT("Custom live integration exceeded600s wall budget; see timing snapshots"));S->Restore();return true;}
    const double Elapsed=W->GetTimeSeconds()-S->Start;S->Peak=FMath::Max(S->Peak,double(P->GetSpeedKmh()));
    if(S->Phase==0 && Elapsed>2)
    {
        Test->TestTrue(TEXT("Keyboard throttle drives Custom actor"),S->Peak>8 && FVector::Dist2D(P->GetActorLocation(),S->Initial)>100);
        Test->TestTrue(TEXT("Real coordinator world tick advances"),F->GetCustomFleet().GetDiagnostics().Steps>200);
        Test->TestTrue(TEXT("F9 records actual Custom movement"),D->GetTrainingRecorder()->IsRecording() && D->GetTrainingRecorder()->GetSampleCount()>5);
        Test->TestFalse(TEXT("V keyboard toggles existing camera"),P->IsFirstPerson());
        Key(EKeys::W,IE_Released);Key(EKeys::F9,IE_Released);Key(EKeys::V,IE_Released);Key(EKeys::SpaceBar,IE_Pressed);S->Phase=1;
    }
    if(S->Phase==1 && Elapsed>4)
    {
        Test->TestTrue(TEXT("Keyboard braking stops Custom kart"),FMath::Abs(P->GetSpeedKmh())<1);
        Key(EKeys::SpaceBar,IE_Released);Key(EKeys::S,IE_Pressed);S->Phase=2;
    }
    if(S->Phase==2)
    {
        S->Reverse=FMath::Min(S->Reverse,double(P->GetSpeedKmh()));
        if(Elapsed>6){Test->TestTrue(TEXT("Keyboard reverse drives backward"),S->Reverse<-2);Key(EKeys::S,IE_Released);Key(EKeys::F9,IE_Pressed);Key(EKeys::F5,IE_Pressed);S->Phase=3;}
    }
    if(S->Phase==3 && Elapsed>6.1)
    {
        Test->TestFalse(TEXT("F9 capture closed"),D->GetTrainingRecorder()->IsRecording());
        Test->TestFalse(TEXT("F5 resets ordered race progress"),R->GetProgress().bStarted);
        Test->TestTrue(TEXT("F5 restores player grid"),P->GetActorLocation().Equals(S->Initial,5));
        Test->TestTrue(TEXT("F5 clears custom speed"),FMath::Abs(P->GetSpeedKmh())<.1);
        Key(EKeys::F9,IE_Released);Key(EKeys::F5,IE_Released);S->RunStart=W->GetTimeSeconds();S->Phase=4;
    }
    if(S->Phase==4)
    {
        KartPhysicsEvaluation::DriveRoute(*P,D->RoutePoints);
        const double Run=W->GetTimeSeconds()-S->RunStart;
        if(Run>10 && FMath::Abs(P->GetSpeedKmh())<1){if(S->StallStart==0)S->StallStart=Run;}
        else S->StallStart=0;
        if(S->StallStart>0 && Run-S->StallStart>30){Test->AddError(TEXT("Custom physical lap failed: player stalled below1km/h for30s; see body/contact snapshots"));S->Restore();return true;}
        if(R->GetProgress().CompletedLaps>=1)
        {
            Test->TestTrue(TEXT("Physical crossings started ordered lap timing"),R->GetProgress().bStarted && R->GetProgress().LapTimes.Num()>0 && R->GetProgress().LapTimes.Last()>0);
            Test->AddInfo(FString::Printf(TEXT("LIVE_CUSTOM player actual ordered lap completed %.3fs; steps%llu pairs%llu maxstep%.4fms"),Run,F->GetCustomFleet().GetDiagnostics().Steps,F->GetCustomFleet().GetDiagnostics().PairsSolved,F->GetCustomFleet().GetDiagnostics().MaxStepMilliseconds));
            S->Restore();return true;
        }
        if(Run>240)
        {
            Test->AddError(FString::Printf(TEXT("Custom physical lap timed out: started%d lap%d CP%d position%s speed%.3f"),R->GetProgress().bStarted,R->GetProgress().CompletedLaps,R->GetProgress().NextCheckpoint,*P->GetActorLocation().ToString(),P->GetSpeedKmh()));
            for(const auto& E:D->GetOpponents())Test->AddInfo(FString::Printf(TEXT("NPC%d lap%d CP%d speed%.3f pos%s"),E.Kart->GetKartNumber(),E.Progress.CompletedLaps,E.Progress.NextCheckpoint,E.Kart->GetSpeedKmh(),*E.Kart->GetActorLocation().ToString()));
            S->Restore();return true;
        }
    }
    return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartCustomLiveTest,"RoadToF1.Physics.Stage9LiveIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartCustomLiveTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
    ADD_LATENT_AUTOMATION_COMMAND(FRunCustomLive(this,MakeShared<FLiveState>()));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
