#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsWorldSubsystem.h"
#include "KartPhysicsEvaluationDriver.h"
#include "KartRaceDirector.h"
#include "RaceTrackGate.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartSoloCircuitTest,"RoadToF1.Physics.Stage9ActualSoloCircuit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartSoloCircuitTest::RunTest(const FString&)
{
    if(!TestTrue(TEXT("Actual map loads"),FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true)))return false;
    auto* W=GEditor->GetEditorWorldContext().World();AKartRaceDirector* D=nullptr;for(TActorIterator<AKartRaceDirector> It(W);It;++It){D=*It;break;}
    if(!TestNotNull(TEXT("Authored route"),D) || D->Grid.Num()!=20)return false;
    auto* F=W->GetSubsystem<UKartPhysicsWorldSubsystem>();auto* K=W->SpawnActor<AKartPawn>(D->Grid.Last().GetLocation(),D->Grid.Last().Rotator());if(!K)return false;
    K->ConfigureRacer(0,false);if(!TestTrue(TEXT("Single actual kart admission"),F->SelectFleet({K}))){AddError(F->GetFleetFailure());return false;}
    F->SetFleetContactDiagnostics(true);
    TArray<ARaceTrackGate*> Actors;for(TActorIterator<ARaceTrackGate> It(W);It;++It)Actors.Add(*It);
    Actors.Sort([](const ARaceTrackGate& A,const ARaceTrackGate& B){return A.Order<B.Order;});
    if(!TestEqual(TEXT("Original thirteen gates"),Actors.Num(),13))return false;
    TArray<FRaceGateGeometry> Gates;for(auto* A:Actors){FRaceGateGeometry G;G.Transform=A->GateBounds->GetComponentTransform();G.Extent=A->GateBounds->GetScaledBoxExtent();Gates.Add(G);}
    FRaceProgress Progress;Progress.Reset(1,12);auto Previous=K->GetActorLocation();double Stall=0;int OldCP=-1;
    for(int Frame=0;Frame<14400 && !Progress.bFinished;++Frame)
    {
        KartPhysicsEvaluation::DriveRoute(*K,D->RoutePoints);
        if(!F->AdvanceCustomWorld(1./60)){AddError(F->GetFleetFailure());break;}
        const auto Current=K->GetActorLocation();
        for(int I=0;I<Gates.Num();++I){double Fraction;if(Gates[I].ForwardCrossing(Previous,Current,Fraction))Progress.CrossGate(I,(Frame+Fraction)/60.);}
        Previous=Current;Stall=FMath::Abs(K->GetSpeedKmh())<1?Stall+1./60:0;
        if(Progress.NextCheckpoint!=OldCP){OldCP=Progress.NextCheckpoint;AddInfo(FString::Printf(TEXT("SOLO sim%.3f CP%d speed%.3f pos%s"),Frame/60.,OldCP,K->GetSpeedKmh(),*Current.ToString()));}
        if(Stall>30)
        {
            AddError(FString::Printf(TEXT("Solo actual-track stall at%.3fs CP%d pos%s input%.3f brake%d"),Frame/60.,Progress.NextCheckpoint,*Current.ToString(),K->GetThrottleInput(),K->IsBrakeHeld()));
            const auto& B=F->GetCustomFleet().Bodies[0];AddInfo(FString::Printf(TEXT("SOLO loads%.4f %.4f %.4f %.4f shared%s scene%s"),B.Vehicle.Wheels[0].Load,B.Vehicle.Wheels[1].Load,B.Vehicle.Wheels[2].Load,B.Vehicle.Wheels[3].Load,*B.SharedImpulse.ToString(),*B.SceneImpulse.ToString()));
            FKartStaticMeshGeometry Scene;FString Why;Scene.Build(*W,Why);
            for(const auto& C:F->GetCustomFleet().LastContacts)
            {
                AddInfo(FString::Printf(TEXT("SOLO source%s normal%s point%s depth%.9f Jn%.9f Jt%s"),*C.Source,*C.Contact.Normal.ToString(),*C.Contact.Point.ToString(),C.Depth,C.Contact.NormalImpulse,*C.Contact.TangentImpulse.ToString()));
                const int T=int(C.Contact.Pair.Second().Value-(1ull<<32));
                if(Scene.GetTriangles().IsValidIndex(T))
                {
                    const auto& Tri=Scene.GetTriangles()[T];
                    AddInfo(FString::Printf(TEXT("SOLO_TRIANGLE index%d face%s vertices%s;%s;%s"),T,*FVector::CrossProduct(Tri.V[1]-Tri.V[0],Tri.V[2]-Tri.V[0]).GetSafeNormal().ToString(),*Tri.V[0].ToString(),*Tri.V[1].ToString(),*Tri.V[2].ToString()));
                }
            }
            break;
        }
    }
    TestTrue(TEXT("Solo actual physical circuit completes all ordered gates"),Progress.bFinished);
    K->ResetKart();F->ReleaseFleet();K->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartActualRejoinTest,"RoadToF1.Physics.Stage9ActualRejoins",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartActualRejoinTest::RunTest(const FString&)
{
    if(!FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true))return false;
    auto* W=GEditor->GetEditorWorldContext().World();AKartRaceDirector* D=nullptr;for(TActorIterator<AKartRaceDirector> It(W);It;++It){D=*It;break;}
    if(!D || D->Grid.Num()!=20)return false;
    auto* F=W->GetSubsystem<UKartPhysicsWorldSubsystem>();auto* K=W->SpawnActor<AKartPawn>(D->Grid.Last().GetLocation(),D->Grid.Last().Rotator());if(!K || !F->SelectFleet({K}))return false;
    int Cases=0;
    for(int Index=0;Index<D->RoutePoints.Num();Index+=50)for(double Side:{-1.,1.})for(bool Reverse:{false,true})
    {
        const auto Tangent=(D->RoutePoints[(Index+1)%D->RoutePoints.Num()]-D->RoutePoints[Index]).GetSafeNormal2D();
        const FVector Normal(-Tangent.Y,Tangent.X,0),Inward=Normal*-Side;
        auto Offroad=D->RoutePoints[Index]+Normal*Side*700;Offroad.Z=18;
        K->SetActorLocationAndRotation(Offroad,(Reverse?-Inward:Inward).Rotation(),false);K->ResetKart();K->SetDriveInput(Reverse?-1:1,0);
        bool Valid=true;double Progress=0;
        for(int Frame=0;Frame<240 && Progress<=700;++Frame)
        {
            if(!F->AdvanceCustomWorld(1./60)){Valid=false;break;}
            Progress=FVector::DotProduct(K->GetActorLocation()-Offroad,Inward);
        }
        if(!TestTrue(FString::Printf(TEXT("Physical actual-road rejoin sample%d side%.0f reverse%d progress%.4f failure%s"),Index,Side,Reverse,Progress,*F->GetFleetFailure()),Valid && Progress>650))
        {K->ResetKart();F->ReleaseFleet();K->Destroy();return false;}
        TestTrue(TEXT("Kart remains above actual grass surface"),K->GetActorLocation().Z>14);++Cases;
    }
    AddInfo(FString::Printf(TEXT("ACTUAL_CUSTOM_REJOINS %d forward/reverse road edges passed"),Cases));
    K->ResetKart();F->ReleaseFleet();K->Destroy();return true;
}
#endif
