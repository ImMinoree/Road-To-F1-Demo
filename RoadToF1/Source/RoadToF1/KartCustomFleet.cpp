#include "KartCustomFleet.h"
#include "KartStaticMeshGeometry.h"
#include "KartPhysicsIntegrator.h"
#include "KartPhysicsSolver.h"
#include "HAL/PlatformTime.h"

namespace
{
double BoxGap(const FKartConvexShape& A,const FKartConvexShape& B,FVector& Normal)
{
    FVector AX[3],BX[3];for(int I=0;I<3;++I){FVector V=FVector::ZeroVector;V[I]=1;AX[I]=A.Orientation.RotateVector(V);BX[I]=B.Orientation.RotateVector(V);}
    const auto D=B.Position-A.Position;double Gap=-TNumericLimits<double>::Max();
    auto Test=[&](FVector N){const double L=N.Size();if(L<1.e-12)return;N/=L;double R=0;
        for(int I=0;I<3;++I)R+=A.HalfExtents[I]*FMath::Abs(FVector::DotProduct(AX[I],N))+B.HalfExtents[I]*FMath::Abs(FVector::DotProduct(BX[I],N));
        const double G=FMath::Abs(FVector::DotProduct(D,N))-R;if(G>Gap){Gap=G;Normal=FVector::DotProduct(D,N)>=0?N:-N;}};
    for(int I=0;I<3;++I){Test(AX[I]);Test(BX[I]);for(int J=0;J<3;++J)Test(FVector::CrossProduct(AX[I],BX[J]));}return Gap;
}
}

