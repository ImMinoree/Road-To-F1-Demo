#pragma once
#include "KartCustomSceneBridge.h"
#include "KartPhysicsBroadphase.h"
#include "KartPhysicsManifoldCache.h"
#include "KartPhysicsSolver.h"
class UWorld;
class AActor;
class FKartStaticMeshGeometry;

struct FKartFleetBody
{
    FKartPhysicsBodyId Id;
    FKartExperimentalVehicle Vehicle;
    FKartVehicleInput Input;
    const AActor* Ignore=nullptr; // Only used during a synchronous world-owned step.
    FVector Impulse=FVector::ZeroVector;
    FVector SharedImpulse=FVector::ZeroVector,SceneImpulse=FVector::ZeroVector;
};
struct FKartFleetContactDiagnostic
{
    FKartSolvedContact Contact;
    double Depth=0,AllowedClosingSpeed=0;
    FString Source;
};
struct FKartFleetDiagnostics
{
    uint64 Steps=0,PairsSolved=0,StaticPairsSolved=0,ContactRows=0,ReusedContacts=0;
    int32 LastPairs=0,LastCandidates=0,DuplicatePairs=0;
    double MaxStepMilliseconds=0,TotalStepMilliseconds=0,MaxCurrentPenetrationCm=0;
    double PrepareMilliseconds=0,GeometryMilliseconds=0,SolverMilliseconds=0,MotionMilliseconds=0;
    double LastNormalResidual=0,MaxNormalResidual=0;
    double MaxActualBoxOverlapCm=0;
    FString DeepestDynamicPair;
    int32 LastIterations=0;
    uint64 AdaptiveSolves=0;
    uint64 RefinedSteps=0;
    int32 MaxRefinement=0;
    uint64 WorldSteps=0;
    double TotalWorldStepMilliseconds=0,MaxWorldStepMilliseconds=0;
};
/** One shared, deterministic velocity constraint batch per fixed step.
 * Public value inputs facilitate replay tests; world subsystem owns scheduling/actors.
 * No independent per-pawn clocks. Static responses use the same custom ownership. */
class ROADTOF1_API FKartCustomFleet
{
public:
    TArray<FKartFleetBody> Bodies;
    bool bDetailedDiagnostics=false;
    bool bCaptureContactDiagnostics=false;
    bool bProfileSteps=false;
    TArray<double> StepMilliseconds;
    TArray<FKartFleetContactDiagnostic> LastContacts;
    FString LastConstraintSummary;
    bool Step(const UWorld& World,const FKartStaticMeshGeometry& Scene,double Dt,FString& Failure);
    bool Remove(FKartPhysicsBodyId Id);
    void ResetCaches();
    const FKartFleetDiagnostics& GetDiagnostics() const {return Diagnostics;}
    const FKartPhysicsBroadphase& GetBroadphase() const {return Tree;}
private:
    bool StepRefined(const UWorld& World,const FKartStaticMeshGeometry& Scene,double Dt,FString& Failure,int32 Level);
    bool StepOnce(const UWorld& World,const FKartStaticMeshGeometry& Scene,double Dt,FString& Failure);
    FKartPhysicsBroadphase Tree;
    FKartPhysicsManifoldCache Cache;
    FKartFleetDiagnostics Diagnostics;
};
