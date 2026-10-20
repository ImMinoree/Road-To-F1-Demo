#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "KartPhysicsIntegrator.h"
#include <limits>

namespace KartStage1Tests
{
bool Same(const FKartRigidBodyState& A, const FKartRigidBodyState& B)
{
    const auto& I = A.GetLocalInertia(); const auto& J = B.GetLocalInertia();
    const auto& U = A.GetLocalInverseInertia(); const auto& V = B.GetLocalInverseInertia();
    return A.GetPosition() == B.GetPosition() && A.GetOrientation() == B.GetOrientation()
        && A.GetLinearVelocity() == B.GetLinearVelocity() && A.GetAngularVelocity() == B.GetAngularVelocity()
        && A.GetForce() == B.GetForce() && A.GetTorque() == B.GetTorque()
        && A.GetMass() == B.GetMass() && A.GetInverseMass() == B.GetInverseMass()
        && A.IsImmovable() == B.IsImmovable() && I.X == J.X && I.Y == J.Y && I.Z == J.Z
        && U.X == V.X && U.Y == V.Y && U.Z == V.Z;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage1BodyTest, "RoadToF1.Physics.Stage1RigidBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKartStage1BodyTest::RunTest(const FString& Parameters)
{
    using namespace KartStage1Tests;
    constexpr double Tol = 1.e-9;
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Inf = std::numeric_limits<double>::infinity();
    FKartRigidBodyState Body;
    TestTrue(TEXT("Default finite state"), Body.IsValid());
    TestTrue(TEXT("Default identity orientation"), Body.GetOrientation() == FQuat::Identity);
    TestTrue(TEXT("Default zero position/motion/forces"), Body.GetPosition().IsZero()
        && Body.GetLinearVelocity().IsZero() && Body.GetAngularVelocity().IsZero()
        && Body.GetForce().IsZero() && Body.GetTorque().IsZero());
    TestTrue(TEXT("Configure known box"), Body.ConfigureBox(12, FVector(2,4,6)));
    TestTrue(TEXT("Inverse mass"), FMath::IsNearlyEqual(Body.GetInverseMass(), 1.0/12, Tol));
    TestTrue(TEXT("Box inertia kg cm^2"), Body.GetLocalInertia().TransformVector(FVector(1)).Equals(FVector(52,40,20), Tol));
    TestTrue(TEXT("Box inverse inertia"), Body.GetLocalInverseInertia().TransformVector(FVector(1)).Equals(FVector(1.0/52,1.0/40,1.0/20), Tol));
    const auto ValidBox = Body;
    for (double Bad : {0.0, -1.0, NaN, Inf, -Inf, std::numeric_limits<double>::denorm_min()})
    {
        TestFalse(TEXT("Reject invalid/unrepresentable mass"), Body.ConfigureBox(Bad, FVector(2,4,6)));
        TestTrue(TEXT("Rejected mass leaves all state unchanged"), Same(Body,ValidBox));
    }
    for (FVector Bad : {FVector(0,1,1), FVector(-1,1,1), FVector(NaN,1,1), FVector(Inf), FVector(1.e300)})
    {
        TestFalse(TEXT("Reject invalid/overflow dimensions"), Body.ConfigureBox(12,Bad));
        TestTrue(TEXT("Rejected size leaves state unchanged"), Same(Body,ValidBox));
    }
    TestTrue(TEXT("Rotated pose accepted"), Body.SetPose(FVector::ZeroVector,FQuat(FVector(0,0,1),PI/4)));
    const auto WorldI = Body.GetWorldInverseInertia();
    TestTrue(TEXT("World tensor rotates, including off diagonal"), WorldI.X.Equals(
        FVector((1.0/52+1.0/40)/2,(1.0/52-1.0/40)/2,0),Tol));
    TestTrue(TEXT("World tensor symmetric"), FMath::IsNearlyEqual(WorldI.X.Y,WorldI.Y.X,Tol));
    TestTrue(TEXT("World tensor Z principal axis"), WorldI.Z.Equals(FVector(0,0,1.0/20),Tol));

    Body = FKartRigidBodyState();
    TestTrue(TEXT("Force first accumulation"), Body.AddForce(FVector(1,2,3)));
    TestTrue(TEXT("Force second accumulation"), Body.AddForce(FVector(4,5,6)));
    TestTrue(TEXT("Forces sum"), Body.GetForce() == FVector(5,7,9));
    TestTrue(TEXT("Torque first accumulation"), Body.AddTorque(FVector(1,2,3)));
    TestTrue(TEXT("Torque second accumulation"), Body.AddTorque(FVector(3,2,1)));
    TestTrue(TEXT("Torques sum"), Body.GetTorque() == FVector(4,4,4));
    TestTrue(TEXT("Force addition does not move body"), Body.GetPosition().IsZero() && Body.GetOrientation() == FQuat::Identity);
    Body.ClearAccumulators();
    Body.SetPose(FVector(10,20,30),FQuat::Identity);
    TestTrue(TEXT("Off-centre force accepted"), Body.AddForceAtPoint(FVector(0,100,0),FVector(12,20,30)));
    TestTrue(TEXT("Off-centre linear force"), Body.GetForce() == FVector(0,100,0));
    TestTrue(TEXT("r cross F about COM"), Body.GetTorque() == FVector(0,0,200));
    Body.AddForceAtPoint(FVector(0,100,0),Body.GetPosition());
    TestTrue(TEXT("Force at COM adds no torque"), Body.GetTorque() == FVector(0,0,200));
    TestTrue(TEXT("Step succeeds"), FKartPhysicsIntegrator::Step(Body,1.0/120));
    TestTrue(TEXT("Step clears both accumulators"), Body.GetForce().IsZero() && Body.GetTorque().IsZero());

    Body = FKartRigidBodyState(); Body.ConfigureBox(2,FVector(100));
    Body.AddForce(FVector(1000,0,0)); // 10 N = 1000 kg*cm/s^2.
    TestTrue(TEXT("Analytical example step"), FKartPhysicsIntegrator::Step(Body,.1));
    TestTrue(TEXT("10N / 2kg: velocity 50 cm/s"), Body.GetLinearVelocity().Equals(FVector(50,0,0),Tol));
    TestTrue(TEXT("Semi-implicit example: position 5 cm, not zero"), Body.GetPosition().Equals(FVector(5,0,0),Tol));
    FKartPhysicsIntegrator::Step(Body,.1);
    TestTrue(TEXT("No repeated force or implicit gravity"), Body.GetLinearVelocity().Equals(FVector(50,0,0),Tol)
        && Body.GetPosition().Equals(FVector(10,0,0),Tol));
    AddInfo(TEXT("Analytic check: 2kg, 10N, dt=0.1s -> v=50cm/s, x=5cm (semi-implicit Euler)."));
    for (int32 Steps : {60,120})
    {
        FKartRigidBodyState Run; Run.ConfigureBox(2,FVector(100));
        const double Dt=1.0/Steps;
        bool Valid=true;
        for(int32 I=0;I<Steps;++I) Valid &= Run.AddForce(FVector(1000,0,0)) && FKartPhysicsIntegrator::Step(Run,Dt);
        TestTrue(TEXT("Constant force sequence valid"),Valid);
        TestTrue(TEXT("One second final velocity"),Run.GetLinearVelocity().Equals(FVector(500,0,0),Tol));
        TestTrue(TEXT("Discrete analytical position"),FMath::IsNearlyEqual(Run.GetPosition().X,250.0*(1+Dt),Tol));
        AddInfo(FString::Printf(TEXT("Fixed dt=1/%d: x=%.9fcm, v=%.9fcm/s; continuous x=250cm; first-order position error expected."),Steps,Run.GetPosition().X,Run.GetLinearVelocity().X));
    }

    Body.SetImmovable(true);
    const auto StaticPose=Body.GetPosition();
    const auto StaticRotation=Body.GetOrientation();
    TestEqual(TEXT("Static inverse mass zero"),Body.GetInverseMass(),0.0);
    TestTrue(TEXT("Static inverse inertia zero"),Body.GetWorldInverseInertia().TransformVector(FVector(1)).IsZero());
    TestFalse(TEXT("Static cannot accept moving velocity"),Body.SetVelocities(FVector(1),FVector::ZeroVector));
    Body.AddForce(FVector(1000)); Body.AddTorque(FVector(1000));
    TestTrue(TEXT("Static step valid"),FKartPhysicsIntegrator::Step(Body,.1));
    TestTrue(TEXT("Static pose/motion unchanged"),Body.GetPosition()==StaticPose && Body.GetOrientation()==StaticRotation
        && Body.GetLinearVelocity().IsZero() && Body.GetAngularVelocity().IsZero());
    TestTrue(TEXT("Static accumulators cleared"),Body.GetForce().IsZero() && Body.GetTorque().IsZero());
    Body.SetImmovable(false);
    TestTrue(TEXT("Dynamic inverse properties restored"),Body.GetInverseMass()==.5 && Body.GetLocalInverseInertia().X.X>0);

    Body=FKartRigidBodyState(); Body.ConfigureBox(12,FVector(2,4,6));
    Body.AddTorque(FVector(0,0,20)); // Izz=20 => alpha=1 rad/s^2.
    FKartPhysicsIntegrator::Step(Body,.1);
    TestTrue(TEXT("Known torque angular velocity"),Body.GetAngularVelocity().Equals(FVector(0,0,.1),Tol));
    TestTrue(TEXT("Positive Z rotation turns X toward Y"),Body.GetOrientation().RotateVector(FVector(1,0,0)).Equals(FVector(FMath::Cos(.01),FMath::Sin(.01),0),Tol));
    const auto Rotated=Body.GetOrientation();
    Body.SetVelocities(FVector::ZeroVector,FVector::ZeroVector);
    FKartPhysicsIntegrator::Step(Body,.1);
    TestTrue(TEXT("Zero torque AND zero omega preserve orientation"),Body.GetOrientation()==Rotated);
    Body.SetVelocities(FVector::ZeroVector,FVector(0,0,1));
    FKartPhysicsIntegrator::Step(Body,.1);
    TestTrue(TEXT("Nonzero omega rotates even with zero torque"),!Body.GetOrientation().Equals(Rotated,Tol));
    TestTrue(TEXT("Torque-free omega retained (Stage1 simplification)"),Body.GetAngularVelocity()==FVector(0,0,1));

    Body=ValidBox; Body.SetPose(FVector::ZeroVector,FQuat(FVector(0,0,1),PI/4));
    const FVector ExpectedAlpha=Body.GetWorldInverseInertia().TransformVector(FVector(10,0,0));
    Body.AddTorque(FVector(10,0,0)); FKartPhysicsIntegrator::Step(Body,.1);
    TestTrue(TEXT("Integrator uses rotated tensor"),Body.GetAngularVelocity().Equals(ExpectedAlpha*.1,Tol));

    FKartRigidBodyState A=ValidBox, B=ValidBox;
    A.SetVelocities(FVector(10,20,30),FVector(.1,.2,.3)); B=A;
    bool LongValid=true, Normalized=true;
    for(int32 I=0;I<10000;++I)
    {
        for(auto* Run:{&A,&B})
        {
            LongValid &= Run->AddForce(FVector(1,2,3)) && Run->AddTorque(FVector(0,0,.01))
                && FKartPhysicsIntegrator::Step(*Run,1.0/120) && Run->IsValid();
            Normalized &= FMath::Abs(Run->GetOrientation().SizeSquared()-1)<1.e-12;
        }
    }
    TestTrue(TEXT("10000-step finite runs"),LongValid);
    TestTrue(TEXT("Quaternion remains normalized"),Normalized);
    TestTrue(TEXT("Independent fixed-step runs exactly reproducible on this build"),Same(A,B));

    Body=ValidBox; Body.AddForce(FVector(1,2,3)); Body.AddTorque(FVector(4,5,6));
    const auto Snapshot=Body;
    for(double Bad:{0.0,-.1,.100001,NaN,Inf})
    {
        TestFalse(TEXT("Invalid dt rejected"),FKartPhysicsIntegrator::Step(Body,Bad));
        TestTrue(TEXT("Invalid dt preserves ALL state including accumulators"),Same(Body,Snapshot));
    }
    TestFalse(TEXT("NaN force rejected"),Body.AddForce(FVector(NaN,0,0)));
    TestFalse(TEXT("Inf torque rejected"),Body.AddTorque(FVector(0,Inf,0)));
    TestFalse(TEXT("Invalid point rejected atomically"),Body.AddForceAtPoint(FVector(1),FVector(Inf)));
    TestFalse(TEXT("Invalid velocity rejected"),Body.SetVelocities(FVector(Inf),FVector::ZeroVector));
    TestFalse(TEXT("Zero quaternion rejected"),Body.SetPose(FVector::ZeroVector,FQuat(0,0,0,0)));
    TestFalse(TEXT("NaN quaternion rejected"),Body.SetPose(FVector::ZeroVector,FQuat(NaN,0,0,1)));
    TestTrue(TEXT("Invalid setters preserve state"),Same(Body,Snapshot));
    Body=FKartRigidBodyState(); Body.AddForce(FVector(std::numeric_limits<double>::max(),0,0));
    const auto Large=Body;
    TestFalse(TEXT("Accumulator overflow rejected"),Body.AddForce(Body.GetForce()));
    TestTrue(TEXT("Overflow does not partially write accumulator"),Same(Body,Large));
    Body=FKartRigidBodyState(); Body.ConfigureBox(.01,FVector(1)); Body.AddForce(FVector(1.e308,0,0));
    const auto OverflowStep=Body;
    TestFalse(TEXT("Integration overflow rejected"),FKartPhysicsIntegrator::Step(Body,.1));
    TestTrue(TEXT("Failed integration is transactional"),Same(Body,OverflowStep));
    return true;
}
#endif
