#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsViewFeedback.h"
#include "KartPawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage8Test,"RoadToF1.Physics.Stage8View",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage8Test::RunTest(const FString&)
{
    FKartPhysicsViewFeedback V;TestTrue(TEXT("Neutral valid"),V.Step({},1./60));
    TestTrue(TEXT("Neutral effect"),V.Offset.IsZero() && V.Rotation.IsZero() && V.FOV==88);
    FKartPhysicsViewInput I;I.LocalAcceleration=FVector(980,400,100);I.LocalAngularVelocity=FVector(0,0,.5);I.SpeedCmPerSecond=1200;
    auto Run=[&](int FPS){FKartPhysicsViewFeedback R;for(int J=0;J<FPS*2;++J)TestTrue(TEXT("Finite feedback"),R.Step(I,1./FPS));return R;};
    auto A=Run(30),B=Run(60),C=Run(120);
    TestTrue(TEXT("Frame-independent constant telemetry"),A.Offset.Equals(B.Offset,1.e-10) && A.Offset.Equals(C.Offset,1.e-10) && FMath::Abs(A.FOV-C.FOV)<1.e-10);
    TestTrue(TEXT("Moderate bounded response"),A.Offset.Size()>0 && A.Offset.Size()<=1.5 && FMath::Abs(A.Rotation.Pitch)<=2);
    FKartPhysicsViewFeedback Small,Large;FKartPhysicsViewInput Hit;Hit.LocalImpactImpulse=FVector(160,0,0);Small.Step(Hit,1./120);
    Hit.LocalImpactImpulse=FVector(160000,0,0);Large.Step(Hit,1./120);
    TestTrue(TEXT("Impact scales with delta velocity"),Large.Offset.Size()>Small.Offset.Size()*10 && Large.Offset.Size()<=1.5);
    double Previous=Large.Offset.Size();bool Decay=true;
    for(int J=0;J<240;++J){Large.Step({},1./120);Decay&=Large.Offset.Size()<=Previous;Previous=Large.Offset.Size();}
    TestTrue(TEXT("Smooth impact decay"),Decay && Previous<1.e-6);
    Hit.LocalAcceleration=FVector(1.e12);Hit.KerbSeverity=100;Hit.EngineActivity=100;Hit.RoadSeverity=100;
    TestTrue(TEXT("Extreme input bounded"),V.Step(Hit,1./30) && V.Offset.Size()<=1.5 && FMath::Abs(V.Rotation.Roll)<=2);
    V.Reset();TestTrue(TEXT("Reset neutral"),V.Offset.IsZero() && V.Rotation.IsZero() && V.FOV==88);
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Pawn=World->SpawnActor<AKartPawn>();const auto Transform=Pawn->GetActorTransform();
    const bool Mode=Pawn->IsFirstPerson();Pawn->ToggleCamera();TestTrue(TEXT("Existing camera toggle"),Pawn->IsFirstPerson()!=Mode);Pawn->ToggleCamera();
    V.Step(I,1./60);TestTrue(TEXT("No game mutation"),Pawn->GetActorTransform().Equals(Transform) && Pawn->IsFirstPerson()==Mode && Pawn->GetSpeedKmh()==0);
    return true;
}
#endif
