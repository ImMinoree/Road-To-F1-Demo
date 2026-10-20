#include "KartPawn.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Components/TextRenderComponent.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/PlayerController.h"
#include "KartRaceDirector.h"
#include "KartTrainingRecorder.h"
#include "KartPhysicsWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

AKartPawn::AKartPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("KartCollision"));
    SetRootComponent(Body);
    Body->SetBoxExtent(FVector(98, 63, 20));
    Body->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    const TCHAR* Names[] = { TEXT("Chassis"), TEXT("Aluminium"), TEXT("Rubber"), TEXT("Fairing"), TEXT("Engine"), TEXT("White"), TEXT("Carbon"), TEXT("Chain") };
    for (const TCHAR* Name : Names)
    {
        UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Art_%s"), Name));
        Piece->SetupAttachment(Body);
        Piece->SetRelativeLocation(FVector(0, 0, -22));
        // Imported kart points along -Y; align it with the pawn's +X forward axis.
        Piece->SetRelativeRotation(FRotator(0, 90, 0));
        Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(*FString::Printf(TEXT("/Game/RoadToF1/Art/VisualV01/Meshes/Kart_%s.Kart_%s"), Name, Name));
        if (Asset.Succeeded()) Piece->SetStaticMesh(Asset.Object);
        Art.Add(Piece);
    }
    Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("FollowBoom"));
    Boom->SetupAttachment(Body);
    Boom->TargetArmLength = 430;
    Boom->SetRelativeLocation(FVector(0, 0, 85));
    Boom->SetRelativeRotation(FRotator(-15, 0, 0));
    Boom->bEnableCameraLag = true;
    Boom->CameraLagSpeed = 8;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    Camera->SetupAttachment(Boom);
    Camera->FieldOfView = 80;
    EyeCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DriverEyeCamera"));
    EyeCamera->SetupAttachment(Body);
    EyeCamera->SetRelativeLocation(ViewSettings.EyePosition);
    EyeCamera->SetRelativeRotation(FRotator(ViewSettings.EyePitch,0,0));
    EyeCamera->FieldOfView = ViewSettings.LowSpeedFOV;
    Camera->SetAutoActivate(false);
    WheelMount=CreateDefaultSubobject<USceneComponent>(TEXT("SteeringWheelMount"));
    WheelMount->SetupAttachment(Body);
    WheelMount->SetRelativeLocation(FVector(24,0,36));
    WheelMount->SetRelativeRotation(FRotator(-40,0,0));
    WheelSpin=CreateDefaultSubobject<USceneComponent>(TEXT("SteeringWheelSpin"));
    WheelSpin->SetupAttachment(WheelMount);
    for(const TCHAR* Name:{TEXT("Rubber"),TEXT("Aluminium"),TEXT("Engine")})
    {
        auto* Piece=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel_%s"),Name));
        Piece->SetupAttachment(WheelSpin);Piece->SetRelativeRotation(FRotator(0,90,0));
        Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*FString::Printf(TEXT("/Game/RoadToF1/Art/VisualV01/Meshes/Wheel_%s.Wheel_%s"),Name,Name));
        if(Mesh.Succeeded())Piece->SetStaticMesh(Mesh.Object);
        WheelArt.Add(Piece);
    }
    const TCHAR* DriverNames[] = {TEXT("Suit"), TEXT("SuitAccent"), TEXT("Helmet"), TEXT("Visor"), TEXT("Boots"), TEXT("Gloves")};
    for (const TCHAR* Name : DriverNames)
    {
        UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Driver_%s"), Name));
        Piece->SetupAttachment(Body);
        Piece->SetRelativeLocation(FVector(0, 0, -22));
        Piece->SetRelativeRotation(FRotator(0, 90, 0));
        Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DriverArt.Add(Piece);
    }
    ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for (int32 Index = 0; Index < 6; ++Index)
    {
        auto AddEffect = [&](const FString& Name, UStaticMesh* Mesh, auto& Array)
        {
            UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
            Piece->SetupAttachment(Body);
            Piece->SetStaticMesh(Mesh);
            Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Piece->SetCastShadow(false);
            Piece->SetVisibility(false);
            Array.Add(Piece);
        };
        AddEffect(FString::Printf(TEXT("Flame%d"), Index), Cone.Object, Flames);
        AddEffect(FString::Printf(TEXT("Smoke%d"), Index), Sphere.Object, Smoke);
    }
    EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
    EngineAudio->SetupAttachment(Body);
    EngineAudio->bAutoActivate = false;
    EngineAudio->bOverrideAttenuation = true;
    EngineAudio->AttenuationOverrides.bAttenuate = true;
    EngineAudio->AttenuationOverrides.bSpatialize = true;
    EngineAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(100);
    EngineAudio->AttenuationOverrides.FalloffDistance = 2200;
    NumberBadge = CreateDefaultSubobject<USceneComponent>(TEXT("NumberBadge"));
    NumberBadge->SetupAttachment(Body);
    NumberBadge->SetRelativeLocation(FVector(-10, 0, 110));
    NumberShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NumberShell"));
    NumberShell->SetupAttachment(NumberBadge);
    NumberShell->SetStaticMesh(Sphere.Object);
    NumberShell->SetRelativeScale3D(FVector(.28));
    NumberShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NumberShell->SetCastShadow(false);
    NumberFace = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NumberFace"));
    NumberFace->SetupAttachment(NumberBadge);
    NumberFace->SetStaticMesh(Sphere.Object);
    NumberFace->SetRelativeLocation(FVector(13.8, 0, 0));
    NumberFace->SetRelativeScale3D(FVector(.012, .20, .20));
    NumberFace->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NumberFace->SetCastShadow(false);
    RacerNumber = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RacerNumber"));
    RacerNumber->SetupAttachment(NumberBadge);
    RacerNumber->SetRelativeLocation(FVector(14.5, 0, 0));
    RacerNumber->SetHorizontalAlignment(EHTA_Center);
    RacerNumber->SetVerticalAlignment(EVRTA_TextCenter);
    RacerNumber->SetWorldSize(12);
    RacerNumber->SetCastShadow(false);
    RacerNumber->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IncidentLight"));
    FireLight->SetupAttachment(Body);
    FireLight->SetRelativeLocation(FVector(-45, 35, 50));
    FireLight->SetLightColor(FLinearColor(1.f, .18f, .01f));
    FireLight->SetAttenuationRadius(250);
    FireLight->SetCastShadows(false);
    FireLight->SetVisibility(false);
}

