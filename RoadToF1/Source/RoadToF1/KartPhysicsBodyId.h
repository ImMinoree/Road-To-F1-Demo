#pragma once
#include "CoreMinimal.h"

/** Session/world-scoped value handle, never an address or reusable node index. Zero is invalid. */
struct FKartPhysicsBodyId
{
    uint64 Value = 0;
    bool IsValid() const { return Value != 0; }
    bool operator==(const FKartPhysicsBodyId& Other) const { return Value == Other.Value; }
    bool operator<(const FKartPhysicsBodyId& Other) const { return Value < Other.Value; }
    friend uint32 GetTypeHash(const FKartPhysicsBodyId& Id) { return ::GetTypeHash(Id.Value); }
};

/** Keep one source per experimental world. Never reset/reuse IDs during that world's lifetime. */
class FKartPhysicsBodyIdSource
{
public:
    FKartPhysicsBodyId Allocate()
    {
        if (Next == 0) return {}; // Exhaustion fails closed, without wrapping to an old ID.
        return {Next++};
    }
private:
    uint64 Next = 1;
};
