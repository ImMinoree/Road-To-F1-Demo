#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "KartPhysicsIntegrator.h"
#include "HAL/IConsoleManager.h"

// Pure numeric demonstration: no world, actor references, registry or transform writes.
static FAutoConsoleCommand BodyProbeCommand(
    TEXT("RoadToF1.Physics.BodyProbe"), TEXT("Isolated 2kg box, 10N X force, 120 fixed steps; logs numeric results only."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FKartRigidBodyState Body;
        bool Valid=Body.ConfigureBox(2,FVector(100));
        for(int32 I=0;I<120 && Valid;++I)
            Valid=Body.AddForce(FVector(1000,0,0)) && FKartPhysicsIntegrator::Step(Body,1.0/120);
        UE_LOG(LogTemp,Display,TEXT("Stage1 isolated BodyProbe: valid=%d mass=%.2fkg Ixx=%.6fkg*cm^2 initialV=0 force=1000kg*cm/s^2 dt=1/120 steps=120 finalX=%.9fcm finalVx=%.9fcm/s. No live kart touched."),
            Valid,Body.GetMass(),Body.GetLocalInertia().X.X,Body.GetPosition().X,Body.GetLinearVelocity().X);
    }));
#endif
