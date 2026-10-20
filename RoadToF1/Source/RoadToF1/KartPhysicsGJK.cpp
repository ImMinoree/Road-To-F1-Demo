#include "KartPhysicsNarrowphase.h"
using namespace KartNarrowphaseNumerics;

namespace KartNarrowphasePrivate
{
FKartSupportPoint Support(const FKartConvexShape& A,const FKartConvexShape& B,const FVector& D)
{
    const FVector X=A.SupportOffset(D),Y=B.SupportOffset(-D);
    // Subtract positions BEFORE adding offsets to reduce large-world cancellation.
    return {(A.Position-B.Position)+(X-Y),A.Position+X,B.Position+Y,D};
}
bool TetraWeights(const FVector& A,const FVector& B,const FVector& C,const FVector& D,double* W)
{
    const FVector X=B-A,Y=C-A,Z=D-A;
    const double Scale=FMath::Max3(X.Size(),Y.Size(),Z.Size());if(Scale<=0)return false;
    const FVector U=X/Scale,V=Y/Scale,T=Z/Scale,P=-A/Scale;
    const double Det=FVector::DotProduct(U,FVector::CrossProduct(V,T));
    if(FMath::Abs(Det)<TetraRelativeMinimum)return false;
    W[1]=FVector::DotProduct(P,FVector::CrossProduct(V,T))/Det;
    W[2]=FVector::DotProduct(U,FVector::CrossProduct(P,T))/Det;
    W[3]=FVector::DotProduct(U,FVector::CrossProduct(V,P))/Det;
    W[0]=1-W[1]-W[2]-W[3];return true;
}
// Enumerate all nonempty simplex subsets (at most 15). Project onto each affine
// feature and retain a convex projection. Degenerate features fall back to edges/points.
FVector Reduce(FKartGJKResult& R)
{
    double Best=TNumericLimits<double>::Max();int32 MaskBest=1;FVector Closest=R.Simplex[0].Minkowski;
    for(int32 Mask=1;Mask<(1<<R.Count);++Mask)
    {
        int32 Id[4],N=0;for(int32 I=0;I<R.Count;++I)if(Mask&(1<<I))Id[N++]=I;
        const FVector A=R.Simplex[Id[0]].Minkowski;double W[4]={1,0,0,0};bool Valid=true;
        if(N==2)
        {
            const FVector E=R.Simplex[Id[1]].Minkowski-A;const double L=E.SizeSquared();
            if(L<DirectionSquaredMinimum)continue;W[1]=-FVector::DotProduct(A,E)/L;W[0]=1-W[1];
        }
        if(N==3)
        {
            const FVector E=R.Simplex[Id[1]].Minkowski-A,F=R.Simplex[Id[2]].Minkowski-A;
            const double EE=E.SizeSquared(),FF=F.SizeSquared(),EF=FVector::DotProduct(E,F),Det=EE*FF-EF*EF;
            if(Det<=GramRelativeMinimum*EE*FF || EE==0 || FF==0)continue;
            const double AE=FVector::DotProduct(A,E),AF=FVector::DotProduct(A,F);
            W[1]=(EF*AF-FF*AE)/Det;W[2]=(EF*AE-EE*AF)/Det;W[0]=1-W[1]-W[2];
        }
        if(N==4)Valid=TetraWeights(A,R.Simplex[Id[1]].Minkowski,R.Simplex[Id[2]].Minkowski,R.Simplex[Id[3]].Minkowski,W);
        FVector P=FVector::ZeroVector;
        for(int32 I=0;I<N;++I){if(W[I]<0)Valid=false;P+=R.Simplex[Id[I]].Minkowski*W[I];}
        if(Valid && P.SizeSquared()<Best){Best=P.SizeSquared();Closest=P;MaskBest=Mask;}
    }
    int32 N=0;for(int32 I=0;I<R.Count;++I)if(MaskBest&(1<<I))R.Simplex[N++]=R.Simplex[I];R.Count=N;return Closest;
}
// Bounded deterministic expansion for origin on line/triangle. A support plane
// at distance <= tolerance certifies boundary proximity; otherwise require a
// strictly enclosing tetrahedron, never invent intersection from failure.
void ExpandSeed(const FKartConvexShape& A,const FKartConvexShape& B,FKartGJKResult& R)
{
    TArray<FKartSupportPoint,TInlineAllocator<64>> P;
    for(int32 I=0;I<R.Count;++I)P.Add(R.Simplex[I]);
    TArray<FVector,TInlineAllocator<64>> Directions;
    const FVector Delta=B.Position-A.Position;
    if(Delta.SizeSquared()>DirectionSquaredMinimum){Directions.Add(Delta);Directions.Add(-Delta);}
    for(const auto* Shape:{&A,&B})for(int32 Axis=0;Axis<3;++Axis)
    {FVector Unit=FVector::ZeroVector;Unit[Axis]=1;const auto N=Shape->Orientation.RotateVector(Unit);Directions.Add(N);Directions.Add(-N);}
    if(R.Count>=3){const auto N=FVector::CrossProduct(P[1].Minkowski-P[0].Minkowski,P[2].Minkowski-P[0].Minkowski);if(N.SizeSquared()>DirectionSquaredMinimum){Directions.Add(N);Directions.Add(-N);}}
    for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y)for(int32 Z=-1;Z<=1;++Z)if(X||Y||Z)Directions.Add(FVector(X,Y,Z));
    for(const auto& D:Directions)
    {
        const FVector N=D/D.Size();const auto S=Support(A,B,N);
        if(FVector::DotProduct(S.Minkowski,N)<=R.Tolerance){R.Status=EKartGJKStatus::Touching;return;}
        bool Duplicate=false;for(const auto& V:P)if((V.Minkowski-S.Minkowski).SizeSquared()<=R.Tolerance*R.Tolerance){Duplicate=true;break;}
        if(!Duplicate)P.Add(S);
    }
    for(int32 I=0;I<P.Num();++I)for(int32 J=I+1;J<P.Num();++J)for(int32 K=J+1;K<P.Num();++K)for(int32 L=K+1;L<P.Num();++L)
    {
        double W[4];if(!TetraWeights(P[I].Minkowski,P[J].Minkowski,P[K].Minkowski,P[L].Minkowski,W))continue;
        if(W[0]>SeedWeightMinimum && W[1]>SeedWeightMinimum && W[2]>SeedWeightMinimum && W[3]>SeedWeightMinimum)
        {R.Simplex[0]=P[I];R.Simplex[1]=P[J];R.Simplex[2]=P[K];R.Simplex[3]=P[L];R.Count=4;R.Status=EKartGJKStatus::Intersecting;return;}
    }
    R.Status=EKartGJKStatus::Degenerate;
}
}
bool FKartNarrowphaseSettings::IsValid() const
{
    return GJKIterations>=0 && GJKIterations<=128 && EPAIterations>=0 && EPAIterations<=512
        && FMath::IsFinite(AbsoluteTolerance) && AbsoluteTolerance>0 && AbsoluteTolerance<=.01
        && FMath::IsFinite(RelativeTolerance) && RelativeTolerance>0 && RelativeTolerance<=1.e-5
        && FMath::IsFinite(EPARelativeTolerance) && EPARelativeTolerance>=1.e-9 && EPARelativeTolerance<=.001;
}
bool KartNarrowphase::SupportMinkowski(const FKartConvexShape& A,const FKartConvexShape& B,const FVector& D,FKartSupportPoint& Out)
{
    if(!A.IsValid() || !B.IsValid() || D.ContainsNaN() || D.GetAbsMax()>1.e100)return false;
    Out=KartNarrowphasePrivate::Support(A,B,D);return !Out.Minkowski.ContainsNaN();
}
FKartGJKResult KartNarrowphase::GJK(const FKartConvexShape& A,const FKartConvexShape& B,const FKartNarrowphaseSettings& S)
{
    using namespace KartNarrowphasePrivate;FKartGJKResult R;
    if(!S.IsValid() || !A.IsValid() || !B.IsValid())return R;
    R.Tolerance=S.AbsoluteTolerance+S.RelativeTolerance*FMath::Max(A.Scale(),B.Scale());
    FVector D=B.Position-A.Position;if(D.SizeSquared()<DirectionSquaredMinimum)D=FVector(1,0,0);
    R.Simplex[0]=Support(A,B,D/D.Size());R.Count=1;
    FVector Closest=R.Simplex[0].Minkowski;
    for(int32 I=0;I<S.GJKIterations;++I)
    {
        R.Iterations=I+1;
        if(Closest.Size()<=R.Tolerance){ExpandSeed(A,B,R);return R;}
        D=-Closest/Closest.Size();const auto P=Support(A,B,D);
        if(FVector::DotProduct(P.Minkowski,D)<-R.Tolerance){R.Status=EKartGJKStatus::Separated;return R;}
        for(int32 J=0;J<R.Count;++J)if((P.Minkowski-R.Simplex[J].Minkowski).Size()<=R.Tolerance)
        {R.Status=EKartGJKStatus::Degenerate;return R;}
        if(R.Count==4){R.Status=EKartGJKStatus::Degenerate;return R;}
        R.Simplex[R.Count++]=P;Closest=Reduce(R);
    }
    R.Status=EKartGJKStatus::IterationLimit;return R;
}
