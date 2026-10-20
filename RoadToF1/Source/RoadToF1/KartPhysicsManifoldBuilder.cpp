#include "KartPhysicsManifoldBuilder.h"

namespace KartManifoldPrivate
{
constexpr double GeometryCm=1.e-7,FaceAlignment=1.-1.e-6;
struct FVertex {FVector P;uint16 Source;uint8 Planes;};
using FPolygon=TArray<FVertex,TInlineAllocator<16>>;
using FPoints=TArray<FKartContactPoint,TInlineAllocator<16>>;
FVector Axis(const FKartConvexShape& S,int32 I){FVector U=FVector::ZeroVector;U[I]=1;return S.Orientation.RotateVector(U);}
// Exact OBB separating-axis fallback for a numerically degenerate GJK simplex.
// No hull/mesh approximation, no fabricated EPA convergence. Edge axes still
// require an independently valid witness if face clipping cannot construct a patch.
bool BoxSAT(const FKartConvexShape& A,const FKartConvexShape& B,FVector& Normal,double& Depth)
{
    const FVector Delta=B.Position-A.Position;Depth=TNumericLimits<double>::Max();
    auto TestAxis=[&](FVector N)
    {
        const double Length=N.Size();if(Length<1.e-12)return true;N/=Length;
        double Radius=0;for(int I=0;I<3;++I)Radius+=A.HalfExtents[I]*FMath::Abs(FVector::DotProduct(Axis(A,I),N))
            +B.HalfExtents[I]*FMath::Abs(FVector::DotProduct(Axis(B,I),N));
        const double D=Radius-FMath::Abs(FVector::DotProduct(Delta,N));
        if(D<0)return false;
        if(D<Depth){Depth=D;Normal=FVector::DotProduct(Delta,N)>=0?N:-N;}return true;
    };
    for(int I=0;I<3;++I)if(!TestAxis(Axis(A,I)) || !TestAxis(Axis(B,I)))return false;
    for(int I=0;I<3;++I)for(int J=0;J<3;++J)if(!TestAxis(FVector::CrossProduct(Axis(A,I),Axis(B,J))))return false;
    return true;
}
int32 BestAxis(const FKartConvexShape& S,const FVector& N,double& Dot)
{
    int32 Best=0;Dot=-1;for(int32 I=0;I<3;++I){const double D=FMath::Abs(FVector::DotProduct(Axis(S,I),N));if(D>Dot){Dot=D;Best=I;}}return Best;
}
bool Less(const FKartContactPoint& A,const FKartContactPoint& B)
{
    if(A.Feature!=B.Feature)return A.Feature<B.Feature;
    for(int32 I=0;I<3;++I)if(A.LocalA[I]!=B.LocalA[I])return A.LocalA[I]<B.LocalA[I];
    for(int32 I=0;I<3;++I)if(A.LocalB[I]!=B.LocalB[I])return A.LocalB[I]<B.LocalB[I];return false;
}
void Add(FPoints& Out,const FVector& WA,const FVector& WB,const FVector& N,uint64 Feature,
    const FKartConvexShape& A,const FKartConvexShape& B)
{
    const double Depth=FVector::DotProduct(WA-WB,N);
    if(Depth < -GeometryCm || WA.ContainsNaN() || WB.ContainsNaN())return;
    FKartContactPoint P;P.WorldA=WA;P.WorldB=WB;P.LocalA=KartManifolds::ToLocal(A,WA);P.LocalB=KartManifolds::ToLocal(B,WB);
    P.Depth=FMath::Max(0.,Depth);P.Feature=Feature;Out.Add(P);
}
void Clip(FPolygon& Poly,const FVector& Origin,const FVector& N,double Extent,uint8 Plane)
{
    FPolygon Next;if(Poly.IsEmpty())return;
    FVertex Last=Poly.Last();double LD=FVector::DotProduct(Last.P-Origin,N)-Extent;
    for(const auto& Current:Poly)
    {
        const double CD=FVector::DotProduct(Current.P-Origin,N)-Extent;
        if((LD<=0)!=(CD<=0))
        {
            const double T=LD/(LD-CD);
            Next.Add({Last.P+(Current.P-Last.P)*T,uint16(Last.Source|Current.Source),uint8(Last.Planes|Current.Planes|Plane)});
        }
        if(CD<=0)Next.Add(Current);Last=Current;LD=CD;
    }
    Poly=MoveTemp(Next);
}
bool BoxClip(FPoints& Out,FKartContactManifold& M,const FKartConvexShape& A,const FKartConvexShape& B)
{
    double DA,DB;const int32 IA=BestAxis(A,M.Normal,DA),IB=BestAxis(B,M.Normal,DB);
    if(FMath::Max(DA,DB)<FaceAlignment)return false; // Edge region: valid EPA witness only.
    const bool RefA=DA>=DB;const auto& Ref=RefA?A:B;const auto& Inc=RefA?B:A;const int32 RI=RefA?IA:IB;
    const FVector Target=RefA?M.Normal:-M.Normal;const double Sign=FVector::DotProduct(Axis(Ref,RI),Target)>=0?1.:-1.;
    const FVector N=Axis(Ref,RI)*Sign;M.Normal=RefA?N:-N;
    double Ignore;const int32 II=BestAxis(Inc,N,Ignore);const double ISign=FVector::DotProduct(Axis(Inc,II),N)>0?-1.:1.;
    const int32 U=(II+1)%3,V=(II+2)%3;const int32 Signs[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};FPolygon Poly;
    for(const auto& S:Signs)
    {
        FVector L=FVector::ZeroVector;L[II]=Inc.HalfExtents[II]*ISign;L[U]=Inc.HalfExtents[U]*S[0];L[V]=Inc.HalfExtents[V]*S[1];
        const int32 Code=(L.X>0?1:0)|(L.Y>0?2:0)|(L.Z>0?4:0);Poly.Add({KartManifolds::ToWorld(Inc,L),uint16(1<<Code),0});
    }
    int32 Side=0;for(int32 I=0;I<3;++I)if(I!=RI)for(double S:{-1.,1.})Clip(Poly,Ref.Position,Axis(Ref,I)*S,Ref.HalfExtents[I],uint8(1<<Side++));
    const FVector FaceCenter=Ref.Position+N*Ref.HalfExtents[RI];
    const uint64 RefFace=RI*2+(Sign>0?1:0),IncFace=II*2+(ISign>0?1:0);
    for(const auto& P:Poly)
    {
        const double D=FVector::DotProduct(FaceCenter-P.P,N);if(D<0)continue;
        const FVector OnRef=P.P+N*D;
        const uint64 Feature=0x100000000ull|(uint64(RefA)<<28)|(RefFace<<24)|(IncFace<<20)|(uint64(P.Planes)<<16)|P.Source;
        Add(Out,RefA?OnRef:P.P,RefA?P.P:OnRef,M.Normal,Feature,A,B);
    }
    return true;
}
bool BoxEdge(FPoints& Out,FKartContactManifold& M,const FKartConvexShape& A,const FKartConvexShape& B)
{
    // The minimum SAT cross axis identifies two finite supporting edges.
    // Solve their closest line parameters, then require both parameters to
    // lie ON the edges and the witness difference to match the SAT normal.
    // This is exact OBB geometry, never a hull approximation or EPA success.
    for(int I=0;I<3;++I)for(int J=0;J<3;++J)
    {
        const FVector U=Axis(A,I),V=Axis(B,J),Cross=FVector::CrossProduct(U,V);
        if(Cross.SizeSquared()<1.e-12 || FMath::Abs(FVector::DotProduct(Cross.GetSafeNormal(),M.Normal))<1.-1.e-8)continue;
        FVector CA=A.Position,CB=B.Position;
        for(int K=0;K<3;++K)
        {
            if(K!=I)CA+=Axis(A,K)*(FVector::DotProduct(Axis(A,K),M.Normal)>=0?A.HalfExtents[K]:-A.HalfExtents[K]);
            if(K!=J)CB+=Axis(B,K)*(FVector::DotProduct(Axis(B,K),M.Normal)>=0?-B.HalfExtents[K]:B.HalfExtents[K]);
        }
        const FVector R=CA-CB;const double UV=FVector::DotProduct(U,V),Den=1-UV*UV;
        if(Den<1.e-12)continue;
        const double S=(UV*FVector::DotProduct(V,R)-FVector::DotProduct(U,R))/Den;
        const double T=UV*S+FVector::DotProduct(V,R);
        if(FMath::Abs(S)>A.HalfExtents[I]+GeometryCm || FMath::Abs(T)>B.HalfExtents[J]+GeometryCm)continue;
        const FVector WA=CA+U*S,WB=CB+V*T,D=WA-WB;const double Depth=FVector::DotProduct(D,M.Normal);
        if(Depth<0 || (D-M.Normal*Depth).Size()>1.e-5)continue;
        Add(Out,WA,WB,M.Normal,0x300000000ull+I*3+J,A,B);return !Out.IsEmpty();
    }
    return false;
}
void Reduce(FPoints& Candidates,FKartContactManifold& M,const FKartManifoldSettings& S,const FKartContactManifold* Old)
{
    Candidates.Sort(Less);FPoints Unique;
    for(const auto& P:Candidates)
    {
        bool Duplicate=false;for(const auto& Q:Unique)if((P.WorldA-Q.WorldA).Size()<=S.DedupCm && (P.WorldB-Q.WorldB).Size()<=S.DedupCm){Duplicate=true;break;}
        if(!Duplicate)Unique.Add(P);
    }
    M.CandidateCount=Unique.Num();if(Unique.IsEmpty())return;
    // Preserve deepest, then greedily cover the patch. Previous contacts break coverage ties.
    auto Persistent=[&](const FKartContactPoint& P){if(Old)for(const auto& Q:Old->Points)if((P.LocalA-Q.LocalA).Size()<=S.AnchorDriftCm && (P.LocalB-Q.LocalB).Size()<=S.AnchorDriftCm)return true;return false;};
    int32 First=0;for(int32 I=1;I<Unique.Num();++I)if(Unique[I].Depth>Unique[First].Depth+GeometryCm)First=I;
    M.Points.Add(Unique[First]);Unique.RemoveAt(First);
    while(!Unique.IsEmpty() && M.Points.Num()<S.MaxContacts)
    {
        int32 Best=0;double Coverage=-1;
        for(int32 I=0;I<Unique.Num();++I)
        {
            double Distance=TNumericLimits<double>::Max();for(const auto& Q:M.Points)Distance=FMath::Min(Distance,(Unique[I].WorldA-Q.WorldA).SizeSquared());
            if(Distance>Coverage+GeometryCm || (FMath::Abs(Distance-Coverage)<=GeometryCm && Persistent(Unique[I]) && !Persistent(Unique[Best])))
            {Coverage=Distance;Best=I;}
        }
        M.Points.Add(Unique[Best]);Unique.RemoveAt(Best);
    }
    M.Points.Sort(Less);
}
}
bool FKartManifoldSettings::IsValid() const
{
    return MaxContacts>=1 && MaxContacts<=4 && Narrowphase.IsValid() && FMath::IsFinite(DedupCm) && DedupCm>0 && DedupCm<=1
        && FMath::IsFinite(AnchorDriftCm) && AnchorDriftCm>0 && AnchorDriftCm<=10
        && FMath::IsFinite(FeatureMatchCm) && FeatureMatchCm>0 && FeatureMatchCm<=10
        && FMath::IsFinite(SeparationCm) && SeparationCm>=0 && SeparationCm<=1
        && FMath::IsFinite(NormalMatchCos) && NormalMatchCos>=0 && NormalMatchCos<=1;
}
FVector KartManifolds::ToWorld(const FKartConvexShape& S,const FVector& P){return S.Position+S.Orientation.RotateVector(P);}
FVector KartManifolds::ToLocal(const FKartConvexShape& S,const FVector& P){return S.Orientation.UnrotateVector(P-S.Position);}
FKartContactManifold KartManifolds::Build(FKartPhysicsPair Pair,const FKartConvexShape& A,const FKartConvexShape& B,
    const FKartManifoldSettings& Settings,const FKartContactManifold* Previous)
{
    using namespace KartManifoldPrivate;FKartContactManifold M;M.Pair=Pair;FPoints Candidates;
    if(!Pair.IsValid() || !Settings.IsValid() || !A.IsValid() || !B.IsValid()){M.Status=EKartManifoldStatus::InvalidInput;return M;}
    if(A.Kind==EKartConvexKind::Sphere && B.Kind==EKartConvexKind::Sphere)
    {
        M.Method=EKartManifoldMethod::SphereAnalytic;const FVector Delta=B.Position-A.Position;const double Distance=Delta.Size();
        if(Distance>A.Radius+B.Radius){M.GJK=EKartGJKStatus::Separated;return M;}
        M.bAmbiguousNormal=Distance==0;M.Normal=M.bAmbiguousNormal?FVector(1,0,0):Delta/Distance;
        Add(Candidates,A.Position+M.Normal*A.Radius,B.Position-M.Normal*B.Radius,M.Normal,1,A,B);
    }
    else if((A.Kind==EKartConvexKind::Sphere && B.Kind==EKartConvexKind::Box) || (B.Kind==EKartConvexKind::Sphere && A.Kind==EKartConvexKind::Box))
    {
        M.Method=EKartManifoldMethod::SphereBoxAnalytic;const bool BoxA=A.Kind==EKartConvexKind::Box;const auto& Box=BoxA?A:B;const auto& Sphere=BoxA?B:A;
        const FVector L=ToLocal(Box,Sphere.Position);FVector Closest=L;for(int32 I=0;I<3;++I)Closest[I]=FMath::Clamp(L[I],-Box.HalfExtents[I],Box.HalfExtents[I]);
        FVector N=L-Closest;const double Distance=N.Size();int32 Feature=0;
        if(Distance>0){if(Distance>Sphere.Radius){M.GJK=EKartGJKStatus::Separated;return M;}N/=Distance;}
        else
        {
            int32 I=0;for(int32 J=1;J<3;++J)if(Box.HalfExtents[J]-FMath::Abs(L[J])<Box.HalfExtents[I]-FMath::Abs(L[I]))I=J;
            const double Sign=L[I]>=0?1.:-1.;N=FVector::ZeroVector;N[I]=Sign;Closest[I]=Sign*Box.HalfExtents[I];Feature=1+I*2+(Sign>0?1:0);
            M.bAmbiguousNormal=L.IsNearlyZero(GeometryCm);
        }
        N=Box.Orientation.RotateVector(N);const FVector WB=ToWorld(Box,Closest),WS=Sphere.Position-N*Sphere.Radius;
        M.Normal=BoxA?N:-N;Add(Candidates,BoxA?WB:WS,BoxA?WS:WB,M.Normal,0x200000000ull+Feature,A,B);
    }
    else
    {
        M.bNarrowphaseRun=true;const auto G=KartNarrowphase::Query(A,B,Settings.Narrowphase);M.GJK=G.GJK;M.EPA=G.EPA;
        if(G.GJK==EKartGJKStatus::Separated || G.GJK==EKartGJKStatus::Touching)return M;
        if(!G.bGeometryValid)
        {
            if(A.Kind==EKartConvexKind::Box && B.Kind==EKartConvexKind::Box
                && (G.GJK==EKartGJKStatus::Degenerate || G.GJK==EKartGJKStatus::IterationLimit))
            {
                double Depth=0;M.Method=EKartManifoldMethod::BoxSATClip;
                if(!BoxSAT(A,B,M.Normal,Depth) || Depth==0)return M;
                if(!BoxClip(Candidates,M,A,B) && !BoxEdge(Candidates,M,A,B)){M.Status=EKartManifoldStatus::GeometryFailure;return M;}
            }
            else {M.Status=G.GJK==EKartGJKStatus::InvalidInput?EKartManifoldStatus::InvalidInput:EKartManifoldStatus::GeometryFailure;return M;}
        }
        else
        {
            M.Normal=G.Normal;
            if(A.Kind==EKartConvexKind::Box && B.Kind==EKartConvexKind::Box && BoxClip(Candidates,M,A,B))M.Method=EKartManifoldMethod::BoxClip;
            else {M.Method=EKartManifoldMethod::EPAWitness;Add(Candidates,G.PointOnA,G.PointOnB,M.Normal,0,A,B);}
        }
    }
    Reduce(Candidates,M,Settings,Previous);M.Status=M.Points.IsEmpty()?EKartManifoldStatus::GeometryFailure:EKartManifoldStatus::Valid;
    if(M.Method==EKartManifoldMethod::SphereAnalytic || M.Method==EKartManifoldMethod::SphereBoxAnalytic)M.GJK=EKartGJKStatus::InvalidInput; // Not run; Method identifies analytical geometry, never claimed GJK convergence.
    if(M.Status==EKartManifoldStatus::Valid && !ValidContact(M,A,B)){M.Points.Reset();M.Status=EKartManifoldStatus::GeometryFailure;}
    return M;
}
bool KartManifolds::ValidContact(const FKartContactManifold& M,const FKartConvexShape& A,const FKartConvexShape& B)
{
    if(!M.Pair.IsValid() || M.Points.IsEmpty() || M.Points.Num()>4 || M.Normal.ContainsNaN() || FMath::Abs(M.Normal.Size()-1)>1.e-6)return false;
    for(const auto& P:M.Points)if(P.WorldA.ContainsNaN() || P.WorldB.ContainsNaN() || P.LocalA.ContainsNaN() || P.LocalB.ContainsNaN()
        || !FMath::IsFinite(P.Depth) || P.Depth<0 || !ToWorld(A,P.LocalA).Equals(P.WorldA,1.e-5) || !ToWorld(B,P.LocalB).Equals(P.WorldB,1.e-5)
        || !(P.WorldA-P.WorldB).Equals(M.Normal*P.Depth,1.e-5))return false;
    return true;
}
