#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartPhysicsNarrowphase.h"
#include "KartRigidBodyState.h"
#include "KartPawn.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include <limits>

namespace KartStage3Tests
{
FKartConvexShape Box(FVector P=FVector::ZeroVector,FVector E=FVector(10),FQuat Q=FQuat::Identity)
{FKartConvexShape S;S.Position=P;S.HalfExtents=E;S.Orientation=Q;return S;}
FKartConvexShape Sphere(FVector P,double R=10)
{auto S=Box(P);S.Kind=EKartConvexKind::Sphere;S.Radius=R;return S;}
FKartConvexShape Hull()
{
    auto S=Box();S.Kind=EKartConvexKind::Hull;
    for(double X:{-10.,10.})for(double Y:{-10.,10.})for(double Z:{-10.,10.})S.Vertices.Add(FVector(X,Y,Z));return S;
}
// Independent OBB SAT oracle, test-only: 6 face axes plus 9 edge cross products.
double SAT(const FKartConvexShape& A,const FKartConvexShape& B)
{
    FVector X[3],Y[3];for(int32 I=0;I<3;++I){FVector U=FVector::ZeroVector;U[I]=1;X[I]=A.Orientation.RotateVector(U);Y[I]=B.Orientation.RotateVector(U);}
    TArray<FVector> Axes;for(int32 I=0;I<3;++I){Axes.Add(X[I]);Axes.Add(Y[I]);for(int32 J=0;J<3;++J)Axes.Add(FVector::CrossProduct(X[I],Y[J]));}
    double Depth=1.e100;
    for(auto N:Axes)
    {
        if(N.Size()<1.e-8)continue;N/=N.Size();double RA=0,RB=0;
        for(int32 I=0;I<3;++I){RA+=A.HalfExtents[I]*FMath::Abs(FVector::DotProduct(X[I],N));RB+=B.HalfExtents[I]*FMath::Abs(FVector::DotProduct(Y[I],N));}
        Depth=FMath::Min(Depth,RA+RB-FMath::Abs(FVector::DotProduct(B.Position-A.Position,N)));
    }
    return Depth;
}
bool Finite(const FKartContactGeometry& R)
{return !R.Normal.ContainsNaN() && !R.PointOnA.ContainsNaN() && !R.PointOnB.ContainsNaN() && FMath::IsFinite(R.Depth) && R.Depth>=0;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartStage3Test,"RoadToF1.Physics.Stage3Narrowphase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartStage3Test::RunTest(const FString& Parameters)
{
    using namespace KartStage3Tests;
    FVector P;const auto A=Box();auto H=Hull();
    TestTrue(TEXT("Box support"),A.Support(FVector(1,-2,3),P) && P==FVector(10,-10,10));
    TestTrue(TEXT("Sphere support"),Sphere(FVector(5,0,0)).Support(FVector(0,2,0),P) && P==FVector(5,10,0));
    TestTrue(TEXT("Hull support"),H.Support(FVector(1,-2,3),P) && P==FVector(10,-10,10));
    TestTrue(TEXT("Zero direction deterministic"),A.Support(FVector::ZeroVector,P) && P==FVector(10));
    const auto Rot=Box(FVector(3,4,5),FVector(2,4,6),FQuat(FVector(0,0,1),3.14159265358979323846/2));
    TestTrue(TEXT("Rotated support"),Rot.Support(FVector(1,1,1),P) && P.Equals(FVector(7,6,11),1.e-9));
    FKartSupportPoint SP;TestTrue(TEXT("Minkowski correspondence"),KartNarrowphase::SupportMinkowski(A,Rot,FVector(1,2,3),SP)
        && SP.Minkowski.Equals(SP.PointOnA-SP.PointOnB,1.e-9));
    auto Invalid=A;Invalid.HalfExtents.X=-1;TestFalse(TEXT("Negative extents"),Invalid.IsValid());
    Invalid=Sphere(FVector::ZeroVector,-1);TestFalse(TEXT("Negative radius"),Invalid.IsValid());
    Invalid=A;Invalid.Position.X=std::numeric_limits<double>::infinity();TestFalse(TEXT("Infinite input"),Invalid.IsValid());
    Invalid=A;Invalid.Orientation=FQuat(0,0,0,0);TestFalse(TEXT("Zero quaternion"),Invalid.IsValid());
    Invalid=H;Invalid.Vertices.Empty();TestFalse(TEXT("Empty hull"),Invalid.IsValid());
    Invalid=H;for(auto& V:Invalid.Vertices)V.Z=0;TestFalse(TEXT("Coplanar hull rejected"),Invalid.IsValid());
    TestEqual(TEXT("Invalid query explicit"),KartNarrowphase::Query(Invalid,A).GJK,EKartGJKStatus::InvalidInput);
    Invalid=H;Invalid.Vertices[0].X=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN hull rejected"),Invalid.IsValid());
    auto Check=[&](const TCHAR* Name,const FKartConvexShape& X,const FKartConvexShape& Y,double Expected,double Tolerance)
    {
        const auto R=KartNarrowphase::Query(X,Y);
        TestTrue(Name,R.GJK==EKartGJKStatus::Intersecting && R.EPA==EKartEPAStatus::Converged && R.bGeometryValid);
        if(!R.bGeometryValid){AddInfo(FString::Printf(TEXT("%s GJK=%d EPA=%d iter=%d/%d"),Name,int(R.GJK),int(R.EPA),R.GJKIterations,R.EPAIterations));return R;}
        TestTrue(TEXT("Analytical depth"),FMath::Abs(R.Depth-Expected)<=Tolerance);
        TestTrue(TEXT("Unit normal and finite"),Finite(R) && FMath::Abs(R.Normal.Size()-1)<1.e-8);
        TestTrue(TEXT("Witness relation"),(R.PointOnA-R.PointOnB).Equals(R.Normal*R.Depth,Tolerance));
        const auto Again=KartNarrowphase::Query(X,Y);
        TestTrue(TEXT("Repeat deterministic"),R.Normal==Again.Normal && R.Depth==Again.Depth && R.EPAIterations==Again.EPAIterations);
        return R;
    };
    TestEqual(TEXT("Separated boxes"),KartNarrowphase::Query(A,Box(FVector(25,0,0))).GJK,EKartGJKStatus::Separated);
    TestEqual(TEXT("Touching boxes"),KartNarrowphase::Query(A,Box(FVector(20,0,0))).GJK,EKartGJKStatus::Touching);
    const auto R=Check(TEXT("Overlap boxes"),A,Box(FVector(15,0,0)),5,.001);
    const auto SwapR=Check(TEXT("Swapped boxes"),Box(FVector(15,0,0)),A,5,.001);
    TestTrue(TEXT("Swap reverses normal"),R.Normal.Equals(-SwapR.Normal,1.e-6));
    Check(TEXT("Deep/coincident boxes"),A,A,20,.001);
    Check(TEXT("Tiny overlap"),A,Box(FVector(19.9999,0,0)),.0001,.00001);
    Check(TEXT("Hull box"),H,Box(FVector(15,0,0)),5,.001);
    const auto O=Box(FVector(14,0,0),FVector(10),FQuat(FVector(0,0,1),.4));
    Check(TEXT("Rotated SAT overlap"),A,O,SAT(A,O),.002);
    TestEqual(TEXT("Rotated separated"),KartNarrowphase::Query(A,Box(FVector(40,0,0),FVector(10),O.Orientation)).GJK,EKartGJKStatus::Separated);
    Check(TEXT("Sphere sphere"),Sphere(FVector::ZeroVector),Sphere(FVector(15,0,0)),5,.003);
    Check(TEXT("Sphere box"),A,Sphere(FVector(15,0,0)),5,.003);
    TestEqual(TEXT("Separated spheres"),KartNarrowphase::Query(Sphere(FVector::ZeroVector),Sphere(FVector(21,0,0))).GJK,EKartGJKStatus::Separated);
    TestEqual(TEXT("Tangential spheres"),KartNarrowphase::Query(Sphere(FVector::ZeroVector),Sphere(FVector(20,0,0))).GJK,EKartGJKStatus::Touching);
    TestEqual(TEXT("Box sphere separation"),KartNarrowphase::Query(A,Sphere(FVector(25,0,0))).GJK,EKartGJKStatus::Separated);
    FKartNarrowphaseSettings Limited;Limited.GJKIterations=0;
    TestEqual(TEXT("GJK explicit iteration limit"),KartNarrowphase::Query(A,A,Limited).GJK,EKartGJKStatus::IterationLimit);
    Limited=FKartNarrowphaseSettings();Limited.EPAIterations=0;
    TestEqual(TEXT("EPA explicit iteration limit"),KartNarrowphase::Query(A,A,Limited).EPA,EKartEPAStatus::IterationLimit);
    TestEqual(TEXT("Invalid EPA seed"),KartNarrowphase::EPA(A,A,FKartGJKResult()).EPA,EKartEPAStatus::InvalidSeed);
    const auto Sym=KartNarrowphase::Query(Sphere(FVector::ZeroVector),Sphere(FVector::ZeroVector));
    TestTrue(TEXT("Symmetric spheres honest bounded result"),Sym.GJK==EKartGJKStatus::Intersecting && Finite(Sym)
        && (Sym.EPA==EKartEPAStatus::Converged || (Sym.EPA==EKartEPAStatus::IterationLimit && !Sym.bGeometryValid)));

    FRandomStream Random(381028);int32 Hits=0,Failures=0,GIter=0,EIter=0,Nonfinite=0;double MaxError=0;
    for(int32 I=0;I<180;++I)
    {
        const double Scale=I%3==0?.01:1.;const FVector Offset=I%7==0?FVector(1.e7,-1.e7,1.e7):FVector::ZeroVector;
        auto X=Box(Offset,FVector(8,10,12)*Scale,FQuat(FVector(1,2,3).GetSafeNormal(),Random.FRandRange(-2,2)));
        auto Y=Box(Offset+FVector(Random.FRandRange(0,35),Random.FRandRange(-12,12),Random.FRandRange(-10,10))*Scale,
            FVector(9,11,8)*Scale,FQuat(FVector(3,1,2).GetSafeNormal(),Random.FRandRange(-2,2)));
        const double Expected=SAT(X,Y);
        if(I%6==0) // Same OBB independently represented by a vertex hull.
        {
            X.Kind=EKartConvexKind::Hull;
            for(double SX:{-1.,1.})for(double SY:{-1.,1.})for(double SZ:{-1.,1.})X.Vertices.Add(X.HalfExtents*FVector(SX,SY,SZ));
        }
        const auto Q=KartNarrowphase::Query(X,Y);GIter+=Q.GJKIterations;EIter+=Q.EPAIterations;
        if(!Finite(Q))++Nonfinite;
        if(Expected>1.e-5*Scale)
        {
            ++Hits;if(!Q.bGeometryValid)++Failures;
            TestTrue(TEXT("Random SAT overlap converges"),Q.GJK==EKartGJKStatus::Intersecting && Q.bGeometryValid);
            if(Q.bGeometryValid){MaxError=FMath::Max(MaxError,FMath::Abs(Q.Depth-Expected));TestTrue(TEXT("Random SAT depth"),FMath::Abs(Q.Depth-Expected)<.005*Scale+1.e-7);}
        }
        else if(Expected<-1.e-5*Scale)TestEqual(TEXT("Random SAT separation"),Q.GJK,EKartGJKStatus::Separated);
    }
    AddInfo(FString::Printf(TEXT("STAGE3_STRESS seed=381028 cases=180 intersections=%d failed=%d nonfinite=%d gjk_iterations=%d epa_iterations=%d max_depth_error_cm=%.9g symmetric_sphere_epa=%d"),Hits,Failures,Nonfinite,GIter,EIter,MaxError,int(Sym.EPA)));
    TestEqual(TEXT("Stress finite"),Nonfinite,0);

    int32 CurvedHits=0,CurvedFailures=0;double CurvedError=0;
    for(int32 I=0;I<120;++I)
    {
        const double Scale=I%4==0?.01:1.;
        const FVector Direction=FVector(Random.FRandRange(.2,1),Random.FRandRange(-1,1),Random.FRandRange(-1,1)).GetSafeNormal();
        const double Distance=I%3==0?19.999:Random.FRandRange(8,28);
        auto X=Sphere(FVector::ZeroVector,10*Scale),Y=Sphere(Direction*Distance*Scale,10*Scale);
        double Expected=(20-Distance)*Scale;
        if(I%2==0){X=Box(FVector::ZeroVector,FVector(10)*Scale);Y.Position=FVector(Distance,2,3)*Scale;Expected=(20-Distance)*Scale;}
        const auto Q=KartNarrowphase::Query(X,Y);
        TestTrue(TEXT("Curved finite"),Finite(Q));
        if(Expected>0)
        {
            ++CurvedHits;if(!Q.bGeometryValid)++CurvedFailures;
            TestTrue(TEXT("Curved analytical overlap converges"),Q.GJK==EKartGJKStatus::Intersecting && Q.bGeometryValid);
            if(Q.bGeometryValid)
            {
                CurvedError=FMath::Max(CurvedError,FMath::Abs(Q.Depth-Expected));
                TestTrue(TEXT("Curved analytical depth"),FMath::Abs(Q.Depth-Expected)<.004*Scale+1.e-7);
                const FVector Local=Y.Orientation.UnrotateVector(Q.PointOnB-Y.Position);
                TestTrue(TEXT("Sphere witness inside surface envelope"),Local.Size()<=Y.Radius+1.e-6 && Local.Size()>=Y.Radius-.01*Scale);
            }
        }
        else TestEqual(TEXT("Curved analytical separation"),Q.GJK,EKartGJKStatus::Separated);
    }
    AddInfo(FString::Printf(TEXT("STAGE3_CURVED cases=120 hits=%d failures=%d max_depth_error_cm=%.9g"),CurvedHits,CurvedFailures,CurvedError));
    const FVector TangentDir=FVector(1,2,3).GetSafeNormal();
    TestEqual(TEXT("Oblique sphere tangency"),KartNarrowphase::Query(Sphere(FVector::ZeroVector),Sphere(TangentDir*20)).GJK,EKartGJKStatus::Touching);
    const auto NearParallel=Box(FVector(19,0,0),FVector(10),FQuat(FVector(0,0,1),1.e-8));
    Check(TEXT("Nearly parallel faces"),A,NearParallel,SAT(A,NearParallel),.002);
    Invalid=H;for(auto& V:Invalid.Vertices)V=FVector(1);TestFalse(TEXT("Repeated hull vertices rejected"),Invalid.IsValid());
    Invalid=H;Invalid.Vertices.SetNum(FKartConvexShape::MaxVertices+1);TestFalse(TEXT("Hull capacity enforced"),Invalid.IsValid());
    auto BadSeed=KartNarrowphase::GJK(A,A);BadSeed.Simplex[1]=BadSeed.Simplex[0];
    TestEqual(TEXT("Repeated seed vertices explicit failure"),KartNarrowphase::EPA(A,A,BadSeed).EPA,EKartEPAStatus::InvalidSeed);
    TestEqual(TEXT("Seed from different pair rejected"),KartNarrowphase::EPA(A,Box(FVector(3,0,0)),KartNarrowphase::GJK(A,A)).EPA,EKartEPAStatus::InvalidSeed);
    const auto SphereForward=KartNarrowphase::Query(Sphere(FVector::ZeroVector),Sphere(FVector(15,0,0)));
    const auto SphereBackward=KartNarrowphase::Query(Sphere(FVector(15,0,0)),Sphere(FVector::ZeroVector));
    TestTrue(TEXT("Sphere swap depth and normal"),SphereForward.bGeometryValid && SphereBackward.bGeometryValid
        && FMath::Abs(SphereForward.Depth-SphereBackward.Depth)<.003 && SphereForward.Normal.Equals(-SphereBackward.Normal,.01));

    FKartRigidBodyState Body;Body.SetPose(FVector(0,0,0),FQuat::Identity);Body.SetVelocities(FVector(2,3,4),FVector(1,0,0));Body.AddForce(FVector(5));Body.AddTorque(FVector(6));
    const auto Before=Body;auto FromBody=A;FromBody.Position=Body.GetPosition();FromBody.Orientation=Body.GetOrientation();
    FKartPhysicsBroadphase Tree;TMap<FKartPhysicsBodyId,FKartConvexShape> Shapes;
    for(uint64 I=1;I<=3;++I){auto Shape=FromBody;Shape.Position.X=(I-1)*5;FKartPhysicsProxy Proxy;Proxy.Id={I};Shape.Bounds(Proxy.Bounds);Tree.RegisterProxy(Proxy);Shapes.Add(Proxy.Id,Shape);}
    TArray<FKartPhysicsPair> Pairs;Tree.GeneratePairs(Pairs);const auto BeforeTree=Tree.GetDiagnostics();FKartPhysicsProxy ProxyBefore;Tree.GetProxy({1},ProxyBefore);
    TArray<FKartContactGeometry> Results;KartNarrowphase::Dispatch(Tree,Pairs,Shapes,Results);TestEqual(TEXT("Three pairs dispatched"),Results.Num(),3);
    for(int32 I=0;I<Results.Num();++I)TestTrue(TEXT("Sorted valid dispatch"),Results[I].Pair==Pairs[I] && Results[I].bGeometryValid);
    const auto DuplicatePair=Pairs[0];Pairs.Add(DuplicatePair);KartNarrowphase::Dispatch(Tree,Pairs,Shapes,Results);TestEqual(TEXT("Duplicate dispatch removed"),Results.Num(),3);
    FKartPhysicsProxy ProxyAfter;Tree.GetProxy({1},ProxyAfter);
    TestTrue(TEXT("Bodies unchanged"),Before.GetPosition()==Body.GetPosition() && Before.GetOrientation()==Body.GetOrientation() && Before.GetLinearVelocity()==Body.GetLinearVelocity()
        && Before.GetAngularVelocity()==Body.GetAngularVelocity() && Before.GetForce()==Body.GetForce() && Before.GetTorque()==Body.GetTorque() && Before.GetMass()==Body.GetMass());
    TestTrue(TEXT("Tree unchanged"),Tree.Validate() && Tree.GetDiagnostics().Reinserts==BeforeTree.Reinserts && Tree.GetDiagnostics().PairNodeVisits==BeforeTree.PairNodeVisits
        && ProxyBefore.Bounds.Min==ProxyAfter.Bounds.Min && ProxyBefore.Bounds.Max==ProxyAfter.Bounds.Max);
    Shapes.FindChecked({1}).Position.X+=1;KartNarrowphase::Dispatch(Tree,Pairs,Shapes,Results);TestEqual(TEXT("Stale shape bounds rejected"),Results[0].GJK,EKartGJKStatus::InvalidInput);
    Shapes.Remove({1});KartNarrowphase::Dispatch(Tree,Pairs,Shapes,Results);TestEqual(TEXT("Missing shape reported"),Results[0].GJK,EKartGJKStatus::InvalidInput);
    Tree.RemoveProxy({1});Tree.SetFilter({2},EKartProxyMobility::Dynamic,1,0xffffffff,false);
    KartNarrowphase::Dispatch(Tree,Pairs,Shapes,Results);TestEqual(TEXT("Removed and disabled skipped"),Results.Num(),0);
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Kart=World->SpawnActor<AKartPawn>(FVector(0,0,1000),FRotator::ZeroRotator);
    const auto Pose=Kart->GetActorTransform();KartNarrowphase::Query(A,O);
    TestTrue(TEXT("Live pawn unaffected"),Pose.Equals(Kart->GetActorTransform()) && Kart->GetCollisionCount()==0);
    TestFalse(TEXT("Custom rejected"),Kart->TrySelectPhysicsBackend(EKartPhysicsBackend::Custom));
    TestEqual(TEXT("Legacy only"),Kart->GetPhysicsBackend(),EKartPhysicsBackend::Legacy);Kart->Destroy();return true;
}
#endif
