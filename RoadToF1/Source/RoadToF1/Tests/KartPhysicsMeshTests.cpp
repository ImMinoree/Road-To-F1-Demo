#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "KartStaticMeshGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "KartMeshSceneMotion.h"
#include "KartCustomSceneBridge.h"
#include "GameFramework/PlayerStart.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartTriangleGeometryTest,"RoadToF1.Physics.Stage9TriangleGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartTriangleGeometryTest::RunTest(const FString&)
{
    for(double Angle:{0.,1.e-8,1.e-6,.5,1.2,UE_DOUBLE_PI/2})
    {
        const FQuat Q(FVector(1,2,3).GetSafeNormal(),Angle);const FVector Origin(123,456,789);
        FKartStaticTriangle T;T.V[0]=Origin+Q.RotateVector(FVector(0,-200,-200));T.V[1]=Origin+Q.RotateVector(FVector(0,200,-200));T.V[2]=Origin+Q.RotateVector(FVector(0,0,200));
        FKartConvexShape B;B.HalfExtents=FVector(50,40,10);B.Position=Origin+Q.RotateVector(FVector(-49.9,0,0));B.Orientation=Q;
        FVector N;TestTrue(TEXT("Analytical triangle depth"),FMath::Abs(FKartStaticMeshGeometry::Separation(B,T,N)+.1)<1.e-7);
        const auto M=FKartStaticMeshGeometry::Contact({{1},{2}},B,T);
        TestEqual(TEXT("Triangle contact valid"),M.Status,EKartManifoldStatus::Valid);
        TestTrue(TEXT("Normal points kart to triangle"),M.Normal.Equals(Q.RotateVector(FVector(1,0,0)),1.e-7));
        for(const auto& P:M.Points)
        {
            TestTrue(TEXT("Triangle witness on actual plane"),FMath::Abs(FVector::DotProduct(P.WorldB-T.V[0],M.Normal))<1.e-7);
            TestTrue(TEXT("Body witness on its surface"),FMath::Abs(P.LocalA.X-50)<1.e-7);
            TestTrue(TEXT("Analytical per-point depth"),FMath::Abs(P.Depth-.1)<1.e-7);
            TestTrue(TEXT("Witness convention"),(P.WorldA-P.WorldB).Equals(M.Normal*P.Depth,1.e-7));
        }
        B.Position=Origin+Q.RotateVector(FVector(-49.9,500,0));
        TestEqual(TEXT("Finite triangle is not an infinite wall"),FKartStaticMeshGeometry::Contact({{1},{2}},B,T).Status,EKartManifoldStatus::NoContact);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartTriangleCastTest,"RoadToF1.Physics.Stage9TriangleCastConvergence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartTriangleCastTest::RunTest(const FString&)
{
    // Captured SouthGarda first-bend tilted chassis / finite asphalt triangle.
    FKartConvexShape A;A.HalfExtents=FVector(98,63,20);A.Position=FVector(12689.876,-3857.655,23.509);
    A.Orientation=FQuat(-.011367610,.004984405,.848681883,.528757974).GetNormalized();
    auto B=A;B.Position=FVector(12692.974,-3853.606,23.408);B.Orientation=FQuat(-.012932976,.004300342,.850151542,.526361664).GetNormalized();
    FKartStaticTriangle T;T.V[0]=FVector(12694.323,-3809.228,2.5);T.V[1]=FVector(13224.304,-4536.634,2.5);T.V[2]=FVector(13322.682,-4462.509,2.5);
    double Time;bool Hit;FString Failure;const bool Ok=KartMeshMotion::CastTriangle(A,B,T,Time,Hit,&Failure);
    TestTrue(FString(TEXT("Tilted mesh cast converges: "))+Failure,Ok);if(!Ok)return false;
    TestTrue(TEXT("Actual finite asphalt contact detected"),Hit);
    FVector N;auto At=A;At.Position=FMath::Lerp(A.Position,B.Position,Time);At.Orientation=FQuat::Slerp(A.Orientation,B.Orientation,Time);
    const double Gap=FKartStaticMeshGeometry::Separation(At,T,N);
    TestTrue(TEXT("Conservative hit remains outside physical triangle"),Gap>=0 && Gap<=.0100002);
    double Oracle=1;for(int I=0;I<=10000;++I){const double F=I/10000.;auto P=A;P.Position=FMath::Lerp(A.Position,B.Position,F);P.Orientation=FQuat::Slerp(A.Orientation,B.Orientation,F);if(FKartStaticMeshGeometry::Separation(P,T,N)<=.01){Oracle=F;break;}}
    TestTrue(TEXT("Cast is no later than independently sampled contact"),Time<=Oracle+1.e-7);
    AddInfo(FString::Printf(TEXT("CAPTURED_TRIANGLE time%.12g gap%.12g oracle%.12g"),Time,Gap,Oracle));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartTriangleSweepMatrixTest,"RoadToF1.Physics.Stage9TriangleSweepMatrix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartTriangleSweepMatrixTest::RunTest(const FString&)
{
    for(double Angle:{0.,.5,UE_DOUBLE_PI/2})for(double Travel:{100.,300.,3000.})for(double Spin:{0.,.01,.1})
    {
        const FQuat Q(FVector(1,2,3).GetSafeNormal(),Angle);const FVector O(123,456,789);
        FKartStaticTriangle T;T.V[0]=O+Q.RotateVector(FVector(0,-2000,-2000));T.V[1]=O+Q.RotateVector(FVector(0,2000,-2000));T.V[2]=O+Q.RotateVector(FVector(0,0,2000));
        FKartConvexShape A;A.HalfExtents=FVector(50,40,10);A.Position=O+Q.RotateVector(FVector(-60,0,0));A.Orientation=Q;
        auto B=A;B.Position+=Q.RotateVector(FVector(Travel,7,0));B.Orientation=Q*FQuat(FVector(1,2,1).GetSafeNormal(),Spin);
        double Time;bool Hit;FString Why;
        if(!TestTrue(FString(TEXT("Rotated finite triangle CCD: "))+Why,KartMeshMotion::CastTriangle(A,B,T,Time,Hit,&Why)) || !TestTrue(TEXT("Fast crossing detected"),Hit))return false;
        auto P=A;P.Position=FMath::Lerp(A.Position,B.Position,Time);P.Orientation=FQuat::Slerp(A.Orientation,B.Orientation,Time);FVector N;
        const double Gap=FKartStaticMeshGeometry::Separation(P,T,N);
        if(!TestTrue(TEXT("TOI outside actual finite surface"),Gap>=0 && Gap<=.0100003))return false;
        // Finite mesh bounds are never substituted by an infinite collision plane.
        A.Position+=Q.RotateVector(FVector(0,5000,0));B.Position+=Q.RotateVector(FVector(0,5000,0));
        if(!TestTrue(TEXT("Finite sweep completes"),KartMeshMotion::CastTriangle(A,B,T,Time,Hit,&Why)) || !TestFalse(TEXT("Outside finite triangle is clear"),Hit))return false;
    }
    FKartStaticTriangle Floor;Floor.V[0]=FVector(-2000,-2000,0);Floor.V[1]=FVector(2000,-2000,0);Floor.V[2]=FVector(0,2000,0);
    FKartConvexShape A;A.HalfExtents=FVector(98,63,20);A.Position=FVector(0,0,20.05);auto B=A;B.Position.X=20;B.Orientation=FQuat(FVector::UpVector,.02);
    double Time;bool Hit;FString Why;TestTrue(TEXT("Parallel yawing floor sweep converges"),KartMeshMotion::CastTriangle(A,B,Floor,Time,Hit,&Why));TestFalse(TEXT("Parallel floor motion has no false impact"),Hit);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartCookedMeshTest,"RoadToF1.Physics.Stage9CookedMeshCoverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartCookedMeshTest::RunTest(const FString&)
{
    if(!TestTrue(TEXT("Load actual SouthGarda map"),FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true)))return false;
    auto* W=GEditor->GetEditorWorldContext().World();
    int PhysicalMeshes=0;
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components)if(C->IsQueryCollisionEnabled() && C->GetCollisionObjectType()==ECC_WorldStatic && C->GetStaticMesh())
        {
            ++PhysicalMeshes;
            auto* B=C->GetStaticMesh()->GetBodySetup();int Count=0;if(B){B->CreatePhysicsMeshes();for(const auto& M:B->TriMeshGeometries)if(M)Count+=M->Elements().GetNumTriangles();}
            AddInfo(FString::Printf(TEXT("STATIC %s asset%s trace%d simple%d cookedTriangles%d transform%s"),*C->GetOwner()->GetActorLabel(),*C->GetStaticMesh()->GetPathName(),B?int(B->GetCollisionTraceFlag()):-1,B?B->AggGeom.GetElementCount():-1,Count,*C->GetComponentTransform().ToString()));
        }
    }
    FKartStaticMeshGeometry Scene;FString Failure;const bool Ok=Scene.Build(*W,Failure);
    TestTrue(FString(TEXT("Exact cooked geometry import: "))+Failure,Ok);
    if(!Ok)return false;
    TestEqual(TEXT("Every physical static mesh represented; builder brush excluded"),Scene.GetSources().Num(),PhysicalMeshes);
    TestTrue(TEXT("Immutable snapshot valid"),Scene.IsCurrent());
    for(const auto& S:Scene.GetSources())TestTrue(TEXT("No empty collision source"),S.Triangles>0);
    AddInfo(FString::Printf(TEXT("COOKED_SNAPSHOT sources%d triangles%d BVHheight%d"),Scene.GetSources().Num(),Scene.GetTriangles().Num(),Scene.GetHeight()));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKartMeshMotionTest,"RoadToF1.Physics.Stage9ActualMeshMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKartMeshMotionTest::RunTest(const FString&)
{
    if(!TestTrue(TEXT("Actual map loaded"),FEditorFileUtils::LoadMap(TEXT("/Game/RoadToF1/SouthGarda_KartRace"),false,true)))return false;
    auto* W=GEditor->GetEditorWorldContext().World();FKartStaticMeshGeometry Scene;FString Failure;
    if(!TestTrue(TEXT("Actual collision snapshot complete"),Scene.Build(*W,Failure))){AddError(Failure);return false;}
    // Verify imported world geometry against independent Unreal component ray queries.
    int Verified=0;
    for(int Source=0;Source<Scene.GetSources().Num();++Source)
    {
        auto* C=Scene.GetSources()[Source].Component.Get();bool Match=false;int Attempts=0;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(KartMeshOracle),true);
        for(const auto& T:Scene.GetTriangles())if(T.Source==Source && Attempts++<64)
        {
            const auto P=(T.V[0]+T.V[1]+T.V[2])/3.;const auto N=FVector::CrossProduct(T.V[1]-T.V[0],T.V[2]-T.V[0]).GetSafeNormal();
            for(double Sign:{-1.,1.}){FHitResult H;if(C->LineTraceComponent(H,P+N*Sign*2,P-N*Sign*2,Params) && (H.ImpactPoint-P).Size()<.05)Match=true;}
            if(Match)break;
        }
        TestTrue(FString(TEXT("Cooked triangle agrees with Unreal: "))+Scene.GetSources()[Source].Path,Match);Verified+=Match?1:0;
    }
    // Choose a real finite vertical wall with clear approach space (not fabricated bounds).
    int Barrier=INDEX_NONE;FVector Outward;FKartConvexShape Start;
    for(int Index=0;Index<Scene.GetTriangles().Num() && Barrier==INDEX_NONE;++Index)
    {
        const auto& T=Scene.GetTriangles()[Index];const auto Cross=FVector::CrossProduct(T.V[1]-T.V[0],T.V[2]-T.V[0]);const auto N=Cross.GetSafeNormal();
        if(FMath::Abs(N.Z)>.05 || Cross.Size()<10000)continue;
        const auto P=(T.V[0]+T.V[1]+T.V[2])/3.;
        for(double Sign:{-1.,1.})
        {
            Start.Position=P+N*Sign*130;Start.Orientation=(-N*Sign).Rotation().Quaternion();Start.HalfExtents=FVector(50,40,10);
            FKartPhysicsAABB B;Start.Bounds(B);TArray<int32> Near;Scene.Query(B,Near);bool Clear=true;
            for(int Other:Near){FVector Normal;if(Scene.Separation(Start,Scene.GetTriangles()[Other],Normal)<=.02){Clear=false;break;}}
            if(Clear){Barrier=Index;Outward=N*Sign;break;}
        }
    }
    if(!TestTrue(TEXT("Real wall with collision-free approach found"),Barrier!=INDEX_NONE))return false;
    const auto& T=Scene.GetTriangles()[Barrier];const auto PlanePoint=T.V[0];
    for(double Speed:{300.,1500.})
    {
        FKartExperimentalVehicle V;V.Reset({},Start.Position);V.Body.SetPose(Start.Position,Start.Orientation);V.Body.SetVelocities(-Outward*Speed,FVector::ZeroVector);
        bool Valid=true;double Closest=1.e9,Impulse=0,MaxDepth=0;
        for(int Step=0;Step<120;++Step)
        {
            FVector J=FVector::ZeroVector;FKartSceneStepStats Stats;const auto P=V.Body.GetPosition();const auto Q=V.Body.GetOrientation();
            if(!KartMeshMotion::Advance(Scene,V,P,Q,1./120,J,&Failure,&Stats,{})){Valid=false;AddInfo(FString::Printf(TEXT("Actual mesh impact stopped step%d %s"),Step,*Failure));break;}
            Impulse+=J.Size();Closest=FMath::Min(Closest,FVector::DotProduct(V.Body.GetPosition()-PlanePoint,Outward));MaxDepth=FMath::Max(MaxDepth,Stats.MaxPenetrationCm);
        }
        TestTrue(TEXT("Actual mesh motion valid"),Valid);TestTrue(TEXT("Real wall stops approach without tunneling"),Closest>=49.8 && Impulse>0);
        TestTrue(TEXT("Mesh penetration below 2mm"),MaxDepth<.2);
        AddInfo(FString::Printf(TEXT("ACTUAL_WALL source%s triangle%d speed%.0f closest%.9g depth%.9g impulse%.9g"),*Scene.GetSources()[T.Source].Path,Barrier,Speed,Closest,MaxDepth,Impulse));
    }
    APlayerStart* Grid=nullptr;for(TActorIterator<APlayerStart> It(W);It;++It){Grid=*It;break;}
    if(!TestNotNull(TEXT("Actual grid player start"),Grid))return false;
    FKartExperimentalVehicle Kart;Kart.Reset({},Grid->GetActorLocation());Kart.Body.SetPose(Grid->GetActorLocation(),Grid->GetActorQuat());
    bool Valid=true;double Peak=0;
    for(int Step=0;Step<360;++Step)
    {
        FVector J;FKartSceneStepStats Stats;
        const FKartVehicleInput Input{Step<180?1.:Step>=300?-1.:0.,0,Step>=180 && Step<300?1.:0.};
        if(!FKartCustomSceneBridge::Step(*W,nullptr,Kart,Input,1./120,J,&Failure,&Stats,{},&Scene)){Valid=false;AddInfo(Failure);break;}
        Peak=FMath::Max(Peak,Kart.Body.GetLinearVelocity().Size()*.036);
    }
    TestTrue(TEXT("Actual grid acceleration/brake/reverse without scene fault"),Valid);TestTrue(TEXT("Actual ground supports usable movement"),Peak>8 && Kart.Body.GetPosition().Z>-100);
    AddInfo(FString::Printf(TEXT("ACTUAL_GRID peakKmh%.6g final%s velocity%s oracleSources%d"),Peak,*Kart.Body.GetPosition().ToString(),*Kart.Body.GetLinearVelocity().ToString(),Verified));
    return true;
}
#endif
