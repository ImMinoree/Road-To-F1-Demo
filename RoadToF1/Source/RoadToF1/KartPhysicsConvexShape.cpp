#include "KartPhysicsConvexShape.h"

double FKartConvexShape::Scale() const
{
    if(Kind==EKartConvexKind::Sphere)return Radius;
    if(Kind==EKartConvexKind::Box)return HalfExtents.Size();
    double S=0;for(const auto& V:Vertices)S=FMath::Max(S,V.Size());return S;
}
bool FKartConvexShape::IsValid() const
{
    if(Position.ContainsNaN() || Position.GetAbsMax()>1.e8 || Orientation.ContainsNaN() || !Orientation.IsNormalized())return false;
    if(Kind==EKartConvexKind::Box)return !HalfExtents.ContainsNaN() && HalfExtents.GetMin()>=1.e-4 && HalfExtents.GetMax()<=1.e5;
    if(Kind==EKartConvexKind::Sphere)return FMath::IsFinite(Radius) && Radius>=1.e-4 && Radius<=1.e5;
    if(Kind!=EKartConvexKind::Hull || Vertices.Num()<4 || Vertices.Num()>MaxVertices)return false;
    for(const auto& V:Vertices)if(V.ContainsNaN() || V.GetAbsMax()>1.e5)return false;
    // Linear-time rank check using longest baseline, greatest area, then plane distance.
    const FVector O=Vertices[0];FVector E=FVector::ZeroVector,N=E;
    for(const auto& V:Vertices)if((V-O).SizeSquared()>E.SizeSquared())E=V-O;
    const double S=Scale(),Tol=FMath::Max(1.e-7,S*1.e-9);
    if(E.Size()<=Tol)return false;
    for(const auto& V:Vertices){const auto C=FVector::CrossProduct(E,V-O);if(C.SizeSquared()>N.SizeSquared())N=C;}
    if(N.Size()<=Tol*E.Size())return false;
    N.Normalize();for(const auto& V:Vertices)if(FMath::Abs(FVector::DotProduct(V-O,N))>Tol)return true;
    return false;
}
FVector FKartConvexShape::SupportOffset(const FVector& Direction) const
{
    FVector D=Direction;
    if(D.SizeSquared()<1.e-28)D=FVector(1,0,0);
    if(Kind==EKartConvexKind::Sphere)return D/D.Size()*Radius;
    const FVector L=Orientation.UnrotateVector(D);FVector P;
    if(Kind==EKartConvexKind::Box)P=FVector(L.X>=0?HalfExtents.X:-HalfExtents.X,L.Y>=0?HalfExtents.Y:-HalfExtents.Y,L.Z>=0?HalfExtents.Z:-HalfExtents.Z);
    else
    {
        P=Vertices[0];double Best=FVector::DotProduct(P,L);
        for(int32 I=1;I<Vertices.Num();++I){const double V=FVector::DotProduct(Vertices[I],L);if(V>Best){Best=V;P=Vertices[I];}}
    }
    return Orientation.RotateVector(P);
}
bool FKartConvexShape::Support(const FVector& D,FVector& Out) const
{
    if(!IsValid() || D.ContainsNaN() || D.GetAbsMax()>1.e100)return false;
    Out=Position+SupportOffset(D);return !Out.ContainsNaN();
}
bool FKartConvexShape::Bounds(FKartPhysicsAABB& Out) const
{
    if(!IsValid())return false;
    for(int32 I=0;I<3;++I){FVector D=FVector::ZeroVector;D[I]=1;Out.Max[I]=(Position+SupportOffset(D))[I];Out.Min[I]=(Position+SupportOffset(-D))[I];}
    return Out.IsValid();
}