void FKartCustomFleet::ResetCaches(){Tree=FKartPhysicsBroadphase();Cache.Reset();Diagnostics={};LastContacts.Reset();StepMilliseconds.Reset();}
bool FKartCustomFleet::Remove(FKartPhysicsBodyId Id)
{
    const int Removed=Bodies.RemoveAll([&](const FKartFleetBody& B){return B.Id==Id;});
    FKartPhysicsProxy P;if(Tree.GetProxy(Id,P))Tree.RemoveProxy(Id);Cache.Reset();return Removed==1;
}
bool FKartCustomFleet::Step(const UWorld& World,const FKartStaticMeshGeometry& Scene,double Dt,FString& Failure)
{
    const double Begin=FPlatformTime::Seconds();const bool Ok=StepRefined(World,Scene,Dt,Failure,0);
    const double Ms=(FPlatformTime::Seconds()-Begin)*1000;
    ++Diagnostics.WorldSteps;Diagnostics.TotalWorldStepMilliseconds+=Ms;Diagnostics.MaxWorldStepMilliseconds=FMath::Max(Diagnostics.MaxWorldStepMilliseconds,Ms);return Ok;
}
bool FKartCustomFleet::StepRefined(const UWorld& World,const FKartStaticMeshGeometry& Scene,double Dt,FString& Failure,int32 Level)
{
    // A speculative velocity projection is only locally linear in rotation.
    // Validate poses before publication. Retry the ENTIRE shared batch, never
    // one kart: rejected trials cannot deliver a second physical impulse.
    const auto SavedBodies=Bodies;const auto SavedTree=Tree;const auto SavedCache=Cache;const auto SavedDiagnostics=Diagnostics;
    const int SampleCount=StepMilliseconds.Num();
    if(StepOnce(World,Scene,Dt,Failure))return true;
    const bool Retry=Failure==TEXT("Refine dynamic motion");
    Bodies=SavedBodies;Tree=SavedTree;Cache=SavedCache;Diagnostics=SavedDiagnostics;StepMilliseconds.SetNum(SampleCount);LastContacts.Reset();
    if(!Retry || Level>=8)return false;
    ++Diagnostics.RefinedSteps;Diagnostics.MaxRefinement=FMath::Max(Diagnostics.MaxRefinement,Level+1);
    if(!StepRefined(World,Scene,Dt*.5,Failure,Level+1))return false;
    const auto First=Bodies;const auto FirstContacts=LastContacts;
    if(!StepRefined(World,Scene,Dt*.5,Failure,Level+1))
    {
        Bodies=SavedBodies;Tree=SavedTree;Cache=SavedCache;Diagnostics=SavedDiagnostics;StepMilliseconds.SetNum(SampleCount);LastContacts.Reset();return false;
    }
    LastContacts.Append(FirstContacts);
    for(auto& E:Bodies)for(const auto& A:First)if(E.Id==A.Id)
    {
        E.Impulse+=A.Impulse;E.SharedImpulse+=A.SharedImpulse;E.SceneImpulse+=A.SceneImpulse;
        for(const auto& B:SavedBodies)if(E.Id==B.Id)E.Vehicle.Acceleration=(E.Vehicle.Body.GetLinearVelocity()-B.Vehicle.Body.GetLinearVelocity())/Dt;
    }
    Failure.Empty();return true;
}
bool FKartCustomFleet::StepOnce(const UWorld& World,const FKartStaticMeshGeometry& Scene,double Dt,FString& Failure)
{
    Failure.Empty();LastContacts.Reset();const double Begin=FPlatformTime::Seconds();
    auto Fail=[&](const TCHAR* Why){Failure=Why;return false;};
    if(!FMath::IsFinite(Dt) || Dt<=0 || Dt>1./120+1.e-12 || Bodies.IsEmpty() || Bodies.Num()>20)return Fail(TEXT("Invalid fleet size/timestep"));
    if(!Scene.IsCurrent())return Fail(TEXT("Fleet static scene snapshot stale"));
    auto Next=Bodies;Next.Sort([](const FKartFleetBody& A,const FKartFleetBody& B){return A.Id<B.Id;});
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> Current;
    TMap<FKartPhysicsBodyId,FKartConvexShape> Shapes,Predicted;
    TMap<FKartPhysicsBodyId,FKartContactMaterial> Materials;
    TMap<FKartPhysicsBodyId,FVector> Before;
    double InteractionEnvelope=0;
    for(auto& E:Next)
    {
        if(!E.Id.IsValid() || E.Id.Value>MAX_uint32 || Current.Contains(E.Id))return Fail(TEXT("Invalid/duplicate fleet ID"));
        Before.Add(E.Id,E.Vehicle.Body.GetLinearVelocity());E.Impulse=E.SharedImpulse=E.SceneImpulse=FVector::ZeroVector;
        if(!FKartCustomSceneBridge::Prepare(World,E.Ignore,E.Vehicle,E.Input,Dt))
        {
            Failure=FString::Printf(TEXT("Fleet force preparation failed id%llu position%s velocity%s omega%s input%.9g/%.9g/%.9g dt%.17g"),E.Id.Value,*E.Vehicle.Body.GetPosition().ToString(),*E.Vehicle.Body.GetLinearVelocity().ToString(),*E.Vehicle.Body.GetAngularVelocity().ToString(),E.Input.Throttle,E.Input.Steer,E.Input.Brake,Dt);return false;
        }
        const auto& B=E.Vehicle.Body;Current.Add(E.Id,B);
        InteractionEnvelope=FMath::Max(InteractionEnvelope,Dt*(B.GetLinearVelocity().Size()+E.Vehicle.Settings.CollisionHalfExtent.Size()*B.GetAngularVelocity().Size()));
        FKartConvexShape Shape;Shape.Position=B.GetPosition();Shape.Orientation=B.GetOrientation();Shape.HalfExtents=E.Vehicle.Settings.CollisionHalfExtent;Shapes.Add(E.Id,Shape);
        auto Future=B;Future.ClearAccumulators();if(!FKartPhysicsIntegrator::Step(Future,Dt))return Fail(TEXT("Invalid fleet prediction"));
        auto P=Shape;P.Position=Future.GetPosition();P.Orientation=Future.GetOrientation();Predicted.Add(E.Id,P);
        FKartPhysicsAABB AABB,PAABB;Shape.Bounds(AABB);P.Bounds(PAABB);const auto Swept=FKartPhysicsAABB::Combine(AABB,PAABB);
        FKartPhysicsProxy Proxy;
        if(Tree.GetProxy(E.Id,Proxy)){if(!Tree.UpdateProxy(E.Id,Swept))return Fail(TEXT("Fleet proxy update failed"));}
        else {Proxy.Id=E.Id;Proxy.Bounds=Swept;if(!Tree.RegisterProxy(Proxy))return Fail(TEXT("Fleet proxy registration failed"));}
        Materials.Add(E.Id,FKartContactMaterial::Surface(EKartContactSurface::Bodywork));
    }
    const double PreparedAt=bProfileSteps?FPlatformTime::Seconds():Begin;
    // Impulses can accelerate a previously stationary middle kart. Include nearby
    // pairs within this step's possible motion even if free prediction never meets.
    for(const auto& E:Next)
    {
        FKartPhysicsAABB A,B,Expanded;Shapes[E.Id].Bounds(A);Predicted[E.Id].Bounds(B);
        FKartPhysicsAABB::Combine(A,B).Expanded(InteractionEnvelope+.02,Expanded);
        if(!Tree.UpdateProxy(E.Id,Expanded))return Fail(TEXT("Interaction envelope update failed"));
    }
    TArray<FKartPhysicsPair> Pairs;Tree.GeneratePairs(Pairs);
    for(const auto& E:Next){FKartPhysicsAABB Bounds;Shapes[E.Id].Bounds(Bounds);if(!Tree.UpdateProxy(E.Id,Bounds))return Fail(TEXT("Current proxy update failed"));}
    if(!Cache.Update(Tree,Pairs,Shapes) || Cache.GetDiagnostics().Failures>0)
    {
        Failure=TEXT("Fleet current contact geometry failed");
        for(const auto& M:Cache.GetResults())if(M.Status!=EKartManifoldStatus::Valid && M.Status!=EKartManifoldStatus::NoContact)
        {
            auto Pose=[](const FKartConvexShape& S){return FString::Printf(TEXT("P %.17g %.17g %.17g Q %.17g %.17g %.17g %.17g"),S.Position.X,S.Position.Y,S.Position.Z,S.Orientation.X,S.Orientation.Y,S.Orientation.Z,S.Orientation.W);};
            Failure+=FString::Printf(TEXT(" pair%llu:%llu status%d gjk%d epa%d method%d A{%s} B{%s}"),M.Pair.First().Value,M.Pair.Second().Value,int(M.Status),int(M.GJK),int(M.EPA),int(M.Method),*Pose(Shapes[M.Pair.First()]),*Pose(Shapes[M.Pair.Second()]));break;
        }
        return false;
    }
    TArray<FKartContactManifold> Constraints;TSet<FKartPhysicsPair> Seen;double Depth=0;
    for(auto M:Cache.GetResults())
    {
        if(Seen.Contains(M.Pair)){++Diagnostics.DuplicatePairs;return Fail(TEXT("Duplicate kart pair"));}Seen.Add(M.Pair);
        if(M.Status==EKartManifoldStatus::Valid)
        {
            FVector Axis;const double Actual=FMath::Max(0.,-BoxGap(Shapes[M.Pair.First()],Shapes[M.Pair.Second()],Axis));
            const bool Peak=Actual>Diagnostics.MaxActualBoxOverlapCm;Diagnostics.MaxActualBoxOverlapCm=FMath::Max(Diagnostics.MaxActualBoxOverlapCm,Actual);
            if(bProfileSteps && Peak)
            {
                auto State=[](const FKartRigidBodyState& B)
                {
                    const auto P=B.GetPosition(),V=B.GetLinearVelocity(),W=B.GetAngularVelocity();const auto Q=B.GetOrientation();
                    return FString::Printf(TEXT("P %.17g %.17g %.17g Q %.17g %.17g %.17g %.17g V %.17g %.17g %.17g W %.17g %.17g %.17g"),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,V.X,V.Y,V.Z,W.X,W.Y,W.Z);
                };
                double PointDepth=0;for(const auto& P:M.Points)PointDepth=FMath::Max(PointDepth,P.Depth);
                Diagnostics.DeepestDynamicPair=FString::Printf(TEXT("step%llu pair%llu:%llu SAT%.17g points%.17g method%d normal%s A{%s} B{%s}"),Diagnostics.Steps,M.Pair.First().Value,M.Pair.Second().Value,Actual,PointDepth,int(M.Method),*M.Normal.ToString(),*State(Current[M.Pair.First()]),*State(Current[M.Pair.Second()]));
                if(Actual>2)UE_LOG(LogTemp,Display,TEXT("CUSTOM_OVERLAP %s"),*Diagnostics.DeepestDynamicPair);
            }
        }
        if(M.Status==EKartManifoldStatus::NoContact)
        {
            M=KartManifolds::Build(M.Pair,Predicted[M.Pair.First()],Predicted[M.Pair.Second()]);
            if(M.Status!=EKartManifoldStatus::Valid && M.Status!=EKartManifoldStatus::NoContact)return Fail(TEXT("Fleet predicted contact geometry failed"));
            if(M.Status==EKartManifoldStatus::NoContact)
            {
                const auto& A=Shapes[M.Pair.First()];const auto& B=Shapes[M.Pair.Second()];FVector N;
                const double Gap=BoxGap(A,B,N);
                if(Gap<=2*InteractionEnvelope+.04)
                {
                    // Include the reachable face patch, not only its first nearly
                    // touching edge. Each original point keeps its own gap/dt bound.
                    // Otherwise a new chain impulse can rotate an unrepresented
                    // corner into the next kart despite satisfying that one edge.
                    const double Patch=FMath::Min(2*InteractionEnvelope+.04,.25*FMath::Min(A.HalfExtents.GetMin(),B.HalfExtents.GetMin()));
                    auto Probe=A;Probe.Position+=N*(FMath::Max(0.,Gap)+FMath::Max(.001,Patch));
                    M=KartManifolds::Build(M.Pair,Probe,B);
                    if(M.Status==EKartManifoldStatus::Valid && FVector::DotProduct(M.Normal,N)<.999)
                    {Probe=A;Probe.Position+=N*(FMath::Max(0.,Gap)+.001);M=KartManifolds::Build(M.Pair,Probe,B);}
                    if(M.Status!=EKartManifoldStatus::Valid && M.Status!=EKartManifoldStatus::NoContact)return Fail(TEXT("Fleet gap patch failed"));
                }
            }
            if(M.Status==EKartManifoldStatus::Valid)
            {
                const auto& A=Current[M.Pair.First()];const auto& B=Current[M.Pair.Second()];
                for(auto& P:M.Points)
                {
                    const auto WA=A.GetPosition()+A.GetOrientation().RotateVector(P.LocalA),WB=B.GetPosition()+B.GetOrientation().RotateVector(P.LocalB);
                    const double Gap=FMath::Max(0.,FVector::DotProduct(WB-WA,M.Normal));
                    P.WorldA=P.WorldB=(WA+WB)*.5;P.Depth=0;P.AllowedClosingSpeed=Gap/Dt;
                    P.LocalA=A.GetOrientation().UnrotateVector(P.WorldA-A.GetPosition());P.LocalB=B.GetOrientation().UnrotateVector(P.WorldB-B.GetPosition());
                }
            }
        }
        if(M.Status==EKartManifoldStatus::Valid){for(const auto& P:M.Points)Depth=FMath::Max(Depth,P.Depth);Constraints.Add(M);}
    }
    const int DynamicPairs=Constraints.Num();int StaticPairs=0;
    // Ground/barrier and kart constraints must be coupled. Otherwise a later
    // floor impulse can invalidate a preceding kart-pair projection in a chain.
    for(const auto& E:Next)
    {
        auto Shape=Shapes[E.Id],Future=Predicted[E.Id];const FVector Offset=E.Vehicle.Settings.StaticShapeOffset();
        Shape.HalfExtents=Future.HalfExtents=E.Vehicle.Settings.StaticShapeHalfExtent();
        Shape.Position+=Shape.Orientation.RotateVector(Offset);Future.Position+=Future.Orientation.RotateVector(Offset);
        FKartPhysicsAABB A,B,Query;Shape.Bounds(A);Future.Bounds(B);FKartPhysicsAABB::Combine(A,B).Expanded(.02,Query);
        TArray<int32> Near;Scene.Query(Query,Near);
        for(int Index:Near)
        {
            const FKartPhysicsBodyId StaticId{(1ull<<32)+uint64(Index)};const auto& Triangle=Scene.GetTriangles()[Index];
            auto M=Scene.Contact({E.Id,StaticId},Shape,Triangle);
            if(M.Status==EKartManifoldStatus::NoContact)
            {
                M=Scene.Contact({E.Id,StaticId},Future,Triangle);
                if(M.Status==EKartManifoldStatus::Valid)
                {
                    const auto& Body=Current[E.Id];
                    for(auto& P:M.Points)
                    {
                        const FVector WA=Body.GetPosition()+Body.GetOrientation().RotateVector(P.LocalA+Offset),WB=P.WorldB;
                        const double Gap=FMath::Max(0.,FVector::DotProduct(WB-WA,M.Normal));
                        P.WorldA=P.WorldB=(WA+WB)*.5;P.Depth=0;P.AllowedClosingSpeed=Gap/Dt;
                        P.LocalA=Body.GetOrientation().UnrotateVector(P.WorldA-Body.GetPosition());P.LocalB=P.WorldB;
                    }
                }
            }
            if(M.Status==EKartManifoldStatus::NoContact)continue;
            if(M.Status!=EKartManifoldStatus::Valid)return Fail(TEXT("Fleet static contact geometry failed"));
            // Contact builder anchors are chassis-local; solver bodies remain
            // at their actual mass origin. Reconstruct every anchor explicitly.
            for(auto& P:M.Points)P.LocalA=Current[E.Id].GetOrientation().UnrotateVector(P.WorldA-Current[E.Id].GetPosition());
            FKartRigidBodyState Static;Static.SetImmovable(true);Current.Add(StaticId,Static);
            Materials.Add(StaticId,FKartContactMaterial::Surface(EKartContactSurface::Bodywork));
            for(const auto& P:M.Points)Depth=FMath::Max(Depth,P.Depth);
            Constraints.Add(M);++StaticPairs;
        }
    }
    const double GeometryAt=bProfileSteps?FPlatformTime::Seconds():PreparedAt;
    FKartSolverSettings Physical;Physical.BiasFraction=0;
    const auto Unsolved=Current;
    auto Solved=FKartPhysicsSolver::Solve(Current,Constraints,Dt,Physical,&Materials);
    if(!Solved.bSuccess){Failure=Solved.Failure;return false;}
    auto Residual=[&](){double Error=0;for(const auto& C:Solved.Contacts)Error=FMath::Max(Error,C.ResidualClosingSpeed);return Error;};
    const double InitialResidual=Residual();
    if(InitialResidual>.05 && Constraints.Num()>1)
    {
        // Retry from untouched velocities, committing only the converged solve.
        // No extra physical impulse is applied to the scene or actors.
        Current=Unsolved;Physical.Iterations=128;
        Solved=FKartPhysicsSolver::Solve(Current,Constraints,Dt,Physical,&Materials);
        if(!Solved.bSuccess){Failure=Solved.Failure;return false;}++Diagnostics.AdaptiveSolves;
    }
    Diagnostics.LastNormalResidual=Residual();Diagnostics.MaxNormalResidual=FMath::Max(Diagnostics.MaxNormalResidual,Residual());Diagnostics.LastIterations=Physical.Iterations;
    if(bDetailedDiagnostics)
    {
        LastConstraintSummary=FString::Printf(TEXT("SOLVE initialResidual%.9g finalResidual%.9g iterations%d"),InitialResidual,Residual(),Physical.Iterations);
        for(const auto& M:Constraints){double GapSpeed=0;for(const auto& P:M.Points)GapSpeed=FMath::Max(GapSpeed,P.AllowedClosingSpeed);
            LastConstraintSummary+=FString::Printf(TEXT(" | pair%llu:%llu n%s points%d maxAllowed%.9g"),M.Pair.First().Value,M.Pair.Second().Value,*M.Normal.ToString(),M.Points.Num(),GapSpeed);}
    }
    for(const auto& C:Solved.Contacts)
    {
        const auto J=C.Normal*C.NormalImpulse+C.TangentImpulse;
        for(auto& E:Next){if(E.Id==C.Pair.First())E.Impulse-=J;else if(E.Id==C.Pair.Second())E.Impulse+=J;}
        if(bCaptureContactDiagnostics && C.NormalImpulse>.01)
        {
            FKartFleetContactDiagnostic Debug;Debug.Contact=C;
            if(C.Pair.Second().Value>=(1ull<<32))
            {
                const int T=int(C.Pair.Second().Value-(1ull<<32));
                Debug.Source=Scene.GetSources()[Scene.GetTriangles()[T].Source].Path;
            }
            else Debug.Source=TEXT("Kart pair");
            for(const auto& M:Constraints)if(M.Pair==C.Pair)
                for(const auto& P:M.Points)if(P.WorldA.Equals(C.Point,.5) || P.PersistentId==C.PersistentId)
                {Debug.Depth=FMath::Max(Debug.Depth,P.Depth);Debug.AllowedClosingSpeed=FMath::Max(Debug.AllowedClosingSpeed,P.AllowedClosingSpeed);}
            LastContacts.Add(MoveTemp(Debug));
        }
    }
    TMap<FKartPhysicsBodyId,FKartRigidBodyState> Correction;
    if(Depth>Physical.SlopCm)
    {
        Correction=Current;for(auto& E:Correction)E.Value.SetVelocities(FVector::ZeroVector,FVector::ZeroVector);
        const auto Push=FKartPhysicsSolver::Solve(Correction,Constraints,Dt);
        if(!Push.bSuccess){Failure=Push.Failure;return false;}
    }
    const double SolvedAt=bProfileSteps?FPlatformTime::Seconds():GeometryAt;
    for(auto& E:Next)
    {
        E.Vehicle.Body=Current[E.Id];
        if(Correction.Num())
        {
            auto Push=Correction[E.Id];Push.ClearAccumulators();if(!FKartPhysicsIntegrator::Step(Push,Dt))return Fail(TEXT("Fleet split correction invalid"));
            E.Vehicle.Body.SetPose(Push.GetPosition(),Push.GetOrientation());
        }
        FVector StaticImpulse;FKartSceneStepStats Stats;
        if(!FKartCustomSceneBridge::Move(World,E.Ignore,E.Vehicle,Dt,StaticImpulse,&Failure,&Stats,Materials[E.Id],Scene))return false;
        E.SharedImpulse=E.Impulse;E.SceneImpulse=StaticImpulse;
        E.Impulse+=StaticImpulse;E.Vehicle.Acceleration=(E.Vehicle.Body.GetLinearVelocity()-Before[E.Id])/Dt;
        if(E.Vehicle.Body.GetLinearVelocity().Size()>6000 || E.Vehicle.Body.GetAngularVelocity().Size()>30)return Fail(TEXT("Fleet numerical velocity guard"));
    }
    // Independent OBB SAT on final poses catches angular/chain motion that
    // invalidated the pre-motion linearized constraints. Query the tree again
    // so newly overlapping pairs cannot be missed by the original candidate set.
    TMap<FKartPhysicsBodyId,FKartConvexShape> FinalShapes;
    for(const auto& E:Next)
    {
        auto S=Shapes[E.Id];S.Position=E.Vehicle.Body.GetPosition();S.Orientation=E.Vehicle.Body.GetOrientation();FinalShapes.Add(E.Id,S);
        FKartPhysicsAABB Bounds;S.Bounds(Bounds);if(!Tree.UpdateProxy(E.Id,Bounds))return Fail(TEXT("Final proxy update failed"));
    }
    TArray<FKartPhysicsPair> FinalPairs;Tree.GeneratePairs(FinalPairs);
    for(const auto& Pair:FinalPairs)
    {
        FVector N;const double BeforeDepth=FMath::Max(0.,-BoxGap(Shapes[Pair.First()],Shapes[Pair.Second()],N));
        const double AfterDepth=FMath::Max(0.,-BoxGap(FinalShapes[Pair.First()],FinalShapes[Pair.Second()],N));
        // One millimetre integration tolerance, below the unchanged two-
        // millimetre regression bound; not a reduction of occupancy margins.
        if(AfterDepth>FMath::Max(.1,BeforeDepth+1.e-6))return Fail(TEXT("Refine dynamic motion"));
        Diagnostics.MaxActualBoxOverlapCm=FMath::Max(Diagnostics.MaxActualBoxOverlapCm,AfterDepth);
    }
    Bodies=MoveTemp(Next);++Diagnostics.Steps;Diagnostics.LastCandidates=Pairs.Num();Diagnostics.LastPairs=DynamicPairs;Diagnostics.PairsSolved+=DynamicPairs;Diagnostics.StaticPairsSolved+=StaticPairs;
    Diagnostics.ContactRows+=Solved.Contacts.Num();Diagnostics.ReusedContacts+=Cache.GetDiagnostics().Reused;Diagnostics.MaxCurrentPenetrationCm=FMath::Max(Diagnostics.MaxCurrentPenetrationCm,Depth);
    const double End=FPlatformTime::Seconds(),Milliseconds=(End-Begin)*1000;Diagnostics.TotalStepMilliseconds+=Milliseconds;Diagnostics.MaxStepMilliseconds=FMath::Max(Diagnostics.MaxStepMilliseconds,Milliseconds);
    if(bProfileSteps)
    {
        Diagnostics.PrepareMilliseconds+=(PreparedAt-Begin)*1000;Diagnostics.GeometryMilliseconds+=(GeometryAt-PreparedAt)*1000;
        Diagnostics.SolverMilliseconds+=(SolvedAt-GeometryAt)*1000;Diagnostics.MotionMilliseconds+=(End-SolvedAt)*1000;
        if(StepMilliseconds.Num()<250000)StepMilliseconds.Add(Milliseconds);
    }
    return true;
}
