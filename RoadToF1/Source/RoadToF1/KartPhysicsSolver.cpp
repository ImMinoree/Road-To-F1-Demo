#include "KartPhysicsSolver.h"

bool FKartSolverSettings::IsValid() const
{
    return Iterations>=1 && Iterations<=128 && FMath::IsFinite(SlopCm) && SlopCm>=0 && SlopCm<=1
        && FMath::IsFinite(BiasFraction) && BiasFraction>=0 && BiasFraction<=.3
        && FMath::IsFinite(MaxBiasCmPerSecond) && MaxBiasCmPerSecond>=0 && MaxBiasCmPerSecond<=100
        && FMath::IsFinite(MaxImpulse) && MaxImpulse>0 && MaxImpulse<=1.e12
        && FMath::IsFinite(BounceThresholdCmPerSecond) && BounceThresholdCmPerSecond>=0 && BounceThresholdCmPerSecond<=1000;
}
namespace
{
struct FRow
{
    FKartPhysicsPair Pair;
    uint64 Id=0;
    FVector Point,RA,RB,N;
    double Mass=0,Bias=0,Lambda=0;
    FVector T1,T2;
    double K11=0,K22=0,K12=0,L1=0,L2=0;
    FKartContactMaterial Material;
};
FVector VelocityAt(const FKartRigidBodyState& B,const FVector& R)
{ return B.GetLinearVelocity()+FVector::CrossProduct(B.GetAngularVelocity(),R); }
bool Apply(FKartRigidBodyState& B,const FVector& R,const FVector& Impulse)
{
    return B.IsImmovable() || B.SetVelocities(B.GetLinearVelocity()+Impulse*B.GetInverseMass(),
        B.GetAngularVelocity()+B.GetWorldInverseInertia().TransformVector(FVector::CrossProduct(R,Impulse)));
}
bool LessVector(const FVector& A,const FVector& B)
{return A.X!=B.X?A.X<B.X:A.Y!=B.Y?A.Y<B.Y:A.Z<B.Z;}
}
FKartSolverResult FKartPhysicsSolver::Solve(TMap<FKartPhysicsBodyId,FKartRigidBodyState>& Bodies,
    const TArray<FKartContactManifold>& Manifolds,double Dt,const FKartSolverSettings& S,
    const TMap<FKartPhysicsBodyId,FKartContactMaterial>* Materials)
{
    FKartSolverResult Result;
    auto Fail=[&](const TCHAR* Reason){Result.Failure=Reason;return Result;};
    if(!S.IsValid() || !FMath::IsFinite(Dt) || Dt<=0 || Dt>.1)return Fail(TEXT("Invalid settings/timestep"));
    auto Next=Bodies; // Transactional isolated implementation; optimize only after profiling.
    TSet<FKartPhysicsPair> Seen;
    TArray<FRow> Rows;
    for(const auto& M:Manifolds)
    {
        if(M.Status==EKartManifoldStatus::NoContact)continue;
        if(M.Status!=EKartManifoldStatus::Valid || !M.Pair.IsValid() || Seen.Contains(M.Pair)
            || M.Normal.ContainsNaN() || FMath::Abs(M.Normal.SizeSquared()-1)>1.e-6
            || M.Points.Num()<1 || M.Points.Num()>4)return Fail(TEXT("Invalid or duplicate manifold"));
        Seen.Add(M.Pair);
        auto* A=Next.Find(M.Pair.First());auto* B=Next.Find(M.Pair.Second());
        if(!A || !B || !A->IsValid() || !B->IsValid())return Fail(TEXT("Missing/invalid body"));
        FKartContactMaterial MA,MB;
        if(Materials){if(const auto* V=Materials->Find(M.Pair.First()))MA=*V;if(const auto* V=Materials->Find(M.Pair.Second()))MB=*V;}
        if(!MA.IsValid() || !MB.IsValid())return Fail(TEXT("Invalid contact material"));
        for(const auto& P:M.Points)
        {
            if(P.WorldA.ContainsNaN() || P.WorldB.ContainsNaN() || P.LocalA.ContainsNaN() || P.LocalB.ContainsNaN()
                || !FMath::IsFinite(P.Depth) || P.Depth<0
                || !(P.WorldA-P.WorldB).Equals(M.Normal*P.Depth,1.e-4)
                || !(A->GetPosition()+A->GetOrientation().RotateVector(P.LocalA)).Equals(P.WorldA,1.e-4)
                || !(B->GetPosition()+B->GetOrientation().RotateVector(P.LocalB)).Equals(P.WorldB,1.e-4))
                return Fail(TEXT("Invalid/stale contact anchors"));
            FRow R;R.Pair=M.Pair;R.Id=P.PersistentId;R.Point=(P.WorldA+P.WorldB)*.5;
            R.RA=R.Point-A->GetPosition();R.RB=R.Point-B->GetPosition();R.N=M.Normal;
            const auto CA=FVector::CrossProduct(R.RA,R.N),CB=FVector::CrossProduct(R.RB,R.N);
            const double K=A->GetInverseMass()+B->GetInverseMass()
                +FVector::DotProduct(CA,A->GetWorldInverseInertia().TransformVector(CA))
                +FVector::DotProduct(CB,B->GetWorldInverseInertia().TransformVector(CB));
            if(!FMath::IsFinite(K) || K<0)return Fail(TEXT("Invalid effective mass"));
            if(A->IsImmovable() && B->IsImmovable())continue;
            if(K<1.e-12)return Fail(TEXT("Near-zero effective mass"));
            R.Mass=1/K;R.Bias=FMath::Min(S.MaxBiasCmPerSecond,S.BiasFraction*FMath::Max(0.,P.Depth-S.SlopCm)/Dt);
            R.Material=FKartContactMaterial::Combine(MA,MB);
            const double InitialV=FVector::DotProduct(VelocityAt(*B,R.RB)-VelocityAt(*A,R.RA),R.N);
            if(InitialV < -S.BounceThresholdCmPerSecond)R.Bias=FMath::Max(R.Bias,-R.Material.Restitution*InitialV);
            R.T1=FVector::CrossProduct(R.N,FMath::Abs(R.N.Z)<.8?FVector(0,0,1):FVector(0,1,0)).GetSafeNormal();
            R.T2=FVector::CrossProduct(R.N,R.T1);
            auto TangentK=[&](const FVector& U,const FVector& V){return (A->GetInverseMass()+B->GetInverseMass())*FVector::DotProduct(U,V)
                +FVector::DotProduct(FVector::CrossProduct(R.RA,U),A->GetWorldInverseInertia().TransformVector(FVector::CrossProduct(R.RA,V)))
                +FVector::DotProduct(FVector::CrossProduct(R.RB,U),B->GetWorldInverseInertia().TransformVector(FVector::CrossProduct(R.RB,V)));};
            R.K11=TangentK(R.T1,R.T1);R.K22=TangentK(R.T2,R.T2);R.K12=TangentK(R.T1,R.T2);
            if(!FMath::IsFinite(R.K11*R.K22-R.K12*R.K12) || R.K11*R.K22-R.K12*R.K12<=1.e-24)return Fail(TEXT("Invalid tangent mass"));
            Rows.Add(R);
        }
    }
    Rows.Sort([](const FRow& A,const FRow& B){if(!(A.Pair==B.Pair))return A.Pair<B.Pair;
        if(A.Id!=B.Id)return A.Id<B.Id;return LessVector(A.Point,B.Point);});
    for(int32 Iter=0;Iter<S.Iterations;++Iter)for(auto& R:Rows)
    {
        auto& A=Next.FindChecked(R.Pair.First());auto& B=Next.FindChecked(R.Pair.Second());
        const double V=FVector::DotProduct(VelocityAt(B,R.RB)-VelocityAt(A,R.RA),R.N);
        const double L=FMath::Max(0.,R.Lambda+(R.Bias-V)*R.Mass);
        if(!FMath::IsFinite(L) || L>S.MaxImpulse)return Fail(TEXT("Impulse overflow/limit"));
        const FVector J=R.N*(L-R.Lambda);R.Lambda=L;
        if(!Apply(A,R.RA,-J) || !Apply(B,R.RB,J))return Fail(TEXT("Nonfinite velocity"));
        const auto Relative=VelocityAt(B,R.RB)-VelocityAt(A,R.RA);
        const double V1=FVector::DotProduct(Relative,R.T1),V2=FVector::DotProduct(Relative,R.T2);
        const double Det=R.K11*R.K22-R.K12*R.K12;
        double L1=R.L1-(R.K22*V1-R.K12*V2)/Det,L2=R.L2-(R.K11*V2-R.K12*V1)/Det;
        const double Length=FMath::Sqrt(L1*L1+L2*L2);
        if(!FMath::IsFinite(Length))return Fail(TEXT("Nonfinite friction impulse"));
        if(Length>R.Material.StaticFriction*R.Lambda && Length>0)
        {const double Scale=R.Material.DynamicFriction*R.Lambda/Length;L1*=Scale;L2*=Scale;}
        const auto TJ=R.T1*(L1-R.L1)+R.T2*(L2-R.L2);R.L1=L1;R.L2=L2;
        if(!Apply(A,R.RA,-TJ) || !Apply(B,R.RB,TJ))return Fail(TEXT("Nonfinite friction velocity"));
    }
    for(const auto& R:Rows)
    {
        const auto& A=Next.FindChecked(R.Pair.First());const auto& B=Next.FindChecked(R.Pair.Second());
        FKartSolvedContact C;C.Pair=R.Pair;C.PersistentId=R.Id;C.Point=R.Point;C.Normal=R.N;C.NormalImpulse=R.Lambda;
        C.TangentImpulse=R.T1*R.L1+R.T2*R.L2;
        C.ResidualClosingSpeed=FMath::Max(0.,R.Bias-FVector::DotProduct(VelocityAt(B,R.RB)-VelocityAt(A,R.RA),R.N));
        Result.Contacts.Add(C);
    }
    Bodies=MoveTemp(Next);Result.bSuccess=true;Result.Iterations=S.Iterations;return Result;
}
