#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "KartPhysicsBroadphase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

static FAutoConsoleCommandWithWorld BroadphaseProbeCommand(
    TEXT("RoadToF1.Physics.BroadphaseProbe"), TEXT("Draw a temporary three-proxy broadphase fixture; no live bodies are registered or moved."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if(!World)return;
        // Location is read solely to place the overlay; local tree never holds actor references.
        const APawn* Player=UGameplayStatics::GetPlayerPawn(World,0);
        const FVector Origin=Player?Player->GetActorLocation()+Player->GetActorForwardVector()*400+FVector(0,0,100):FVector(0,0,200);
        FKartPhysicsBroadphase Tree; FKartPhysicsBodyIdSource Ids;
        for(int32 I=0;I<3;++I)
        {
            FKartPhysicsProxy P; P.Id=Ids.Allocate();
            const FVector Center=Origin+FVector(I*70,0,0);
            P.Bounds={Center-FVector(50),Center+FVector(50)};
            if(I==2)P.Mobility=EKartProxyMobility::Static;
            if(!Tree.RegisterProxy(P))return;
            DrawDebugBox(World,Center,FVector(50),I==2?FColor::Blue:FColor::Cyan,false,8);
            DrawDebugString(World,Center+FVector(0,0,70),FString::Printf(TEXT("TEST proxy %llu"),P.Id.Value),nullptr,FColor::White,8);
        }
        TArray<FKartBroadphaseDebugNode> Nodes; Tree.GetDebugNodes(Nodes);
        for(const auto& N:Nodes)DrawDebugBox(World,N.Bounds.Center(),N.Bounds.Extents(),N.Id.IsValid()?FColor::Yellow:FColor::Silver,false,8);
        TArray<FKartPhysicsPair> Pairs;Tree.GeneratePairs(Pairs);
        for(const auto& Pair:Pairs)
        {
            FKartPhysicsProxy A,B;Tree.GetProxy(Pair.First(),A);Tree.GetProxy(Pair.Second(),B);
            DrawDebugLine(World,A.Bounds.Center(),B.Bounds.Center(),FColor::Green,false,8,0,3);
        }
        const auto D=Tree.GetDiagnostics();
        UE_LOG(LogTemp,Display,TEXT("Stage2 BroadphaseProbe: proxies=%d nodes=%d height=%d pairs=%d visits=%llu valid=%d. Temporary AABB candidates only; Legacy untouched."),
            D.ProxyCount,D.NodeCount,D.Height,D.PairCount,D.PairNodeVisits,Tree.Validate());
    }));
#endif
