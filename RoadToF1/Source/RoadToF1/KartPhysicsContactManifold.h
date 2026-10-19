#pragma once
#include "KartPhysicsNarrowphase.h"

enum class EKartManifoldStatus : uint8 { NoContact, Valid, InvalidInput, GeometryFailure, Unsupported };
enum class EKartManifoldMethod : uint8 { None, SphereAnalytic, SphereBoxAnalytic, BoxClip, EPAWitness };
/** cm, shape origin == body origin, no scale/offset. Positive Depth means penetration;
 * WorldA-WorldB = Normal*Depth. Age counts matched manual updates, not seconds.
 * No impulse fields or solver behavior in Stage4. */
struct FKartContactPoint
{
    FVector WorldA=FVector::ZeroVector,WorldB=FVector::ZeroVector;
    FVector LocalA=FVector::ZeroVector,LocalB=FVector::ZeroVector;
    double Depth=0;
    uint64 Feature=0,PersistentId=0;
    uint32 Age=0;
};
struct FKartContactManifold
{
    FKartPhysicsPair Pair;
    FVector Normal=FVector::ZeroVector;
    TArray<FKartContactPoint,TInlineAllocator<4>> Points;
    EKartManifoldStatus Status=EKartManifoldStatus::NoContact;
    EKartManifoldMethod Method=EKartManifoldMethod::None;
    EKartGJKStatus GJK=EKartGJKStatus::InvalidInput;
    EKartEPAStatus EPA=EKartEPAStatus::NotRun;
    bool bAmbiguousNormal=false;
    bool bNarrowphaseRun=false; // Analytical methods do not claim GJK/EPA execution.
    uint32 Age=0;
    int32 CandidateCount=0;
};
struct FKartManifoldSettings
{
    int32 MaxContacts=4;
    double DedupCm=.001,AnchorDriftCm=.5,FeatureMatchCm=1.,SeparationCm=.02;
    double NormalMatchCos=.95; // Dimensionless normal-alignment threshold.
    FKartNarrowphaseSettings Narrowphase;
    bool IsValid() const;
};
