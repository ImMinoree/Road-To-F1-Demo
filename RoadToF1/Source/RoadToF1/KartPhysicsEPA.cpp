#include "KartPhysicsNarrowphase.h"
using namespace KartNarrowphaseNumerics;

namespace KartNarrowphasePrivate
{
FKartSupportPoint Support(const FKartConvexShape&,const FKartConvexShape&,const FVector&);
bool TetraWeights(const FVector&,const FVector&,const FVector&,const FVector&,double*);
struct FFace {int32 A,B,C;FVector N;double D;};
bool Face(int32 A,int32 B,int32 C,const TArray<FKartSupportPoint,TInlineAllocator<520>>& V,FFace& Out,double Tol)
{
    FVector N=FVector::CrossProduct(V[B].Minkowski-V[A].Minkowski,V[C].Minkowski-V[A].Minkowski);
    const double L=N.Size();if(L<=Tol*Tol || !FMath::IsFinite(L))return false;
    N/=L;double D=FVector::DotProduct(N,V[A].Minkowski);
    if(D<0){Swap(B,C);N=-N;D=-D;}
    Out={A,B,C,N,D};return true;
}
bool Barycentric(const FFace& F,const TArray<FKartSupportPoint,TInlineAllocator<520>>& V,double* W)
{
    const FVector A=V[F.A].Minkowski,E=V[F.B].Minkowski-A,G=V[F.C].Minkowski-A,P=F.N*F.D-A;
    const double EE=E.SizeSquared(),GG=G.SizeSquared(),EG=FVector::DotProduct(E,G),Det=EE*GG-EG*EG;
    if(Det<=GramRelativeMinimum*EE*GG)return false;
    W[1]=(GG*FVector::DotProduct(P,E)-EG*FVector::DotProduct(P,G))/Det;
    W[2]=(EE*FVector::DotProduct(P,G)-EG*FVector::DotProduct(P,E))/Det;W[0]=1-W[1]-W[2];
    return W[0]>=-BarycentricSlack && W[1]>=-BarycentricSlack && W[2]>=-BarycentricSlack;
}
}
FKartContactGeometry KartNarrowphase::EPA(const FKartConvexShape& A,const FKartConvexShape& B,const FKartGJKResult& Seed,const FKartNarrowphaseSettings& S)
{
    using namespace KartNarrowphasePrivate;FKartContactGeometry R;R.GJK=Seed.Status;R.GJKIterations=Seed.Iterations;R.EPA=EKartEPAStatus::InvalidSeed;
    if(!A.IsValid() || !B.IsValid() || !S.IsValid() || Seed.Status!=EKartGJKStatus::Intersecting || Seed.Count!=4)return R;
    const double Tol=S.AbsoluteTolerance+S.RelativeTolerance*FMath::Max(A.Scale(),B.Scale());
    const double Convergence=S.AbsoluteTolerance+S.EPARelativeTolerance*FMath::Max(A.Scale(),B.Scale());
    TArray<FKartSupportPoint,TInlineAllocator<520>> V;
    for(int32 I=0;I<4;++I)
    {
        const auto& P=Seed.Simplex[I];
        if(P.Minkowski.ContainsNaN() || P.PointOnA.ContainsNaN() || P.PointOnB.ContainsNaN()
            || P.Direction.ContainsNaN() || P.Direction.GetAbsMax()>1.e100
            || !(P.PointOnA-P.PointOnB).Equals(P.Minkowski,Tol*4+WorldWitnessRoundoffCm))return R;
        const auto Expected=Support(A,B,P.Direction);
        if(!Expected.Minkowski.Equals(P.Minkowski,Tol) || !Expected.PointOnA.Equals(P.PointOnA,Tol+WorldWitnessRoundoffCm)
            || !Expected.PointOnB.Equals(P.PointOnB,Tol+WorldWitnessRoundoffCm))return R;
        V.Add(P);
    }
    double TW[4];if(!TetraWeights(V[0].Minkowski,V[1].Minkowski,V[2].Minkowski,V[3].Minkowski,TW))return R;
    for(double W:TW)if(W<=0)return R;
    TArray<FFace,TInlineAllocator<1040>> Faces;
    const int32 Indices[4][3]={{0,1,2},{0,3,1},{0,2,3},{1,3,2}};
    for(const auto& I:Indices){FFace F;if(!Face(I[0],I[1],I[2],V,F,Tol))return R;Faces.Add(F);}
    for(int32 Iter=0;Iter<S.EPAIterations;++Iter)
    {
        R.EPAIterations=Iter+1;int32 Closest=INDEX_NONE;double Best=TNumericLimits<double>::Max(),Weights[3]={};
        for(int32 I=0;I<Faces.Num();++I)
        {
            double W[3];if(Faces[I].D<Best && Barycentric(Faces[I],V,W))
            {Best=Faces[I].D;Closest=I;for(int32 J=0;J<3;++J)Weights[J]=W[J];}
        }
        if(Closest==INDEX_NONE){R.EPA=EKartEPAStatus::Degenerate;return R;}
        const FFace F=Faces[Closest];const auto P=Support(A,B,F.N);
        const double Advance=FVector::DotProduct(P.Minkowski,F.N)-F.D;
        if(Advance < -Tol){R.EPA=EKartEPAStatus::Degenerate;return R;}
        if(Advance<=Convergence)
        {
            const int32 Id[3]={F.A,F.B,F.C};
            for(int32 I=0;I<3;++I){R.PointOnA+=V[Id[I]].PointOnA*Weights[I];R.PointOnB+=V[Id[I]].PointOnB*Weights[I];}
            R.Normal=F.N;R.Depth=F.D;R.bGeometryValid=true;R.EPA=EKartEPAStatus::Converged;return R;
        }
        for(const auto& Old:V)if((Old.Minkowski-P.Minkowski).Size()<=Tol){R.EPA=EKartEPAStatus::Degenerate;return R;}
        if(V.Num()>=520){R.EPA=EKartEPAStatus::CapacityLimit;return R;}
        TArray<FIntPoint,TInlineAllocator<1040>> Edges;
        auto Edge=[&](int32 X,int32 Y)
        {
            const int32 Reverse=Edges.IndexOfByKey(FIntPoint(Y,X));
            if(Reverse!=INDEX_NONE)Edges.RemoveAt(Reverse,1,EAllowShrinking::No);else Edges.Add(FIntPoint(X,Y));
        };
        for(int32 I=Faces.Num()-1;I>=0;--I)
        {
            const auto Old=Faces[I];if(FVector::DotProduct(Old.N,P.Minkowski)-Old.D>Tol*VisibleToleranceFraction)
            {Edge(Old.A,Old.B);Edge(Old.B,Old.C);Edge(Old.C,Old.A);Faces.RemoveAt(I,1,EAllowShrinking::No);}
        }
        if(Edges.Num()<3){R.EPA=EKartEPAStatus::Degenerate;return R;}
        // A valid convex horizon is one closed directed loop, not disconnected fans.
        int32 Next=Edges[0].Y;for(int32 I=1;I<Edges.Num();++I)
        {
            int32 Found=INDEX_NONE;for(int32 J=I;J<Edges.Num();++J)if(Edges[J].X==Next){Found=J;break;}
            if(Found==INDEX_NONE){R.EPA=EKartEPAStatus::Degenerate;return R;}
            Edges.Swap(I,Found);Next=Edges[I].Y;
        }
        if(Next!=Edges[0].X){R.EPA=EKartEPAStatus::Degenerate;return R;}
        if(Faces.Num()+Edges.Num()>1040){R.EPA=EKartEPAStatus::CapacityLimit;return R;}
        const int32 New=V.Add(P);
        for(const auto E:Edges){FFace NewFace;if(!Face(E.X,E.Y,New,V,NewFace,Tol)){R.EPA=EKartEPAStatus::Degenerate;return R;}Faces.Add(NewFace);}
    }
    R.EPA=EKartEPAStatus::IterationLimit;return R;
}
