#pragma once
#include "KartCustomSceneBridge.h"
#include "KartStaticMeshGeometry.h"
namespace KartMeshMotion
{
    // Conservative finite triangle/OBB sweep; useful for independent CCD fixtures.
    bool CastTriangle(const FKartConvexShape& Start,const FKartConvexShape& End,const FKartStaticTriangle& Triangle,double& Time,bool& Hit,FString* Failure=nullptr);
    // Vehicle velocities/wheels are already force-integrated. Pose is advanced once here.
    bool Advance(const FKartStaticMeshGeometry& Scene,FKartExperimentalVehicle& Vehicle,
        const FVector& Start,const FQuat& Rotation,double Dt,FVector& Impulse,FString* Failure,
        FKartSceneStepStats* Stats,const FKartContactMaterial& Material);
}
