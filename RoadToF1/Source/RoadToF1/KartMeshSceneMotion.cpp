#include "KartMeshSceneMotion.h"
#include "KartPhysicsIntegrator.h"
#include "KartPhysicsSolver.h"
namespace
{
constexpr double ContactMargin=.02;
bool SolveContacts(const FKartStaticMeshGeometry& Scene,FKartExperimentalVehicle& V,double Dt,
    FVector& Impulse,FString* Failure,FKartSceneStepStats* Stats,const FKartContactMaterial& Material)
{
    FKartConvexShape Shape;Shape.Orientation=V.Body.GetOrientation();Shape.Position=V.Body.GetPosition()+Shape.Orientation.RotateVector(V.Settings.StaticShapeOffset());
    Shape.HalfExtents=V.Settings.StaticShapeHalfExtent();
    FKartPhysicsAABB Bounds,Query;Shape.Bounds(Bounds);Bounds.Expanded(ContactMargin,Query);
    TArray<int32> Candidates;Scene.Query(Query,Candidates);if(Stats)Stats->TriangleCandidates+=Candidates.Num();
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> Bodies;Bodies.Add({1},V.Body);
    TMap<FKartPhysicsBodyId,FKartContactMaterial> Materials;Materials.Add({1},Material);
    TArray<FKartContactManifold> Contacts;
    for(int Index:Candidates)
    {
        const auto& T=Scene.GetTriangles()[Index];FVector N;const double Gap=Scene.Separation(Shape,T,N);
        if(Gap>ContactMargin)continue;
        const FKartPhysicsBodyId Id{(1ull<<32)+uint64(Index)};auto Probe=Shape;
        if(Gap>0)Probe.HalfExtents+=FVector(ContactMargin*1.5);
        auto M=Scene.Contact({{1},Id},Probe,T);
        if(M.Status==EKartManifoldStatus::NoContact)continue;
        if(M.Status!=EKartManifoldStatus::Valid){if(Failure)*Failure=TEXT("Invalid finite triangle contact");return false;}
        if(Gap>0)for(auto& P:M.Points)
        {
            // Speculative zero-depth contact across a 0.02cm margin, not penetration.
            FVector Near=Shape.Orientation.UnrotateVector(P.WorldB-Shape.Position);
            for(int I=0;I<3;++I)Near[I]=FMath::Clamp(Near[I],-Shape.HalfExtents[I],Shape.HalfExtents[I]);
            const FVector OnBody=Shape.Position+Shape.Orientation.RotateVector(Near);
            P.WorldA=P.WorldB=(P.WorldB+OnBody)*.5;P.Depth=0;
            P.LocalA=Shape.Orientation.UnrotateVector(P.WorldA-Shape.Position);P.LocalB=P.WorldB;
        }
        for(auto& P:M.Points)P.LocalA=V.Body.GetOrientation().UnrotateVector(P.WorldA-V.Body.GetPosition());
        FKartRigidBodyState Static;Static.SetImmovable(true);Bodies.Add(Id,Static);Materials.Add(Id,Material);Contacts.Add(M);
        if(Stats)for(const auto& P:M.Points)Stats->MaxPenetrationCm=FMath::Max(Stats->MaxPenetrationCm,P.Depth);
    }
    if(Contacts.IsEmpty())return true;
    FKartSolverSettings Settings;Settings.BiasFraction=0;
    const auto Solved=FKartPhysicsSolver::Solve(Bodies,Contacts,Dt,Settings,&Materials);
    if(!Solved.bSuccess){if(Failure)*Failure=Solved.Failure;return false;}
    V.Body=Bodies[{1}];for(const auto& C:Solved.Contacts)Impulse-=C.Normal*C.NormalImpulse+C.TangentImpulse;
    if(Stats){Stats->Contacts+=Solved.Contacts.Num();Stats->Iterations+=Solved.Iterations;
        for(const auto& C:Solved.Contacts)Stats->MaxResidualClosingSpeed=FMath::Max(Stats->MaxResidualClosingSpeed,C.ResidualClosingSpeed);}
    bool Push=false;for(const auto& M:Contacts)for(const auto& P:M.Points)Push|=P.Depth>Settings.SlopCm;
    if(Push)
    {
        for(auto& B:Bodies)B.Value.SetVelocities(FVector::ZeroVector,FVector::ZeroVector);
        const auto R=FKartPhysicsSolver::Solve(Bodies,Contacts,Dt);
        if(!R.bSuccess){if(Failure)*Failure=R.Failure;return false;}
        auto B=Bodies[{1}];B.ClearAccumulators();if(!FKartPhysicsIntegrator::Step(B,Dt))return false;
        V.Body.SetPose(B.GetPosition(),B.GetOrientation());if(Stats)Stats->Iterations+=R.Iterations;
    }
    return true;
}
// Conservative advancement along a normalized pose interval. SAT separation is a
// lower bound on distance; translation projection + angular support motion bounds
// how fast that particular separating plane can close. No triangle AABB response.
bool CastTriangleInternal(const FKartConvexShape& Start,const FKartConvexShape& End,const FKartStaticTriangle& T,
    double& Time,bool& Hit,FString* Failure,const FVector& Offset=FVector::ZeroVector)
{
    Hit=false;Time=1;FVector N;
    auto PoseAt=[&](double At){auto Q=Start;Q.Orientation=FQuat::Slerp(Start.Orientation,End.Orientation,At);Q.Position=FMath::Lerp(Start.Position,End.Position,At)+Q.Orientation.RotateVector(Offset);return Q;};
    if(FKartStaticMeshGeometry::Separation(PoseAt(0),T,N)<=ContactMargin)return true; // Already constrained by pre-solve.
    const auto D=End.Position-Start.Position;const double AngularBound=(Start.HalfExtents.Size()+Offset.Size())*Start.Orientation.AngularDistance(End.Orientation);
    auto Delta=(End.Orientation*Start.Orientation.Inverse()).GetNormalized();if(Delta.W<0)Delta=Delta*-1.;
    FVector Axis;double Angle;Delta.ToAxisAndAngle(Axis,Angle);
    auto ProjectedAngular=[&](const FQuat& Q,const FVector& PlaneNormal,double IntervalRadius)
    {
        if(Angle<1.e-12)return 0.;double Bound=0;const double NP=FVector::CrossProduct(Axis,PlaneNormal).Size();
        for(int I=0;I<3;++I)
        {
            FVector Local=FVector::ZeroVector;Local[I]=1;const FVector A=Q.RotateVector(Local),Derivative=FVector::CrossProduct(Axis,A);
            // Fixed-axis slerp: projected support derivative plus its bounded
            // variation over this angular interval, for every OBB corner.
            Bound+=Start.HalfExtents[I]*(FMath::Abs(FVector::DotProduct(Derivative,PlaneNormal))+IntervalRadius*Angle*NP*Derivative.Size());
        }
        const auto OffsetDerivative=FVector::CrossProduct(Axis,Q.RotateVector(Offset));
        Bound+=FMath::Abs(FVector::DotProduct(OffsetDerivative,PlaneNormal))+IntervalRadius*Angle*NP*OffsetDerivative.Size();
        return Angle*Bound;
    };
    double At=0;
    for(int Iter=0;Iter<96;++Iter)
    {
        auto Q=PoseAt(At);
        const double Gap=FKartStaticMeshGeometry::Separation(Q,T,N);
        // Conservative advancement approaches the skin asymptotically. Terminate
        // by spatial accuracy, not floating-point equality to the exact skin.
        // This1nm tolerance stays well inside the existing0.02cm contact margin.
        if(Gap<=ContactMargin*.5+1.e-7){Time=At;Hit=true;return true;}
        const double Closing=FMath::Max(0.,FVector::DotProduct(D,N))+ProjectedAngular(Q.Orientation,N,1-At);
        if(Closing<1.e-12 || Gap-ContactMargin*.5>Closing*(1-At))return true;
        const double Advance=.9*(Gap-ContactMargin*.5)/Closing;
        if(Advance<1.e-12){Time=At;Hit=true;return true;}
        At+=Advance;if(At>=1)return true;
    }
    // Near tangencies can make the global angular closing bound much larger
    // than actual closure. Certify the remaining interval by left-first bounded
    // subdivision instead of taking hundreds of vanishing advancement steps.
    // Any point moves <= translation + radius*angle. A separating midpoint
    // plane with more clearance than that bound proves the WHOLE interval clear.
    struct FInterval{double L,R;int Depth;};TArray<FInterval,TInlineAllocator<64>> Pending;Pending.Add({At,1,0});
    for(int Visit=0;Visit<4096 && !Pending.IsEmpty();++Visit)
    {
        const auto I=Pending.Pop(EAllowShrinking::No);const double Mid=(I.L+I.R)*.5;
        auto Q=PoseAt(Mid);
        const double Gap=FKartStaticMeshGeometry::Separation(Q,T,N);
        const double Radius=(I.R-I.L)*.5;
        const double Motion=Radius*(FMath::Abs(FVector::DotProduct(D,N))+ProjectedAngular(Q.Orientation,N,Radius));
        if(Gap>ContactMargin*.5+Motion)continue;
        if(Motion<=1.e-7){Time=I.L;Hit=true;return true;}
        if(I.Depth>=30){if(Failure)*Failure=TEXT("Triangle interval depth exhausted");return false;}
        Pending.Add({Mid,I.R,I.Depth+1});Pending.Add({I.L,Mid,I.Depth+1});
    }
    if(Pending.IsEmpty())return true;
    if(Failure)*Failure=FString::Printf(TEXT("Triangle interval certificate budget exhausted at%.12g angular%.12g start%s end%s triangle%s;%s;%s"),At,AngularBound,*Start.Position.ToString(),*End.Position.ToString(),*T.V[0].ToString(),*T.V[1].ToString(),*T.V[2].ToString());return false;
}
}
bool KartMeshMotion::CastTriangle(const FKartConvexShape& A,const FKartConvexShape& B,const FKartStaticTriangle& T,double& Time,bool& Hit,FString* Failure)
{return CastTriangleInternal(A,B,T,Time,Hit,Failure);}
bool KartMeshMotion::Advance(const FKartStaticMeshGeometry& Scene,FKartExperimentalVehicle& V,
    const FVector& Start,const FQuat& Rotation,double Dt,FVector& Impulse,FString* Failure,
    FKartSceneStepStats* Stats,const FKartContactMaterial& Material)
{
    if(!Scene.IsCurrent()){if(Failure)*Failure=TEXT("Static collision snapshot changed; rebuild required");return false;}
    V.Body.SetPose(Start,Rotation);V.Body.ClearAccumulators();double Remaining=Dt;
    for(int Event=0;Event<16;++Event)
    {
        if(!SolveContacts(Scene,V,Remaining,Impulse,Failure,Stats,Material))return false;
        auto Predicted=V.Body;Predicted.ClearAccumulators();if(!FKartPhysicsIntegrator::Step(Predicted,Remaining))return false;
        FKartConvexShape A,B;A.Position=V.Body.GetPosition();A.Orientation=V.Body.GetOrientation();
        A.HalfExtents=V.Settings.StaticShapeHalfExtent();B=A;B.Position=Predicted.GetPosition();B.Orientation=Predicted.GetOrientation();
        const FVector Offset=V.Settings.StaticShapeOffset();auto SA=A,SB=B;SA.Position+=SA.Orientation.RotateVector(Offset);SB.Position+=SB.Orientation.RotateVector(Offset);
        FKartPhysicsAABB BA,BB,Query;SA.Bounds(BA);SB.Bounds(BB);
        const double Arc=(A.HalfExtents.Size()+Offset.Size())*A.Orientation.AngularDistance(B.Orientation);
        FKartPhysicsAABB::Combine(BA,BB).Expanded(Arc+ContactMargin,Query);
        TArray<int32> Candidates;Scene.Query(Query,Candidates);double Earliest=1;
        for(int Index:Candidates)
        {
            double Time;bool Hit;if(Stats)++Stats->MeshSweeps;
            if(!CastTriangleInternal(A,B,Scene.GetTriangles()[Index],Time,Hit,Failure,Offset)){if(Failure)*Failure+=FString::Printf(TEXT(" source%d triangle%d"),Scene.GetTriangles()[Index].Source,Index);return false;}
            if(Hit)Earliest=FMath::Min(Earliest,Time);
        }
        V.Body.SetPose(FMath::Lerp(A.Position,B.Position,Earliest),FQuat::Slerp(A.Orientation,B.Orientation,Earliest));
        Remaining*=1-Earliest;
        if(Earliest>=1 || Remaining<1.e-7 || Remaining*(V.Body.GetLinearVelocity().Size()+A.HalfExtents.Size()*V.Body.GetAngularVelocity().Size())<1.e-6)
            return SolveContacts(Scene,V,FMath::Max(Remaining,1.e-7),Impulse,Failure,Stats,Material);
    }
    if(Failure)*Failure=TEXT("Mesh motion contact budget exhausted");return false;
}
