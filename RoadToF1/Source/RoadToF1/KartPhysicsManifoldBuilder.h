#pragma once
#include "KartPhysicsContactManifold.h"

namespace KartManifolds
{
    // Shape arguments MUST correspond to canonical Pair.First/Second.
    ROADTOF1_API FKartContactManifold Build(FKartPhysicsPair Pair,const FKartConvexShape& A,
        const FKartConvexShape& B,const FKartManifoldSettings& Settings={},const FKartContactManifold* Previous=nullptr);
    ROADTOF1_API FVector ToWorld(const FKartConvexShape& S,const FVector& Local);
    ROADTOF1_API FVector ToLocal(const FKartConvexShape& S,const FVector& World);
    ROADTOF1_API bool ValidContact(const FKartContactManifold& M,const FKartConvexShape& A,const FKartConvexShape& B);
}
