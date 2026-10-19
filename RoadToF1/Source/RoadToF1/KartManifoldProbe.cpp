#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "KartPhysicsManifoldCache.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
static FAutoConsoleCommandWithWorld ManifoldProbe(TEXT("RoadToF1.Physics.ManifoldProbe"),
    TEXT("Three temporary contact refreshes, final patch drawn8seconds. No live response."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    if(!World)return;const auto* Player=UGameplayStatics::GetPlayerPawn(World,0);FKartConvexShape A;
    A.Position=Player?Player->GetActorLocation()+FVector(250,0,160):FVector(0,0,200);A.HalfExtents=FVector(50);
    auto B=A;B.Position+=FVector(80,0,0);FKartPhysicsBroadphase Tree;TMap<FKartPhysicsBodyId,FKartConvexShape> Shapes;
    for(uint64 Id:{1ull,2ull}){const auto& S=Id==1?A:B;FKartPhysicsProxy P;P.Id={Id};S.Bounds(P.Bounds);Tree.RegisterProxy(P);Shapes.Add(P.Id,S);}
    FKartPhysicsManifoldCache Cache;
    for(int32 Step=0;Step<3;++Step){B.Position.Y+=.02;Shapes.Add({2},B);FKartPhysicsAABB Bounds;B.Bounds(Bounds);Tree.UpdateProxy({2},Bounds);
        TArray<FKartPhysicsPair> Pairs;Tree.GeneratePairs(Pairs);Cache.Update(Tree,Pairs,Shapes);}
    DrawDebugBox(World,A.Position,A.HalfExtents,FColor::Cyan,false,8);DrawDebugBox(World,B.Position,B.HalfExtents,FColor::Yellow,false,8);
    const FColor Colors[4]={FColor::Green,FColor::Magenta,FColor::Orange,FColor::White};
    for(const auto& M:Cache.GetResults())
    {
        for(int32 I=0;I<M.Points.Num();++I){const auto& P=M.Points[I];DrawDebugPoint(World,P.WorldA,12,Colors[I],false,8);DrawDebugPoint(World,P.WorldB,8,Colors[I],false,8);
            DrawDebugLine(World,P.WorldA,P.WorldB,Colors[I],false,8);DrawDebugDirectionalArrow(World,P.WorldB,P.WorldB+M.Normal*30,8,FColor::Red,false,8);
            DrawDebugString(World,P.WorldA+FVector(0,0,8),FString::Printf(TEXT("id%llu feature%llu age%u\nlocalA %s localB %s"),P.PersistentId,P.Feature,P.Age,*P.LocalA.ToCompactString(),*P.LocalB.ToCompactString()),nullptr,Colors[I],8);}
        DrawDebugString(World,A.Position+FVector(0,0,100),FString::Printf(TEXT("Stage4 TEST pair1:2 points%d GJK%d EPA%d"),M.Points.Num(),int(M.GJK),int(M.EPA)),nullptr,FColor::White,8);
    }
    const auto D=Cache.GetDiagnostics();UE_LOG(LogTemp,Display,TEXT("ManifoldProbe active=%d reused=%d new=%d invalidated=%d max_points=%d failures=%d Legacy untouched"),D.Active,D.Reused,D.New,D.Invalidated,D.MaxPoints,D.Failures);
}));
#endif