void AKartPawn::BeginPlay()
{
    Super::BeginPlay();
    for (UStaticMeshComponent* Piece : DriverArt)
    {
        const FString Name = Piece->GetName();
        Piece->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/RoadToF1/Art/DriversV01/Meshes/%s.%s"), *Name, *Name)));
    }
    ConfigureRacer(0, false);
    EngineWave = NewObject<USoundWaveProcedural>(this);
    EngineWave->SetSampleRate(FKartEngineSound::SampleRate);
    EngineWave->NumChannels = 1;
    EngineWave->Duration = INDEFINITELY_LOOPING_DURATION;
    EngineAudio->SetSound(EngineWave);
    UpdateAudioAndNumber(0);
    EngineAudio->Play();
    UMaterialInterface* FlameMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RoadToF1/Art/IncidentV01/M_Flame.M_Flame"));
    UMaterialInterface* SmokeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RoadToF1/Art/IncidentV01/M_Smoke.M_Smoke"));
    for (UStaticMeshComponent* Piece : Flames) if (FlameMat) Piece->SetMaterial(0, FlameMat);
    for (UStaticMeshComponent* Piece : Smoke) if (SmokeMat) Piece->SetMaterial(0, SmokeMat);
}

void AKartPawn::ConfigureRacer(int32 Index, bool bOpponent)
{
    bOpponentRacer=bOpponent;
    SpeedLimitKmh = bOpponent ? 55 : 57;
    static const TCHAR* Names[] = {TEXT("YOU"), TEXT("Luca Rossi"), TEXT("Noah Weber"), TEXT("Maya Patel"), TEXT("Leo Martin"), TEXT("Eva Novak"), TEXT("Kai Tanaka"), TEXT("Zara Khan"), TEXT("Finn Walsh"), TEXT("Mila Costa"), TEXT("Theo Laurent"), TEXT("Aria Singh"), TEXT("Hugo Silva"), TEXT("Nina Berg"), TEXT("Owen Clarke"), TEXT("Sara Malik"), TEXT("Alex Chen"), TEXT("Isla Reed"), TEXT("Enzo Romano"), TEXT("Freya Olsen")};
    DriverName = Names[FMath::Clamp(Index, 0, 19)];
    KartNumber = bOpponent ? Index : 20;
    RacerNumber->SetText(FText::FromString(FString::Printf(TEXT("%d"), KartNumber)));
    RacerNumber->SetTextRenderColor(FColor::Black);
    EngineAudio->SetVolumeMultiplier(bOpponent ? .045f : .14f);
    SuitColor = FLinearColor::MakeFromHSV8((Index * 97) % 256, 210, 235);
    const FLinearColor Accent = FLinearColor::MakeFromHSV8((Index * 97 + 100) % 256, Index % 2 ? 180 : 30, 255);
    for (UStaticMeshComponent* Piece : DriverArt)
        for (int32 Slot = 0; Slot < Piece->GetNumMaterials(); ++Slot)
            if (UMaterialInstanceDynamic* Mat = Piece->CreateAndSetMaterialInstanceDynamic(Slot))
                Mat->SetVectorParameterValue(TEXT("LiveryColor"), Piece->GetName() == TEXT("Driver_Suit") ? SuitColor : Accent);
    if (!DriverArt.IsEmpty() && DriverArt[0]->GetNumMaterials() > 0) NumberShell->SetMaterial(0, DriverArt[0]->GetMaterial(0));
    if (bOpponent)
    {
        Boom->SetComponentTickEnabled(false);
        Camera->Deactivate();
    }
    ApplyCameraMode();
}

