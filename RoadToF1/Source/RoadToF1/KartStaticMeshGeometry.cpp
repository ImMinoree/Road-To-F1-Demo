#include "KartStaticMeshGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Brush.h"
#include "Debug/DebugDrawComponent.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Algo/Sort.h"

bool FKartStaticMeshGeometry::Build(const UWorld& World,FString& Failure)
{
    Failure.Empty();Triangles.Reset();Sources.Reset();Order.Reset();Nodes.Reset();Height=0;
    TArray<UPrimitiveComponent*> Components;
    for(TActorIterator<AActor> It(&World);It;++It)
    {
        if(*It==World.GetDefaultBrush())continue; // Editor construction brush, no scene collision body.
        TInlineComponentArray<UPrimitiveComponent*> Found;It->GetComponents(Found);
        // Debug-draw primitives have no geometric physics body even though their
        // inherited default collision flags can say WorldStatic/query-enabled.
        for(auto* C:Found)if(!Cast<UDebugDrawComponent>(C) && C->IsQueryCollisionEnabled() && C->GetCollisionObjectType()==ECC_WorldStatic)Components.Add(C);
    }
    Components.Sort([](const UPrimitiveComponent& A,const UPrimitiveComponent& B){return A.GetPathName()<B.GetPathName();});
    for(auto* C:Components)
    {
        if(const auto* Box=Cast<UBoxComponent>(C))
        {
            FKartStaticGeometrySource Source;Source.Component=C;Source.bBox=true;Source.BoxExtent=Box->GetUnscaledBoxExtent();Source.Transform=C->GetComponentTransform();Source.Path=C->GetPathName();
            FVector V[8];for(int I=0;I<8;++I)V[I]=Source.Transform.TransformPosition(Source.BoxExtent*FVector(I&1?1:-1,I&2?1:-1,I&4?1:-1));
            const int Faces[12][3]={{0,2,6},{0,6,4},{1,5,7},{1,7,3},{0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,1,3},{0,3,2},{4,6,7},{4,7,5}};
            for(const auto& F:Faces){FKartStaticTriangle T;T.Source=Sources.Num();for(int I=0;I<3;++I)T.V[I]=V[F[I]];
                T.Bounds.Min=T.V[0].ComponentMin(T.V[1]).ComponentMin(T.V[2]);T.Bounds.Max=T.V[0].ComponentMax(T.V[1]).ComponentMax(T.V[2]);Triangles.Add(T);++Source.Triangles;}
            Sources.Add(Source);continue;
        }
        auto* SM=Cast<UStaticMeshComponent>(C);auto* Body=SM && SM->GetStaticMesh()?SM->GetStaticMesh()->GetBodySetup():nullptr;
        if(!Body){Failure=TEXT("Missing static mesh collision: ")+C->GetPathName();return false;}
        if(Body->GetCollisionTraceFlag()!=CTF_UseComplexAsSimple && Body->AggGeom.GetElementCount()>0)
        {Failure=TEXT("Simple collision requires an explicit shape adapter: ")+C->GetPathName();return false;}
        Body->CreatePhysicsMeshes();
        FKartStaticGeometrySource Source;Source.Component=C;Source.Geometry=Body;Source.GeometryRevision=Body->BodySetupGuid;Source.Transform=C->GetComponentTransform();Source.Path=C->GetPathName();
        const int32 SourceIndex=Sources.Num();
        for(const auto& Mesh:Body->TriMeshGeometries)if(Mesh)
        {
            const auto& P=Mesh->Particles();const auto& E=Mesh->Elements();
            for(int32 I=0;I<E.GetNumTriangles();++I)
            {
                FKartStaticTriangle T;T.Source=SourceIndex;
                for(int J=0;J<3;++J)
                {
                    const int32 Index=E.RequiresLargeIndices()?int32(E.GetLargeIndexBuffer()[I][J]):int32(E.GetSmallIndexBuffer()[I][J]);
                    if(Index<0 || Index>=int32(P.Size())){Failure=TEXT("Invalid cooked collision index");return false;}
                    T.V[J]=Source.Transform.TransformPosition(FVector(P.GetX(Index)));
                    if(T.V[J].ContainsNaN()){Failure=TEXT("Nonfinite collision vertex");return false;}
                }
                if(FVector::CrossProduct(T.V[1]-T.V[0],T.V[2]-T.V[0]).SizeSquared()<1.e-16){++Source.SkippedDegenerate;continue;}
                T.Bounds.Min=T.V[0].ComponentMin(T.V[1]).ComponentMin(T.V[2]);T.Bounds.Max=T.V[0].ComponentMax(T.V[1]).ComponentMax(T.V[2]);
                if(!T.Bounds.IsValid()){Failure=TEXT("Invalid collision bounds");return false;}
                Triangles.Add(T);++Source.Triangles;
            }
        }
        if(Source.Triangles==0){Failure=TEXT("No cooked triangles: ")+C->GetPathName();return false;}
        Sources.Add(Source);
    }
    for(int I=0;I<Triangles.Num();++I)Order.Add(I);
    if(Order.Num())BuildNode(0,Order.Num(),1);
    return true;
}
bool FKartStaticMeshGeometry::IsCurrent() const
{
    for(const auto& S:Sources)
    {
        if(S.bBox)
        {
            const auto* Box=Cast<UBoxComponent>(S.Component.Get());
            if(!Box || !Box->IsQueryCollisionEnabled() || !Box->GetComponentTransform().Equals(S.Transform,1.e-9) || !Box->GetUnscaledBoxExtent().Equals(S.BoxExtent,1.e-9))return false;
            continue;
        }
        const auto* C=Cast<UStaticMeshComponent>(S.Component.Get());
        if(!C || !C->IsQueryCollisionEnabled() || !C->GetStaticMesh() || C->GetStaticMesh()->GetBodySetup()!=S.Geometry.Get()
            || !S.Geometry.IsValid() || S.Geometry->BodySetupGuid!=S.GeometryRevision || !C->GetComponentTransform().Equals(S.Transform,1.e-9))return false;
    }
    return true;
}
int32 FKartStaticMeshGeometry::BuildNode(int32 Start,int32 Count,int32 Depth)
{
    const int32 Id=Nodes.AddDefaulted();auto Bounds=Triangles[Order[Start]].Bounds;
    for(int I=1;I<Count;++I)Bounds=FKartPhysicsAABB::Combine(Bounds,Triangles[Order[Start+I]].Bounds);
    Nodes[Id].Bounds=Bounds;Height=FMath::Max(Height,Depth);
    if(Count<=8){Nodes[Id].Start=Start;Nodes[Id].Count=Count;return Id;}
    const auto Extent=Bounds.Max-Bounds.Min;const int Axis=Extent.X>=Extent.Y && Extent.X>=Extent.Z?0:Extent.Y>=Extent.Z?1:2;
    Algo::Sort(MakeArrayView(Order.GetData()+Start,Count),[&](int A,int B){const double X=Triangles[A].Bounds.Center()[Axis],Y=Triangles[B].Bounds.Center()[Axis];return X==Y?A<B:X<Y;});
    const int L=BuildNode(Start,Count/2,Depth+1),R=BuildNode(Start+Count/2,Count-Count/2,Depth+1);Nodes[Id].Left=L;Nodes[Id].Right=R;return Id;
}
void FKartStaticMeshGeometry::Query(const FKartPhysicsAABB& Bounds,TArray<int32>& Out) const
{
    Out.Reset();if(Nodes.IsEmpty() || !Bounds.IsValid())return;
    TArray<int32,TInlineAllocator<64>> Stack;Stack.Add(0);
    while(!Stack.IsEmpty())
    {
        const auto& N=Nodes[Stack.Pop(EAllowShrinking::No)];if(!N.Bounds.Overlaps(Bounds))continue;
        if(N.Count){for(int I=0;I<N.Count;++I){const int T=Order[N.Start+I];if(Triangles[T].Bounds.Overlaps(Bounds))Out.Add(T);}}
        else {Stack.Add(N.Right);Stack.Add(N.Left);}
    }
    Out.Sort();
}
double FKartStaticMeshGeometry::Separation(const FKartConvexShape& Box,const FKartStaticTriangle& T,FVector& Normal)
{
    FVector P[3],Edge[3];for(int I=0;I<3;++I)P[I]=Box.Orientation.UnrotateVector(T.V[I]-Box.Position);
    for(int I=0;I<3;++I)Edge[I]=P[(I+1)%3]-P[I];
    double Gap=-TNumericLimits<double>::Max();FVector Best;
    auto Test=[&](FVector N)
    {
        const double L=N.Size();if(L<1.e-12)return;N/=L;
        double Low=FVector::DotProduct(P[0],N),High=Low;
        for(int I=1;I<3;++I){const double D=FVector::DotProduct(P[I],N);Low=FMath::Min(Low,D);High=FMath::Max(High,D);}
        const double R=FVector::DotProduct(Box.HalfExtents,N.GetAbs());const double G=FMath::Max(Low-R,-High-R);
        if(G>Gap){Gap=G;Best=Low>=-High?N:-N;}
    };
    Test(FVector::CrossProduct(Edge[0],Edge[1]));
    for(int I=0;I<3;++I){FVector A=FVector::ZeroVector;A[I]=1;Test(A);for(int J=0;J<3;++J)Test(FVector::CrossProduct(A,Edge[J]));}
    Normal=Box.Orientation.RotateVector(Best);return Gap;
}
FKartContactManifold FKartStaticMeshGeometry::Contact(FKartPhysicsPair Pair,const FKartConvexShape& Box,const FKartStaticTriangle& T)
{
    FKartContactManifold M;M.Pair=Pair;M.Method=EKartManifoldMethod::TriangleClip;FVector N;
    if(!Box.IsValid() || !Pair.IsValid()){M.Status=EKartManifoldStatus::InvalidInput;return M;}
    if(Separation(Box,T,N)>1.e-7)return M;
    TArray<FVector,TInlineAllocator<16>> Poly;for(auto P:T.V)Poly.Add(Box.Orientation.UnrotateVector(P-Box.Position));
    for(int I=0;I<3;++I)for(double Sign:{-1.,1.})
    {
        TArray<FVector,TInlineAllocator<16>> Next;if(Poly.IsEmpty())return M;
        auto Last=Poly.Last();double LD=Last[I]*Sign-Box.HalfExtents[I];
        for(auto P:Poly){const double D=P[I]*Sign-Box.HalfExtents[I];if((D<=0)!=(LD<=0))Next.Add(Last+(P-Last)*(LD/(LD-D)));if(D<=0)Next.Add(P);Last=P;LD=D;}
        Poly=MoveTemp(Next);
    }
    const FVector LN=Box.Orientation.UnrotateVector(N);M.Normal=N;
    TArray<FKartContactPoint> Points;
    for(auto P:Poly)
    {
        double D=TNumericLimits<double>::Max();
        for(int I=0;I<3;++I)
        {
            // Clipping can leave roundoff just outside a face. Dividing that tiny
            // negative gap by a nearly parallel normal magnifies it into a bogus
            // negative ray exit. Validate the coordinate before the division.
            if(FMath::Abs(P[I])>Box.HalfExtents[I]+1.e-7){M.Status=EKartManifoldStatus::GeometryFailure;return M;}
            if(FMath::Abs(LN[I])>1.e-12)
            {
                const double Gap=Box.HalfExtents[I]-(LN[I]>0?P[I]:-P[I]);
                D=FMath::Min(D,FMath::Max(0.,Gap)/FMath::Abs(LN[I]));
            }
        }
        if(!FMath::IsFinite(D) || D< -1.e-7){M.Status=EKartManifoldStatus::GeometryFailure;return M;}
        FKartContactPoint C;C.Depth=FMath::Max(0.,D);C.WorldB=Box.Position+Box.Orientation.RotateVector(P);C.WorldA=C.WorldB+N*C.Depth;
        C.LocalA=Box.Orientation.UnrotateVector(C.WorldA-Box.Position);C.LocalB=C.WorldB;C.Feature=Points.Num()+1;
        bool Duplicate=false;for(const auto& Old:Points)Duplicate|=Old.WorldB.Equals(C.WorldB,1.e-5);if(!Duplicate)Points.Add(C);
    }
    // Deepest first, then spatial coverage of the finite clipped polygon.
    while(!Points.IsEmpty() && M.Points.Num()<4)
    {
        int Best=0;double Score=-1;
        for(int I=0;I<Points.Num();++I){double S=Points[I].Depth;if(!M.Points.IsEmpty()){S=TNumericLimits<double>::Max();for(const auto& P:M.Points)S=FMath::Min(S,(P.WorldB-Points[I].WorldB).SizeSquared());}if(S>Score){Score=S;Best=I;}}
        M.Points.Add(Points[Best]);Points.RemoveAt(Best);
    }
    M.Status=M.Points.IsEmpty()?EKartManifoldStatus::NoContact:EKartManifoldStatus::Valid;return M;
}
