#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KartCollisionProvider.h"
#include "KartPhysicsBroadphase.h"
#include "KartPhysicsBackend.h"
#include "KartExperimentalVehicle.h"
#include "KartFixedStepClock.h"
#include "KartPhysicsViewFeedback.h"
#include "KartCustomFleet.h"
#include "KartStaticMeshGeometry.h"
#include "KartPhysicsWorldSubsystem.generated.h"
class AKartPawn;

/** Option B exclusive ownership boundary:
 * Custom owns position/orientation/velocity/forces/impulses and final transform.
 * Unreal provides geometry queries only. Custom karts must not simulate in Unreal.
 * Never combine custom response with legacy swept movement/impact rotation.
 * The shared coordinator solves each canonical kart pair once per accepted integration slice; rejected trials never publish response.
 * Incident consumers may display/count events, not add physical impulses/rotations.
 * Explicit fleet selection activates a world-level120Hz clock; pawns skip Legacy.
 * Legacy remains default. Restricted single-body fixtures retain a separate clock.
 * Validated experimental admission requires explicit development opt-in.
 */
UCLASS()
class ROADTOF1_API UKartPhysicsWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaSeconds) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;
    const IKartCollisionProvider& GetCollisionProvider() const { return Provider; }
    bool ProbeKart(const AKartPawn& Kart, float Distance, bool bDraw, FHitResult& Hit) const;
    FKartPhysicsBodyId AllocateExperimentalBodyId() { return ExperimentalIds.Allocate(); }
    FKartPhysicsBroadphase& GetExperimentalBroadphase() { return ExperimentalBroadphase; }
    const FKartPhysicsBroadphase& GetExperimentalBroadphase() const { return ExperimentalBroadphase; }
    // Stage9 restricted test path only. No SouthGarda/race-director/mixed-backend admission.
    bool SelectTestBackend(AKartPawn& Kart,EKartPhysicsBackend Requested);
    void AdvanceTestKart(AKartPawn& Kart,double FrameSeconds);
    void ResetTestKart(AKartPawn& Kart);
    bool SelectFleet(const TArray<AKartPawn*>& Karts,bool bExplicitDevelopmentOptIn=false);
    bool ReleaseFleet();
    bool IsFleetKart(const AKartPawn& Kart) const;
    bool AdvanceCustomWorld(double FrameSeconds);
    bool HasFleetFault() const {return bFleetFault;}
    const FString& GetFleetFailure() const {return FleetFailure;}
    const FKartCustomFleet& GetCustomFleet() const {return Fleet;}
    void SetFleetContactDiagnostics(bool Enabled){Fleet.bCaptureContactDiagnostics=Enabled;}
    void SetFleetProfiling(bool Enabled){Fleet.bProfileSteps=Enabled;}
    bool GetStaticChassisQuery(const AKartPawn& Kart,FKartConvexShape& Shape) const;
    bool CanTranslateStaticChassis(const AKartPawn& Kart,const FVector& Delta) const;
    double GetDroppedCustomTime() const {return FleetClock.DroppedSeconds;}
    // Stage9/10 validated: explicit experimental admission only; Legacy remains default.
    static constexpr bool IsExperimentalRaceReady(){return true;}
private:
    struct FLiveKart
    {
        TWeakObjectPtr<AKartPawn> Pawn;
        FKartPhysicsBodyId Id;
        FKartPhysicsViewFeedback View;
        FTransform PreviousPose;
        bool bFirstPerson=true,bRetired=false;
    };
    void ResetFleetKart(AKartPawn& Kart);
    TArray<FLiveKart> FleetActors;
    FKartCustomFleet Fleet;
    FKartStaticMeshGeometry FleetScene;
    FKartFixedStepClock FleetClock;
    bool bFleetFault=false;
    FString FleetFailure;
    FUnrealKartCollisionProvider Provider;
    FKartPhysicsBodyIdSource ExperimentalIds;
    FKartPhysicsBroadphase ExperimentalBroadphase;
    TWeakObjectPtr<AKartPawn> TestKart;
    FKartExperimentalVehicle TestVehicle;
    FKartFixedStepClock TestClock;
    FKartPhysicsViewFeedback TestView;
    FTransform PreviousTestPose;
    bool bTestFault=false;
    bool bTestFirstPerson=true;
};
