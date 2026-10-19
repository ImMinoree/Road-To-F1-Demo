#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "KartPhysicsNarrowphase.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
static FAutoConsoleCommandWithWorld NarrowphaseProbe(TEXT("RoadToF1.Physics.NarrowphaseProbe"),
    TEXT("Stage3 temporary box/sphere geometry only. No live response."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    if(!World)return;const auto* Player=UGameplayStatics::GetPlayerPawn(World,0);
    FKartConvexShape A;A.Position=Player?Player->GetActorLocation()+FVector(200,0,150):FVector(0,0,200);A.HalfExtents=FVector(50);
    auto B=A;B.Kind=EKartConvexKind::Sphere;B.Radius=40;B.Position+=FVector(70,0,0);
    const auto G=KartNarrowphase::GJK(A,B);const auto R=KartNarrowphase::Query(A,B);
    DrawDebugBox(World,A.Position,A.HalfExtents,FColor::Cyan,false,8);
    DrawDebugSphere(World,B.Position,B.Radius,24,FColor::Yellow,false,8);
    for(const auto* Shape:{&A,&B}){FKartPhysicsAABB Bounds;Shape->Bounds(Bounds);DrawDebugBox(World,Bounds.Center(),Bounds.Extents(),FColor::Silver,false,8);}
    // Simplex shown in translated Minkowski space, not mistaken for world contact points.
    const FVector Origin=A.Position+FVector(0,0,180);
    for(int32 I=0;I<G.Count;++I)for(int32 J=I+1;J<G.Count;++J)DrawDebugLine(World,Origin+G.Simplex[I].Minkowski,Origin+G.Simplex[J].Minkowski,FColor::Magenta,false,8);
    if(R.bGeometryValid)
    {
        DrawDebugPoint(World,R.PointOnA,12,FColor::Green,false,8);DrawDebugPoint(World,R.PointOnB,12,FColor::Green,false,8);
        DrawDebugDirectionalArrow(World,R.PointOnB,R.PointOnB+R.Normal*FMath::Max(30.,R.Depth),10,FColor::Red,false,8);
    }
    const auto Label=FString::Printf(TEXT("Stage3 TEST A/B | GJK=%d EPA=%d | depth %.5fcm | valid=%d\nUpper magenta: Minkowski simplex"),int(R.GJK),int(R.EPA),R.Depth,R.bGeometryValid);
    DrawDebugString(World,Origin+FVector(0,0,100),Label,nullptr,R.bGeometryValid?FColor::Green:FColor::Orange,8);
    UE_LOG(LogTemp,Display,TEXT("NarrowphaseProbe %s | iterations=%d/%d; Legacy untouched"),*Label,R.GJKIterations,R.EPAIterations);
}));
#endif
