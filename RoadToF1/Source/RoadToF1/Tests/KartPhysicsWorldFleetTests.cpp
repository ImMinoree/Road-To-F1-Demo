#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPawn.h"
#include "KartRaceDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Components/PrimitiveComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartActualWorldFleetTest,"RoadToF1.Physics.Stage9ActualWorldFleet",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartActualWorldFleetTest::RunTest(const FString&)
{
    if(!TestTrue(TEXT("Load actual race map"),FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true)))return false;
    auto* W=GEditor->GetEditorWorldContext().World();AKartRaceDirector* D=nullptr;for(TActorIterator<AKartRaceDirector> It(W);It;++It){D=*It;break;}
    if(!TestNotNull(TEXT("Authored race grid"),D) || !TestEqual(TEXT("Twenty authored grid slots"),D->Grid.Num(),20))return false;
    auto* Physics=W->GetSubsystem<UKartPhysicsWorldSubsystem>();if(!TestNotNull(TEXT("World coordinator"),Physics))return false;
    for(int Count:{2,5,10,20})
    {
        TArray<AKartPawn*> Actors;
        for(int I=0;I<Count;++I)
        {
            const int Slot=I==Count-1?19:I;auto* K=W->SpawnActor<AKartPawn>(D->Grid[Slot].GetLocation(),D->Grid[Slot].Rotator());
            if(!K){AddError(TEXT("Fixture spawn failed"));return false;}K->ConfigureRacer(I,I!=Count-1);Actors.Add(K);
        }
        if(!TestTrue(TEXT("Atomic explicit fleet admission"),Physics->SelectFleet(Actors)))
        {AddInfo(Physics->GetFleetFailure());return false;}
        const auto Start=Actors.Last()->GetActorLocation();bool Valid=true;
        for(auto* K:Actors)K->SetDriveInput(1,0);
        for(int Frame=0;Frame<120;++Frame)if(!Physics->AdvanceCustomWorld(1./60)){Valid=false;AddInfo(FString::Printf(TEXT("World%d frame%d %s"),Count,Frame,*Physics->GetFleetFailure()));break;}
        TestTrue(TEXT("Actual-map fleet remains finite"),Valid);
        if(!Valid)return false;
        TestTrue(TEXT("Actor transform and speed published"),FVector::Dist2D(Actors.Last()->GetActorLocation(),Start)>100 && Actors.Last()->GetSpeedKmh()>8);
        TestEqual(TEXT("One clock produces240 shared steps, not240 per pawn"),Physics->GetCustomFleet().GetDiagnostics().Steps,uint64(240));
        for(auto* K:Actors){TestEqual(TEXT("Custom ownership"),K->GetPhysicsBackend(),EKartPhysicsBackend::Custom);TestFalse(TEXT("No engine rigid body simulation"),CastChecked<UPrimitiveComponent>(K->GetRootComponent())->IsSimulatingPhysics());}
        TestFalse(TEXT("Moving fleet cannot change owners"),Physics->ReleaseFleet());
        Actors.Last()->ToggleCamera();TestFalse(TEXT("Existing V-mode toggle available"),Actors.Last()->IsFirstPerson());Actors.Last()->ToggleCamera();
        for(int I=0;I<Count;++I){const int Slot=I==Count-1?19:I;Actors[I]->SetActorTransform(D->Grid[Slot]);Actors[I]->ResetKart();}
        TestEqual(TEXT("Reset clears shared clock diagnostics"),Physics->GetCustomFleet().GetDiagnostics().Steps,uint64(0));
        for(auto* K:Actors)TestTrue(TEXT("Reset clears custom velocity"),K->GetVelocity().IsZero() && K->GetSpeedKmh()==0);
        TestTrue(TEXT("Stationary group fallback"),Physics->ReleaseFleet());
        for(auto* K:Actors){TestEqual(TEXT("Legacy fallback restored"),K->GetPhysicsBackend(),EKartPhysicsBackend::Legacy);K->Destroy();}
        AddInfo(FString::Printf(TEXT("ACTUAL_WORLD_FLEET bodies%d movement/reset/view/fallback passed"),Count));
    }
    return true;
}
#endif
