#pragma once
#include "KartExperimentalVehicle.h"
class UWorld;
class AActor;
class FKartStaticMeshGeometry;
/** Optional caller-owned diagnostic counters; no shipping Tick logging. */
struct FKartSceneStepStats
{
    int32 Contacts=0,Iterations=0,OutOfIntervalHits=0,InitialOverlaps=0;
    int32 TriangleCandidates=0,MeshSweeps=0;
    double MaxPenetrationCm=0,MaxResidualClosingSpeed=0;
};
/** Restricted single-kart bridge. Unreal discovers geometry; custom code owns response.
 * Exact UBoxComponent endpoint manifolds plus translational TOI mitigation.
 * Mesh adapters, full rotational CCD, cached impulses and multi-kart readiness remain absent. */
class ROADTOF1_API FKartCustomSceneBridge
{
public:
    // Full-track collision coverage and integration are not yet validated. Automation may exercise the draft;
    // ordinary PIE/Development/Shipping must not admit this backend until the regression passes.
    static constexpr bool IsGameplayReady(){return false;}
    static bool Step(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,
        const FKartVehicleInput& Input,double Dt,FVector& ImpactImpulse,FString* Failure=nullptr,
        FKartSceneStepStats* Stats=nullptr,const FKartContactMaterial& Material={},const FKartStaticMeshGeometry* Scene=nullptr);
    // Coordinator phases: forces update wheel/velocity state, but never advance pose.
    static bool Prepare(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,const FKartVehicleInput& Input,double Dt);
    static bool HasGroundSupport(const UWorld& World,const AActor* Ignore,const FKartExperimentalVehicle& Vehicle);
    // Advance already force-integrated state; never apply engine/tire forces again.
    static bool Move(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,double Dt,
        FVector& Impulse,FString* Failure,FKartSceneStepStats* Stats,const FKartContactMaterial& Material,const FKartStaticMeshGeometry& Scene);
private:
    static bool Advance(const UWorld& World,const AActor* Ignore,FKartExperimentalVehicle& Vehicle,
        const FKartVehicleInput& Input,double Dt,FVector& ImpactImpulse,FString* Failure,int32 Depth,
        FKartSceneStepStats* Stats,const FKartContactMaterial& Material,bool bApplyForces=true,int32 MotionIterations=0,const FKartStaticMeshGeometry* Scene=nullptr);
};
