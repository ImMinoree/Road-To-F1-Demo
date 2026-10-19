#pragma once
#include "KartPhysicsBodyId.h"

/** Canonical candidate IDs only; does not imply exact collision. */
class FKartPhysicsPair
{
public:
    FKartPhysicsPair() = default;
    FKartPhysicsPair(FKartPhysicsBodyId X, FKartPhysicsBodyId Y) : A(Y<X?Y:X), B(Y<X?X:Y) {}
    FKartPhysicsBodyId First() const { return A; }
    FKartPhysicsBodyId Second() const { return B; }
    bool IsValid() const { return A.IsValid() && B.IsValid() && A<B; }
    bool operator==(const FKartPhysicsPair& P) const { return A==P.A && B==P.B; }
    bool operator<(const FKartPhysicsPair& P) const { return A<P.A || (A==P.A && B<P.B); }
    friend uint32 GetTypeHash(const FKartPhysicsPair& P) { return HashCombine(GetTypeHash(P.A),GetTypeHash(P.B)); }
private:
    FKartPhysicsBodyId A,B;
};
