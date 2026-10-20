#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsSolver.h"
#include "KartPhysicsManifoldBuilder.h"
#include "KartPhysicsIntegrator.h"
#include "KartPawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
namespace KartStage5
{
FKartRigidBodyState Body(double Mass,FVector P,FVector V=FVector::ZeroVector)
{FKartRigidBodyState B;B.ConfigureBox(Mass,FVector(20));B.SetPose(P,FQuat::Identity);B.SetVelocities(V,FVector::ZeroVector);return B;}
FKartContactManifold Contact(const TMap<FKartPhysicsBodyId,FKartRigidBodyState>& B,FVector Point=FVector(9,0,0))
{
    FKartContactManifold M;M.Pair={{1},{2}};M.Status=EKartManifoldStatus::Valid;M.Normal=FVector(1,0,0);
    FKartContactPoint P;P.WorldA=P.WorldB=Point;
    P.LocalA=Point-B.FindChecked({1}).GetPosition();P.LocalB=Point-B.FindChecked({2}).GetPosition();M.Points.Add(P);return M;
}
FKartConvexShape Shape(const FKartRigidBodyState& B)
{FKartConvexShape S;S.Position=B.GetPosition();S.Orientation=B.GetOrientation();S.HalfExtents=FVector(10);return S;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage5Test,"RoadToF1.Physics.Stage5Solver",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage5Test::RunTest(const FString&)
{
    using namespace KartStage5;
    FKartSolverSettings S;S.BiasFraction=0;
    for(double Mass:{1.,3.})
    {
        TMap<FKartPhysicsBodyId,FKartRigidBodyState> B;
        B.Add({1},Body(1,FVector::ZeroVector,FVector(100,0,0)));B.Add({2},Body(Mass,FVector(18,0,0),FVector(-50,0,0)));
        const auto R=FKartPhysicsSolver::Solve(B,{Contact(B)},1./120,S);
        TestTrue(TEXT("Head-on solve"),R.bSuccess);
        const double Expected=(100-50*Mass)/(1+Mass);
        TestTrue(TEXT("Analytical inelastic velocity A"),FMath::Abs(B[{1}].GetLinearVelocity().X-Expected)<1.e-9);
        TestTrue(TEXT("Momentum conservation B"),FMath::Abs(B[{2}].GetLinearVelocity().X-Expected)<1.e-9);
        TestTrue(TEXT("No attractive impulse"),R.Contacts.Num()==1 && R.Contacts[0].NormalImpulse>=0);
    }
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> B;
    B.Add({1},Body(1,FVector::ZeroVector,FVector(100,0,0)));B.Add({2},Body(1,FVector(18,0,0)));B[{2}].SetImmovable(true);
    auto Off=B;TestTrue(TEXT("Off-center solve"),FKartPhysicsSolver::Solve(Off,{Contact(Off,FVector(9,5,0))},1./120,S).bSuccess);
    TestTrue(TEXT("Off-center angular response"),Off[{1}].GetAngularVelocity().Z>0);
    TestTrue(TEXT("Static unchanged"),Off[{2}].GetLinearVelocity().IsZero() && Off[{2}].GetAngularVelocity().IsZero());
    TestTrue(TEXT("Wall solve"),FKartPhysicsSolver::Solve(B,{Contact(B)},1./120,S).bSuccess);
    TestTrue(TEXT("Wall stops normal speed"),B[{1}].GetLinearVelocity().IsNearlyZero(1.e-9));
    B[{1}].SetVelocities(FVector(-20,0,0),FVector::ZeroVector);
    auto R=FKartPhysicsSolver::Solve(B,{Contact(B)},1./120,S);
    TestTrue(TEXT("Separating contact no attraction"),R.bSuccess && R.Contacts[0].NormalImpulse==0 && B[{1}].GetLinearVelocity().X==-20);
    auto Invalid=Contact(B);Invalid.Points[0].WorldA.X+=1;
    TestFalse(TEXT("Stale geometry rejected"),FKartPhysicsSolver::Solve(B,{Invalid},1./120,S).bSuccess);
    TestEqual(TEXT("Failure atomic"),B[{1}].GetLinearVelocity().X,-20.);
    TestFalse(TEXT("Duplicate pairs rejected"),FKartPhysicsSolver::Solve(B,{Contact(B),Contact(B)},1./120,S).bSuccess);
    B[{1}].SetImmovable(true);
    TestTrue(TEXT("Two static bodies safely skipped"),FKartPhysicsSolver::Solve(B,{Contact(B)},1./120,S).bSuccess);
    // Rotating four-point box against a fixed floor, gravity integrated before solve.
    auto Rest=[&]()
    {
        TMap<FKartPhysicsBodyId,FKartRigidBodyState> X;
        X.Add({1},Body(2,FVector(0,0,19.9)));X.Add({2},Body(1,FVector::ZeroVector));X[{2}].SetImmovable(true);
        double MaxDepth=0,MaxSpeed=0,MaxResidual=0;int32 MaxPoints=0;
        bool Valid=true;
        for(int32 I=0;I<1200;++I)
        {
            auto& A=X[{1}];A.SetVelocities(A.GetLinearVelocity()+FVector(0,0,-980./120),A.GetAngularVelocity());
            auto M=KartManifolds::Build({{1},{2}},Shape(A),Shape(X[{2}]));
            for(const auto& P:M.Points)MaxDepth=FMath::Max(MaxDepth,P.Depth);
            MaxPoints=FMath::Max(MaxPoints,M.Points.Num());
            FKartSolverSettings RestSettings;RestSettings.Iterations=40;
            const auto Q=FKartPhysicsSolver::Solve(X,{M},1./120,RestSettings);Valid&=Q.bSuccess;
            for(const auto& C:Q.Contacts)MaxResidual=FMath::Max(MaxResidual,C.ResidualClosingSpeed);
            Valid&=FKartPhysicsIntegrator::Step(X[{1}],1./120);MaxSpeed=FMath::Max(MaxSpeed,X[{1}].GetLinearVelocity().Size());
        }
        TestTrue(TEXT("Rest simulation finite and valid"),Valid);
        TestEqual(TEXT("Four-point contact"),MaxPoints,4);
        TestTrue(TEXT("Penetration bounded"),MaxDepth<.11);
        TestTrue(TEXT("Rest speed bounded"),MaxSpeed<2);
        TestTrue(TEXT("Multipoint convergence"),MaxResidual<1.e-5);
        AddInfo(FString::Printf(TEXT("Rest 1200 steps: max depth %.9f cm, speed %.9f cm/s, residual %.9f cm/s"),MaxDepth,MaxSpeed,MaxResidual));
        return X[{1}];
    };
    const auto First=Rest(),Second=Rest();
    TestTrue(TEXT("Exact deterministic replay"),First.GetPosition()==Second.GetPosition() && First.GetLinearVelocity()==Second.GetLinearVelocity() && First.GetOrientation()==Second.GetOrientation());
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Pawn=World->SpawnActor<AKartPawn>();
    const auto Transform=Pawn->GetActorTransform();FKartPhysicsSolver::Solve(B,{},1./120);
    TestTrue(TEXT("Legacy unchanged"),Pawn->GetActorTransform().Equals(Transform) && Pawn->GetPhysicsBackend()==EKartPhysicsBackend::Legacy && !Pawn->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));
    return true;
}
#endif
