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

/** Option B ownership boundary (future custom backend):
 * Custom owns position/orientation/velocity/forces/impulses and final transform.
 * Unreal provides geometry queries only. Custom karts must not simulate in Unreal.
 * Never combine custom response with legacy swept movement/impact rotation.
 * This shared coordinator will solve each canonical kart pair once per step.
 * Incident consumers may display/count events, not add physical impulses/rotations.
 * Stage9 adds one explicitly admitted test kart, advanced by its Pawn tick through this coordinator.
 * The broadphase remains dormant until shared multi-kart admission is implemented and validated.
 * No automatic world tick or default Custom registration. Production race admission is rejected.
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
    double GetDroppedCustomTime() const {return FleetClock.DroppedSeconds;}
    // Closed until all Stage9 admission gates pass; never changes the Legacy default.
    static constexpr bool IsExperimentalRaceReady(){return false;}
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