int32 AKartPawn::GetDriverMeshCount() const
{
    int32 Count = 0;
    for (const UStaticMeshComponent* Piece : DriverArt) if (Piece->GetStaticMesh()) ++Count;
    return Count;
}

void AKartPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    const auto BindHeldKey = [Input](FKey Key, float* Value)
    {
        FInputKeyBinding Press(FInputChord(Key), IE_Pressed);
        Press.KeyDelegate.GetDelegateForManualSet().BindLambda([Value]() { *Value = 1; });
        Input->KeyBindings.Add(Press);
        FInputKeyBinding Release(FInputChord(Key), IE_Released);
        Release.KeyDelegate.GetDelegateForManualSet().BindLambda([Value]() { *Value = 0; });
        Input->KeyBindings.Add(Release);
    };
    BindHeldKey(EKeys::W, &Forward);
    BindHeldKey(EKeys::S, &Reverse);
    BindHeldKey(EKeys::A, &Left);
    BindHeldKey(EKeys::D, &Right);
    Input->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AKartPawn::BrakePressed);
    Input->BindKey(EKeys::SpaceBar, IE_Released, this, &AKartPawn::BrakeReleased);
    Input->BindKey(EKeys::V, IE_Pressed, this, &AKartPawn::ToggleCamera);
    Input->BindKey(EKeys::F9, IE_Pressed, this, &AKartPawn::ToggleTrainingRecording);
