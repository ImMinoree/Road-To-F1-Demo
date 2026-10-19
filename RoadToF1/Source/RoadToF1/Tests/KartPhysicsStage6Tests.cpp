#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsSolver.h"
#include "KartPhysicsIntegrator.h"
#include "KartPhysicsManifoldBuilder.h"
namespace KartStage6
{
FKartRigidBodyState Make(FVector P,FVector V,bool Static=false)
{FKartRigidBodyState B;B.ConfigureBox(2,FVector(20));B.SetPose(P,FQuat::Identity);B.SetVelocities(V,FVector::ZeroVector);B.SetImmovable(Static);return B;}
FKartConvexShape Shape(const FKartRigidBodyState& B,FVector Extent=FVector(10))
{FKartConvexShape S;S.Position=B.GetPosition();S.Orientation=B.GetOrientation();S.HalfExtents=Extent;return S;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage6Test,"RoadToF1.Physics.Stage6Materials",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage6Test::RunTest(const FString&)
{
    using namespace KartStage6;
    for(double E:{0.,.5,1.})for(double Speed:{20.,500.})
    {
        TMap<FKartPhysicsBodyId,FKartRigidBodyState> B;
        B.Add({1},Make(FVector::ZeroVector,FVector(Speed,0,0)));B.Add({2},Make(FVector(19.9,0,0),FVector(-Speed,0,0)));
        auto M=KartManifolds::Build({{1},{2}},Shape(B[{1}]),Shape(B[{2}]));
        TMap<FKartPhysicsBodyId,FKartContactMaterial> Materials;Materials.Add({1},{0,0,E});Materials.Add({2},{0,0,E});
        FKartSolverSettings S;S.BiasFraction=0;S.Iterations=64;
        const auto R=FKartPhysicsSolver::Solve(B,{M},1./120,S,&Materials);
        TestTrue(TEXT("Restitution solve"),R.bSuccess);
        const double Expected=Speed*2>S.BounceThresholdCmPerSecond?-E*Speed:0;
        TestTrue(TEXT("Analytical bounce and threshold"),FMath::Abs(B[{1}].GetLinearVelocity().X-Expected)<1.e-5);
        TestTrue(TEXT("Energy bounded"),B[{1}].GetLinearVelocity().SizeSquared()+B[{2}].GetLinearVelocity().SizeSquared()<=2*Speed*Speed+1.e-4);
    }
    auto Slide=[&](double InitialSpeed,double Acceleration)
    {
        TMap<FKartPhysicsBodyId,FKartRigidBodyState> B;
        B.Add({1},Make(FVector(0,0,19.9),FVector(InitialSpeed,0,0)));B.Add({2},Make(FVector::ZeroVector,FVector::ZeroVector,true));
        TMap<FKartPhysicsBodyId,FKartContactMaterial> Mats;Mats.Add({1},{.8,.6,0});Mats.Add({2},{.8,.6,0});
        bool Valid=true,Bounds=true;double MaxZ=0;
        for(int I=0;I<600;++I)
        {
            auto& A=B[{1}];A.SetVelocities(A.GetLinearVelocity()+FVector(Acceleration,0,-980)/120.,A.GetAngularVelocity());
            auto M=KartManifolds::Build({{1},{2}},Shape(A),Shape(B[{2}],FVector(10000,10000,10)));
            FKartSolverSettings S;S.Iterations=64;
            const auto R=FKartPhysicsSolver::Solve(B,{M},1./120,S,&Mats);Valid&=R.bSuccess;
            for(const auto& C:R.Contacts)Bounds&=C.TangentImpulse.Size()<=.8*C.NormalImpulse+1.e-8;
            Valid&=FKartPhysicsIntegrator::Step(B[{1}],1./120);MaxZ=FMath::Max(MaxZ,FMath::Abs(B[{1}].GetLinearVelocity().Z));
        }
        TestTrue(TEXT("Sliding/static finite"),Valid);TestTrue(TEXT("Coulomb disk bounds"),Bounds);
        TestTrue(TEXT("No vertical jitter growth"),MaxZ<2);
        TestTrue(TEXT("Dynamic friction stops box"),B[{1}].GetLinearVelocity().Size()<.01);
        if(InitialSpeed==0)TestTrue(TEXT("Static friction holds subthreshold load"),FMath::Abs(B[{1}].GetPosition().X)<.01);
        AddInfo(FString::Printf(TEXT("Slide v0 %.1f a %.1f final x %.6f speed %.9f"),InitialSpeed,Acceleration,B[{1}].GetPosition().X,B[{1}].GetLinearVelocity().Size()));
        return B[{1}];
    };
    const auto A=Slide(200,0),B=Slide(200,0);Slide(0,100);
    TestTrue(TEXT("Deterministic friction replay"),A.GetPosition()==B.GetPosition() && A.GetOrientation()==B.GetOrientation());
    for(auto Surface:{EKartContactSurface::Asphalt,EKartContactSurface::Kerb,EKartContactSurface::Grass,EKartContactSurface::LowGrip,EKartContactSurface::Barrier,EKartContactSurface::Bodywork})
        TestTrue(TEXT("Bounded surface material"),FKartContactMaterial::Surface(Surface).IsValid());
    TestFalse(TEXT("Energy-generating restitution rejected"),FKartContactMaterial{.5,.4,1.1}.IsValid());
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> Stack;
    Stack.Add({1},Make(FVector(0,0,0),FVector::ZeroVector,true));
    Stack.Add({2},Make(FVector(0,0,19.9),FVector::ZeroVector));Stack.Add({3},Make(FVector(0,0,39.8),FVector::ZeroVector));
    TMap<FKartPhysicsBodyId,FKartContactMaterial> SM;
    for(uint64 Id=1;Id<=3;++Id)SM.Add({Id},{.8,.6,0});
    bool StackValid=true;double MaxPenetration=0;
    for(int I=0;I<600;++I)
    {
        for(uint64 Id=2;Id<=3;++Id){auto& V=Stack[{Id}];V.SetVelocities(V.GetLinearVelocity()+FVector(0,0,-980./120),V.GetAngularVelocity());}
        TArray<FKartContactManifold> Manifolds;
        for(uint64 Id=1;Id<=2;++Id){auto M=KartManifolds::Build({{Id},{Id+1}},Shape(Stack[{Id}]),Shape(Stack[{Id+1}]));
            for(const auto& P:M.Points)MaxPenetration=FMath::Max(MaxPenetration,P.Depth);Manifolds.Add(M);}
        FKartSolverSettings S;S.Iterations=96;
        StackValid&=FKartPhysicsSolver::Solve(Stack,Manifolds,1./120,S,&SM).bSuccess;
        for(uint64 Id=2;Id<=3;++Id)StackValid&=FKartPhysicsIntegrator::Step(Stack[{Id}],1./120);
    }
    TestTrue(TEXT("Resting stack valid bounded"),StackValid && MaxPenetration<.15 && Stack[{3}].GetLinearVelocity().Size()<.01);
    AddInfo(FString::Printf(TEXT("Stack max penetration %.9f final speed %.9f"),MaxPenetration,Stack[{3}].GetLinearVelocity().Size()));
    // Oblique impact has nonzero tangential and angular momentum transfer, no friction energy gain.
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> O;
    O.Add({1},Make(FVector::ZeroVector,FVector(200,80,30)));O.Add({2},Make(FVector(19.9,0,0),FVector::ZeroVector,true));
    auto OM=KartManifolds::Build({{1},{2}},Shape(O[{1}]),Shape(O[{2}]));FKartSolverSettings OS;OS.BiasFraction=0;OS.Iterations=96;
    auto OR=FKartPhysicsSolver::Solve(O,{OM},1./120,OS,&SM);
    const auto& OB=O[{1}];const auto LocalW=OB.GetOrientation().UnrotateVector(OB.GetAngularVelocity());
    const double FinalEnergy=OB.GetMass()*OB.GetLinearVelocity().SizeSquared()+FVector::DotProduct(LocalW,OB.GetLocalInertia().TransformVector(LocalW));
    TestTrue(TEXT("Oblique dissipative energy"),OR.bSuccess && FinalEnergy<=2*(200*200+80*80+30*30)+1.e-4);
    return true;
}
#endif
