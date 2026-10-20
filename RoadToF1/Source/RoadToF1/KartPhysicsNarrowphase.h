#pragma once
#include "KartPhysicsConvexShape.h"
#include "KartPhysicsBroadphase.h"

enum class EKartGJKStatus : uint8 { InvalidInput, Separated, Touching, Intersecting, Degenerate, IterationLimit };
enum class EKartEPAStatus : uint8 { NotRun, Converged, InvalidSeed, Degenerate, IterationLimit, CapacityLimit };
struct FKartSupportPoint
{
    FVector Minkowski=FVector::ZeroVector, PointOnA=FVector::ZeroVector, PointOnB=FVector::ZeroVector;
    FVector Direction=FVector::ZeroVector; // Allows EPA to reject seeds from another shape snapshot.
};
namespace KartNarrowphaseNumerics
{
    // Dimensionless conditioning guards except the explicitly named cm error.
    constexpr double DirectionSquaredMinimum=1.e-28;
    constexpr double GramRelativeMinimum=1.e-14;
    constexpr double TetraRelativeMinimum=1.e-12;
    constexpr double SeedWeightMinimum=1.e-10;
    constexpr double BarycentricSlack=1.e-8;
    constexpr double WorldWitnessRoundoffCm=1.e-7;
    constexpr double VisibleToleranceFraction=.1;
}
struct FKartNarrowphaseSettings
{
    int32 GJKIterations=64,EPAIterations=256;
    // cm absolute + shape scale relative (independent of world translation).
    double AbsoluteTolerance=1.e-7,RelativeTolerance=1.e-9;
    double EPARelativeTolerance=1.e-5;
    bool IsValid() const;
};
struct FKartGJKResult
{
    EKartGJKStatus Status=EKartGJKStatus::InvalidInput;
    FKartSupportPoint Simplex[4];int32 Count=0,Iterations=0;
    double Tolerance=0;
};
/** Normal points A->B: PointOnA-PointOnB ~= Normal*Depth; moving A by -Normal*Depth
 * would separate it. No movement is performed. Symmetric normals are non-unique.
 * Only bGeometryValid permits consumption of normal/depth/witnesses. */
struct FKartContactGeometry
{
    FKartPhysicsPair Pair;
    EKartGJKStatus GJK=EKartGJKStatus::InvalidInput;
    EKartEPAStatus EPA=EKartEPAStatus::NotRun;
    FVector Normal=FVector::ZeroVector,PointOnA=FVector::ZeroVector,PointOnB=FVector::ZeroVector;
    double Depth=0;int32 GJKIterations=0,EPAIterations=0;
    bool bGeometryValid=false;
};
namespace KartNarrowphase
{
    ROADTOF1_API bool SupportMinkowski(const FKartConvexShape& A,const FKartConvexShape& B,const FVector& D,FKartSupportPoint& Out);
    ROADTOF1_API FKartGJKResult GJK(const FKartConvexShape& A,const FKartConvexShape& B,const FKartNarrowphaseSettings& Settings={});
    ROADTOF1_API FKartContactGeometry EPA(const FKartConvexShape& A,const FKartConvexShape& B,const FKartGJKResult& Seed,const FKartNarrowphaseSettings& Settings={});
    ROADTOF1_API FKartContactGeometry Query(const FKartConvexShape& A,const FKartConvexShape& B,const FKartNarrowphaseSettings& Settings={});
    // Explicit snapshot dispatcher. Never mutates tree/shapes/bodies. Caller owns shape ID map.
    // Missing/invalid/stale bounds produce InvalidInput; removed/disabled/filtered pairs skipped.
    ROADTOF1_API void Dispatch(const FKartPhysicsBroadphase& Tree,const TArray<FKartPhysicsPair>& Candidates,
        const TMap<FKartPhysicsBodyId,FKartConvexShape>& Shapes,TArray<FKartContactGeometry>& Out);
}