#if !UE_BUILD_SHIPPING
    FInputKeyBinding PhysicsKey(FInputChord(EKeys::F10),IE_Pressed);
    PhysicsKey.KeyDelegate.GetDelegateForManualSet().BindLambda([this]()
    {
        auto* Physics=GetWorld()->GetSubsystem<UKartPhysicsWorldSubsystem>();
        bool Ok=false;
        if(Physics->IsFleetKart(*this))Ok=Physics->ReleaseFleet();
        else
        {
            TArray<AKartPawn*> Karts;for(TActorIterator<AKartPawn> It(GetWorld());It;++It)Karts.Add(*It);
            Ok=Physics->SelectFleet(Karts,true);
        }
        if(GEngine)GEngine->AddOnScreenDebugMessage(71010,6,Ok?FColor::Cyan:FColor::Yellow,
            Ok?(Physics->IsFleetKart(*this)?TEXT("CUSTOM physics active (experimental)"):TEXT("LEGACY physics active")):
            FString(TEXT("Physics switch rejected; restart with F5, then F10. "))+Physics->GetFleetFailure());
    });
    Input->KeyBindings.Add(PhysicsKey);
#endif
}

void AKartPawn::ToggleTrainingRecording()
{
    if (AKartRaceDirector* Director = Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AKartRaceDirector::StaticClass())))
        Director->GetTrainingRecorder()->ToggleCapture();
}

void AKartPawn::SetDriveInput(float Throttle, float Steering, bool Brake)
{
    if (bRaceFinished) { Throttle = 0; Steering = 0; Brake = false; }
    Forward = FMath::Max(0.f, FMath::Clamp(Throttle, -1.f, 1.f));
    Reverse = FMath::Max(0.f, -FMath::Clamp(Throttle, -1.f, 1.f));
    Right = FMath::Max(0.f, FMath::Clamp(Steering, -1.f, 1.f));
    Left = FMath::Max(0.f, -FMath::Clamp(Steering, -1.f, 1.f));
    bBrake = Brake;
}

void AKartPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateAudioAndNumber(DeltaSeconds);
    if(PhysicsBackend==EKartPhysicsBackend::Custom)
    {
        auto* Physics=GetWorld()->GetSubsystem<UKartPhysicsWorldSubsystem>();
        if(!Physics->IsFleetKart(*this))Physics->AdvanceTestKart(*this,DeltaSeconds);
        return; // Exclusive owner: never enter Legacy swept movement or physical incident response.
    }
    // Bounded substeps keep swept movement, braking and steering stable during hitches.
    bool ViewKerb=false;
    const bool SampleKerb=IsLocallyControlled() && bFirstPerson;
    float Remaining = FMath::Min(DeltaSeconds, .25f);
    while (Remaining > UE_SMALL_NUMBER)
    {
        const float Dt = FMath::Min(Remaining, 1.f / 120.f);
        Remaining -= Dt;
        UpdateIncident(Dt);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(KartGround), false, this);
        FHitResult Ground;
        const FVector Position = GetActorLocation();
        const FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
        bool bGrounded = GetWorld()->LineTraceSingleByObjectType(Ground, Position + FVector(0, 0, 50), Position - FVector(0, 0, 65), GroundObjects, Query)
            && Ground.Normal.Z > .55f && Position.Z - Ground.ImpactPoint.Z < 55;
        // Support the whole footprint, including its next movement. A centre-only
        // trace lowered the box onto grass while its nose still touched asphalt,
        // making the 6.5cm road edge behave like a wall. Limit climb to 12cm so
        // this cannot lift the kart over actual buildings or barriers.
        float SupportHeight = bGrounded ? Ground.ImpactPoint.Z : Position.Z - 22;
        if (bGrounded)
        {
            const FVector Ahead = GetActorForwardVector() * Speed * Dt;
            for (float X : {-98.f, 0.f, 98.f}) for (float Y : {-63.f, 0.f, 63.f})
            {
                const FVector Probe = Position + Ahead + GetActorForwardVector() * X + GetActorRightVector() * Y;
                FHitResult Support;
                if (GetWorld()->LineTraceSingleByObjectType(Support, Probe + FVector(0, 0, 40), Probe - FVector(0, 0, 65), GroundObjects, Query)
                    && Support.Normal.Z > .55f && Support.ImpactPoint.Z <= Position.Z - 22 + 12)
                {
                    SupportHeight = FMath::Max(SupportHeight, Support.ImpactPoint.Z);
                    if(SampleKerb && Support.GetActor() && Support.GetActor()->ActorHasTag(TEXT("VisualKerb")))ViewKerb=true;
                }
            }
            const float Rise = SupportHeight + 22 - Position.Z;
            if (Rise > 0)
            {
                FHitResult Ceiling;
                AddActorWorldOffset(FVector(0, 0, Rise), true, &Ceiling);
            }
        }
        const bool bDisabled = FireSeconds > 0 || StunSeconds > 0;
        const float Throttle = (bDisabled || bRaceFinished) ? 0 : Forward - Reverse;
        if (bGrounded)
        {
            if (bRaceFinished) Speed = FMath::FInterpConstantTo(Speed, 0.f, Dt, 400.f);
            else if (bDisabled || bBrake || (Speed * Throttle < 0)) Speed = FMath::FInterpConstantTo(Speed, 0.f, Dt, 1500.f);
            else if (!FMath::IsNearlyZero(Throttle)) Speed += Throttle * 300.f * Dt;
            else Speed = FMath::FInterpConstantTo(Speed, 0.f, Dt, 180.f);
            Speed = FMath::Clamp(Speed, -400.f, SpeedLimitKmh / .036f);
            const float DesiredSteering = bRaceFinished ? 0 : Right - Left;
            SmoothedSteering = FMath::FInterpTo(SmoothedSteering, DesiredSteering, Dt, FMath::IsNearlyZero(DesiredSteering) ? 5.f : 4.f);
            const float SteeringAngle = FMath::DegreesToRadians(SmoothedSteering * 22.f / (1.f + FMath::Abs(Speed) / 1400.f));
            const float DesiredYawRate = FMath::Clamp(FMath::RadiansToDegrees(Speed / 104.f * FMath::Tan(SteeringAngle)), -55.f, 55.f);
            YawRate = FMath::FInterpTo(YawRate, DesiredYawRate, Dt, 5.f);
            AddActorWorldRotation(FRotator(0, YawRate * Dt, 0));
            VerticalSpeed = 0;
        }
        else VerticalSpeed -= 980.f * Dt;
        FVector Delta = GetActorForwardVector() * Speed * Dt;
        Delta.Z = bGrounded ? SupportHeight + 22 - GetActorLocation().Z : VerticalSpeed * Dt;
        FHitResult Hit;
        AddActorWorldOffset(Delta, true, &Hit);
        if (Hit.bBlockingHit)
        {
            if (Hit.Normal.Z < .55f)
            {
                if (AKartPawn* Other = Cast<AKartPawn>(Hit.GetActor()))
                {
                    const float Closing = FMath::Max(0., -FVector::DotProduct(GetVelocity() - Other->GetVelocity(), Hit.Normal));
                    if (Closing > 100)
                    {
                        ReceiveCollision(Closing, Hit.Normal);
                        Other->ReceiveCollision(Closing, -Hit.Normal);
                    }
                }
                Speed = 0;
            }
            else VerticalSpeed = 0;
        }
    }
    UpdateDrivingView(DeltaSeconds, ViewKerb);
}

FVector AKartPawn::GetCollisionHalfExtent() const { return Body->GetScaledBoxExtent(); }

void AKartPawn::SetRaceFinished()
{
    bRaceFinished = true;
    // Retain track collision, but a cooling-down finisher cannot block the racing field.
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    SetDriveInput(0, 0);
}

void AKartPawn::ResetKart()
{
    bRaceFinished = false;
    SetActorHiddenInGame(false);
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Speed = VerticalSpeed = SmoothedSteering = YawRate = 0;
    EngineSynth = FKartEngineSound();
    if (EngineWave) EngineWave->ResetAudio();
    FireSeconds = StunSeconds = CollisionCooldown = IncidentClock = 0;
    CollisionCount = 0;
    UpdateIncident(0);
    SetDriveInput(0, 0);
    ResetDrivingView();
    Boom->bEnableCameraLag = false;
    Boom->TickComponent(0, LEVELTICK_All, nullptr);
    Boom->bEnableCameraLag = true;
    if(PhysicsBackend==EKartPhysicsBackend::Custom)GetWorld()->GetSubsystem<UKartPhysicsWorldSubsystem>()->ResetTestKart(*this);
}

