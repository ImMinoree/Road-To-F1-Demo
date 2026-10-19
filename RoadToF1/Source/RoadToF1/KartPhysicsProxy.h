#pragma once
#include "KartPhysicsBodyId.h"
#include "KartPhysicsAABB.h"

enum class EKartProxyMobility : uint8 { Dynamic, Static };
namespace KartCollisionCategory
{
    constexpr uint32 Kart=1u<<0, TrackStatic=1u<<1, Barrier=1u<<2, Prop=1u<<3, All=0xffffffffu;
}
/** Value-only broadphase metadata: no UObject/body pointer, contact or solver state. */
struct FKartPhysicsProxy
{
    FKartPhysicsBodyId Id;
    FKartPhysicsAABB Bounds;
    EKartProxyMobility Mobility=EKartProxyMobility::Dynamic;
    uint32 Category=KartCollisionCategory::Kart, Mask=KartCollisionCategory::All;
    bool bEnabled=true;
    bool IsValid() const { return Id.IsValid() && Bounds.IsValid()
        && (Mobility==EKartProxyMobility::Dynamic || Mobility==EKartProxyMobility::Static); }
    static bool AllowsPair(const FKartPhysicsProxy& A, const FKartPhysicsProxy& B)
    {
        return A.Id.IsValid() && B.Id.IsValid() && !(A.Id==B.Id) && A.bEnabled && B.bEnabled
            && !(A.Mobility==EKartProxyMobility::Static && B.Mobility==EKartProxyMobility::Static)
            && (A.Category&B.Mask)!=0 && (B.Category&A.Mask)!=0;
    }
};