void AKartPawn::ReceiveCollision(float ImpactSpeed, const FVector& Normal)
{
    if (bRaceFinished || CollisionCooldown > 0 || ImpactSpeed < 100) return;
    CollisionCooldown = 1;
    ++CollisionCount;
    if(PhysicsBackend==EKartPhysicsBackend::Legacy)Speed *= .25f;
    StunSeconds = FMath::Clamp(ImpactSpeed / 1500.f, .15f, 1.f);
    const float Side = FVector::DotProduct(Normal, GetActorRightVector());
    if(PhysicsBackend==EKartPhysicsBackend::Legacy)AddActorWorldRotation(FRotator(0, FMath::Clamp(Side * ImpactSpeed / 70.f, -12.f, 12.f), 0));
    if (ImpactSpeed >= 700) FireSeconds = 6;
}

void AKartPawn::UpdateIncident(float Dt)
{
    FireSeconds = FMath::Max(0.f, FireSeconds - Dt);
    StunSeconds = FMath::Max(0.f, StunSeconds - Dt);
    CollisionCooldown = FMath::Max(0.f, CollisionCooldown - Dt);
    IncidentClock += Dt;
    const bool bFire = FireSeconds > 0;
    for (int32 Index = 0; Index < Flames.Num(); ++Index)
    {
        const float Pulse = .75f + .25f * FMath::Sin(IncidentClock * 19 + Index * 2.1f);
        Flames[Index]->SetVisibility(bFire);
        Flames[Index]->SetRelativeLocation(FVector(-45 + (Index % 3 - 1) * 12, 35 + (Index / 3 - .5f) * 14, 15 + Pulse * 18));
        Flames[Index]->SetRelativeScale3D(FVector(.11f * Pulse, .11f * Pulse, .35f * Pulse));
        const float Rise = FMath::Fmod(IncidentClock * .55f + Index / 6.f, 1.f);
        Smoke[Index]->SetVisibility(bFire);
        Smoke[Index]->SetRelativeLocation(FVector(-45 - Rise * 20, 35, 35 + Rise * 110));
        Smoke[Index]->SetRelativeScale3D(FVector(.08f + Rise * .4f));
    }
    FireLight->SetVisibility(bFire);
    if (bFire) FireLight->SetIntensity(1500 + 500 * FMath::Sin(IncidentClock * 17));
}

AKartGameMode::AKartGameMode()
{
    DefaultPawnClass = AKartPawn::StaticClass();
}

void AKartPawn::UpdateAudioAndNumber(float Dt)
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        FVector Eye; FRotator Look;
        PC->GetPlayerViewPoint(Eye, Look);
        NumberBadge->SetWorldRotation((Eye - NumberBadge->GetComponentLocation()).Rotation());
        const bool bNear = FVector::DistSquared(Eye, GetActorLocation()) < FMath::Square(4500.f);
        RacerNumber->SetVisibility(bNear); NumberShell->SetVisibility(bNear); NumberFace->SetVisibility(bNear);
    }
    if (!EngineWave) return;
    while (EngineWave->GetAvailableAudioByteCount() < 8192)
    {
        TArray<int16> Samples;
        EngineSynth.Render(Samples, 2048, FMath::Abs(GetSpeedKmh()) / SpeedLimitKmh, GetThrottleInput(), IsBurning(), KartNumber);
        EngineWave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
    }
    const AKartRaceDirector* Director = Cast<AKartRaceDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AKartRaceDirector::StaticClass()));
    const float Base = IsPlayerControlled() ? .14f : .045f;
    EngineAudio->SetVolumeMultiplier(Base * (Director && Director->IsCommentaryPlaying() ? .4f : 1.f));
}
