#include "Delivery/BotanicusDeliveryShip.h"
#include "Delivery/BotanicusDeliveryZoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "Components/SplineComponent.h"
#include "Components/SphereComponent.h"
#endif


ABotanicusDeliveryShip::ABotanicusDeliveryShip()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    DoorAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/livreur/animation/Animation_doors.Animation_doors")));
    EngineSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Delivery/SW_DeliveryEngine.SW_DeliveryEngine")));
    HornSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Delivery/SW_DeliveryHorn.SW_DeliveryHorn")));
    DoorSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Delivery/SW_DeliveryDoors.SW_DeliveryDoors")));
    ShipAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Delivery/SA_DeliveryShip.SA_DeliveryShip")));
    SmokeSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/VFX/Delivery/NS_DeliverySmoke.NS_DeliverySmoke")));
    // Racine du vaisseau
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("FlightRoot"));
    SetRootComponent(Root);

    VisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualPivot"));
    VisualPivot->SetupAttachment(Root);
    EngineSmokeEmitter = CreateDefaultSubobject<USceneComponent>(TEXT("EngineSmokeEmitter"));
    EngineSmokeEmitter->SetupAttachment(VisualPivot);
    EngineSmokeEmitter->SetRelativeLocation(FVector(-450,0,30));
    EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("DeliveryEngineAudio"));
    EngineAudio->SetupAttachment(VisualPivot);
    EngineAudio->bAutoActivate = false;
    EngineAudio->bAllowSpatialization = true;
    EngineAudio->bStopWhenOwnerDestroyed = true;

    // Ancien Static Mesh conservé pour la compatibilité du Blueprint
    ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
    ShipMesh->SetupAttachment(VisualPivot);
    ShipMesh->SetVisibility(false, false);
    ShipMesh->SetHiddenInGame(true, false);
    ShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShipMesh->SetGenerateOverlapEvents(false);
    ShipMesh->SetCanEverAffectNavigation(false);

    // Nouveau Skeletal Mesh
    ShipSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
        TEXT("ShipSkeletalMesh")
    );

    // Hérite de la position et de l'orientation de l'ancien mesh
    ShipSkeletalMesh->SetupAttachment(ShipMesh);
    ShipSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShipSkeletalMesh->SetGenerateOverlapEvents(false);
    ShipSkeletalMesh->SetCanEverAffectNavigation(false);

    // Point de largage des cartons : inchangé
    CargoDropPoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("CargoDropPoint")
    );
    CargoDropPoint->SetupAttachment(Root);
    CargoDropPoint->SetRelativeLocation(FVector(0, 0, -150));

#if WITH_EDITOR
    TrajectoryPreview=CreateEditorOnlyDefaultSubobject<USplineComponent>(TEXT("TrajectoryPreview"));
    if(TrajectoryPreview)
    {
        TrajectoryPreview->SetupAttachment(Root);TrajectoryPreview->SetIsVisualizationComponent(true);
        TrajectoryPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);TrajectoryPreview->SetCanEverAffectNavigation(false);
        TrajectoryPreview->SetHiddenInGame(true);TrajectoryPreview->SetDrawDebug(true);
        TrajectoryPreview->EditorUnselectedSplineSegmentColor=FLinearColor(.25f,.65f,1.f);
    }
    auto Marker=[&](const TCHAR* Name,FColor Color)
    {
        auto* Sphere=CreateEditorOnlyDefaultSubobject<USphereComponent>(Name);
        if(Sphere)
        {
            Sphere->SetupAttachment(Root);Sphere->InitSphereRadius(100);Sphere->ShapeColor=Color;
            Sphere->SetIsVisualizationComponent(true);Sphere->SetHiddenInGame(true);
            Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sphere->SetCanEverAffectNavigation(false);
        }
        return Sphere;
    };
    SpawnPreview=Marker(TEXT("TrajectorySpawn"),FColor::Green);
    StopPreview=Marker(TEXT("TrajectoryStop"),FColor::Yellow);
    DespawnPreview=Marker(TEXT("TrajectoryDespawn"),FColor::Red);
#endif
    SetActorEnableCollision(false);
}

void ABotanicusDeliveryShip::BeginPlay()
{
    Super::BeginPlay();
    TArray<USceneComponent*> AttachedVisuals; GetRootComponent()->GetChildrenComponents(false,AttachedVisuals);
    for (auto* Child : AttachedVisuals)
        if (Child != VisualPivot && Child != CargoDropPoint)
            Child->AttachToComponent(VisualPivot,FAttachmentTransformRules::KeepRelativeTransform);
    LoadedHorn = HornSound.LoadSynchronous(); LoadedDoors = DoorSound.LoadSynchronous();
    LoadedDoorAnimation=DoorAnimation.LoadSynchronous();
    InitializeDoorAnimation();
    if (GetNetMode()!=NM_DedicatedServer)
    {
        LoadedSmoke = SmokeSystem.LoadSynchronous(); LoadedAttenuation = ShipAttenuation.LoadSynchronous();
        EngineAudio->SetSound(EngineSound.LoadSynchronous()); EngineAudio->AttenuationSettings = LoadedAttenuation;
    }
    ShipMesh->SetVisibility(false, false);
    ShipMesh->SetHiddenInGame(true, false);
    ShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // No collision even if the replacement mesh has an authored collision preset.
    SetActorEnableCollision(false);
    UpdateFlight();
    UpdateDoorAnimation(true);
    if (Flight.Phase!=EBotanicusDeliveryShipPhase::Inactive && Flight.Phase!=EBotanicusDeliveryShipPhase::Finished) StartPresentation();
}

void ABotanicusDeliveryShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABotanicusDeliveryShip, Flight);
}
double ABotanicusDeliveryShip::ServerNow() const
{
    const auto* State = GetWorld()->GetGameState();
    return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}
void ABotanicusDeliveryShip::SetPhase(EBotanicusDeliveryShipPhase Phase, float Duration,
    const FTransform& From, const FTransform& To)
{
    check(HasAuthority());
    Flight.Phase = Phase; Flight.Duration = Duration; Flight.StartedAt = ServerNow();
    Flight.From = From; Flight.To = To;
    ForceNetUpdate(); OnRep_Flight();
    UE_LOG(LogTemp, Display, TEXT("DeliveryShip %s phase=%d time=%.3f duration=%.2f"),
        *GetName(), int32(Phase), Flight.StartedAt, Duration);
}
void ABotanicusDeliveryShip::StartDelivery(ABotanicusDeliveryZoneActor* Zone)
{
    if (!HasAuthority() || !IsValid(Zone) || Flight.Phase != EBotanicusDeliveryShipPhase::Inactive) return;
    DeliveryZone = Zone;
    Flight.RandomSeed=FMath::RandRange(1,MAX_int32-200); Flight.SequenceStartedAt=ServerNow();
    if(!TrajectoryX || !TrajectoryY || !TrajectoryZ)
    {
        UE_LOG(LogTemp,Error,TEXT("DeliveryShip %s: assign all three trajectory curves; orders remain pending."),*GetName());
        return;
    }
    Flight.CurveX=TrajectoryX;Flight.CurveY=TrajectoryY;Flight.CurveZ=TrajectoryZ;
    Flight.StopProgress=FMath::Clamp(DeliveryStopProgress,.01f,.99f);
    Flight.CurveFrame=GetTrajectoryStopFrame(Zone);
    HoverTransform=Flight.CurveFrame;
    ExitTransform=FTransform(Flight.CurveFrame.GetRotation(),EvaluateTrajectory(1));
    const FTransform EntryTransform(Flight.CurveFrame.GetRotation(),EvaluateTrajectory(0));
    SetPhase(EBotanicusDeliveryShipPhase::Arriving, FMath::Max(.1f,ArrivalDuration), EntryTransform, HoverTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Arrived,Flight.Duration,false);
}
void ABotanicusDeliveryShip::Arrived()
{
    const float HornDuration=LoadedHorn ? LoadedHorn->GetDuration() : 0;
    SetPhase(EBotanicusDeliveryShipPhase::Hovering,FMath::Max(.01f,HornDelay)+HornDuration,HoverTransform,HoverTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::SoundHorn,FMath::Max(.01f,HornDelay),false);
}
void ABotanicusDeliveryShip::SoundHorn()
{
    MulticastShipCue(0,ServerNow());
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::OpenDoors,
        LoadedHorn ? FMath::Max(.01f,LoadedHorn->GetDuration()) : .01f,false);
}
void ABotanicusDeliveryShip::OpenDoors()
{
    if(!HasAuthority() || Flight.Phase!=EBotanicusDeliveryShipPhase::Hovering)return;
    const float Duration=GetDoorMotionDuration();
    if(Duration<=0 || !DoorAnimatedMesh)
    {
        UE_LOG(LogTemp,Error,TEXT("DeliveryShip %s: missing compatible door animation; paid orders remain pending."),*GetName());
        return;
    }
    SetPhase(EBotanicusDeliveryShipPhase::OpeningDoors,Duration,HoverTransform,HoverTransform);
    MulticastShipCue(1,Flight.StartedAt);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::DropNext,Duration,false);
}
void ABotanicusDeliveryShip::MulticastShipCue_Implementation(uint8 Cue,double StartedAt)
{
    if (GetNetMode()==NM_DedicatedServer || ServerNow()-StartedAt>.5) return;
    auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>();
    if (!Audio) return;
    auto* Voice=Audio->PlaySpatialTransition(this,Cue==0 ? LoadedHorn.Get():LoadedDoors.Get(),
        GetActorLocation(),Cue==0 ? HornVolume:DoorVolume,0,LoadedAttenuation,10000.f);
    if (Voice) Voice->AttachToComponent(VisualPivot,FAttachmentTransformRules::KeepWorldTransform);
    UE_LOG(LogTemp,Verbose,TEXT("DeliveryCue %s cue=%d net=%d serverTime=%.3f voice=%d engine=%d"),
        *GetName(),Cue,int32(GetNetMode()),StartedAt,IsValid(Voice),EngineAudio->IsPlaying());
}
void ABotanicusDeliveryShip::DropNext()
{
    if(!HasAuthority() || (Flight.Phase!=EBotanicusDeliveryShipPhase::OpeningDoors && Flight.Phase!=EBotanicusDeliveryShipPhase::Dropping))return;
    // Do not rely on timer rounding or a cosmetic animation callback to authorize a carton.
    const double Remaining=Flight.StartedAt+Flight.Duration-ServerNow();
    if(Flight.Phase==EBotanicusDeliveryShipPhase::OpeningDoors && Remaining>0)
    {
        GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::DropNext,float(FMath::Max(.001,Remaining)),false);return;
    }
    if (!IsValid(DeliveryZone)) { TryDepart(); return; }
    if (Flight.Phase != EBotanicusDeliveryShipPhase::Dropping)
        SetPhase(EBotanicusDeliveryShipPhase::Dropping,0,HoverTransform,HoverTransform);
    // A blocked spawn retries; the paid order stays pending until a real carton exists.
    DeliveryZone->DropNextOrder(this);
    const bool bMore = DeliveryZone->HasQueuedOrders();
    if(bMore)
        GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::DropNext,FMath::Max(.1f,DropInterval),false);
    else
        TryDepart(); // The carton is already spawned/launched; do not wait for its impact or another timer.
}
void ABotanicusDeliveryShip::TryDepart()
{
    if (IsValid(DeliveryZone) && DeliveryZone->HasQueuedOrders()) { DropNext(); return; }
    const float Duration=GetDoorMotionDuration();
    SetPhase(EBotanicusDeliveryShipPhase::ClosingDoors,Duration,HoverTransform,HoverTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::DepartAfterDoorsClosed,FMath::Max(.001f,Duration),false);
}
void ABotanicusDeliveryShip::DepartAfterDoorsClosed()
{
    if(!HasAuthority() || Flight.Phase!=EBotanicusDeliveryShipPhase::ClosingDoors)return;
    const double Remaining=Flight.StartedAt+Flight.Duration-ServerNow();
    if(Remaining>0)
    {
        GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::DepartAfterDoorsClosed,float(FMath::Max(.001,Remaining)),false);return;
    }
    // Orders arriving after the departure decision stay queued for the next ship, as before.
    SetPhase(EBotanicusDeliveryShipPhase::Departing,FMath::Max(.1f,DepartureDuration),HoverTransform,ExitTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Finish,Flight.Duration,false);
}
void ABotanicusDeliveryShip::Finish()
{
    if(!HasAuthority() || Flight.Phase!=EBotanicusDeliveryShipPhase::Departing)return;
    const double Remaining=Flight.StartedAt+Flight.Duration-ServerNow();
    if(Remaining>0)
    {
        GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Finish,float(FMath::Max(.001,Remaining)),false);return;
    }
    // Reach the authored last point before hiding, even if the timer precedes this frame's actor tick.
    UpdateFlight();
    SetPhase(EBotanicusDeliveryShipPhase::Finished,0,ExitTransform,ExitTransform);
    Destroy();
}
void ABotanicusDeliveryShip::OnRep_Flight()
{
    const bool bActive=Flight.Phase!=EBotanicusDeliveryShipPhase::Inactive && Flight.Phase!=EBotanicusDeliveryShipPhase::Finished;
    SetActorTickEnabled(bActive);
    if (Flight.Phase==EBotanicusDeliveryShipPhase::Hovering) HoverStartedAt=Flight.StartedAt;
    if (bActive && HasActorBegunPlay()) StartPresentation();
    UpdateFlight();
    UpdateDoorAnimation(true);
}
FTransform ABotanicusDeliveryShip::GetTrajectoryStopFrame(const ABotanicusDeliveryZoneActor* Zone) const
{
    const FTransform Frame=Zone->GetDeliveryFrame();
    const FVector Cargo=Frame.GetLocation()+FVector(0,0,FMath::Max(1000.f,HoverHeight))+Frame.TransformVectorNoScale(DeliveryHoverPoint);
    return FTransform(Frame.GetRotation(),Cargo-Frame.GetRotation().RotateVector(CargoDropPoint->GetRelativeLocation()),FVector::OneVector);
}
float ABotanicusDeliveryShip::TrajectoryProgress(float NormalizedTime) const
{
    if(Flight.Phase==EBotanicusDeliveryShipPhase::Arriving)return Flight.StopProgress*MovementProgress(NormalizedTime);
    if(Flight.Phase==EBotanicusDeliveryShipPhase::Departing)return FMath::Lerp(Flight.StopProgress,1.f,MovementProgress(NormalizedTime));
    return Flight.Phase==EBotanicusDeliveryShipPhase::Finished ? 1.f : Flight.StopProgress;
}
FVector ABotanicusDeliveryShip::EvaluateTrajectory(float Progress) const
{
    if(!Flight.CurveX || !Flight.CurveY || !Flight.CurveZ)return Flight.CurveFrame.GetLocation();
    auto Sample=[&](float U){return FVector(Flight.CurveX->GetFloatValue(U),Flight.CurveY->GetFloatValue(U),Flight.CurveZ->GetFloatValue(U));};
    return Flight.CurveFrame.GetLocation()+Flight.CurveFrame.TransformVectorNoScale(Sample(FMath::Clamp(Progress,0.f,1.f))-Sample(Flight.StopProgress));
}
FVector ABotanicusDeliveryShip::TrajectoryTangent(float Progress) const
{
    // Stay on the current side of the stop: the outgoing tangent may differ from the incoming one.
    const bool bDeparting=Flight.Phase==EBotanicusDeliveryShipPhase::Departing;
    const float Min=bDeparting ? Flight.StopProgress:0.f,Max=bDeparting ? 1.f:Flight.StopProgress;
    return EvaluateTrajectory(FMath::Min(Max,Progress+.001f))-EvaluateTrajectory(FMath::Max(Min,Progress-.001f));
}
void ABotanicusDeliveryShip::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshTrajectoryPreview();
}
void ABotanicusDeliveryShip::RefreshTrajectoryPreview()
{
#if WITH_EDITOR
    // No preview geometry, tick or debug drawing in PIE, game or Blueprint thumbnails.
    if(!GetWorld() || GetWorld()->WorldType!=EWorldType::Editor || !TrajectoryPreview)return;
    const bool bShow=bShowTrajectoryPreview && TrajectoryX && TrajectoryY && TrajectoryZ;
    TrajectoryPreview->SetVisibility(bShow);
    for(auto* Marker:{SpawnPreview.Get(),StopPreview.Get(),DespawnPreview.Get()})if(Marker)Marker->SetVisibility(bShow);
    TrajectoryPreview->ClearSplinePoints(false);
    if(!bShow){TrajectoryPreview->UpdateSpline();return;}
    const float Stop=FMath::Clamp(DeliveryStopProgress,.01f,.99f);
    const FTransform Frame=IsValid(PreviewDeliveryZone)?GetTrajectoryStopFrame(PreviewDeliveryZone):FTransform(GetActorQuat(),GetActorLocation(),FVector::OneVector);
    auto Sample=[&](float U){return FVector(TrajectoryX->GetFloatValue(U),TrajectoryY->GetFloatValue(U),TrajectoryZ->GetFloatValue(U));};
    auto Position=[&](float U){return Frame.GetLocation()+Frame.TransformVectorNoScale(Sample(U)-Sample(Stop));};
    constexpr int32 Samples=256;
    for(int32 I=0;I<=Samples;++I)
    {
        TrajectoryPreview->AddSplinePoint(Position(float(I)/Samples),ESplineCoordinateSpace::World,false);
        TrajectoryPreview->SetSplinePointType(I,ESplinePointType::Linear,false);
    }
    TrajectoryPreview->UpdateSpline();
    SpawnPreview->SetWorldLocation(Position(0));StopPreview->SetWorldLocation(Position(Stop));DespawnPreview->SetWorldLocation(Position(1));
#endif
}
float ABotanicusDeliveryShip::MovementProgress(float NormalizedTime) const
{
    const float T=FMath::Clamp(NormalizedTime,0.f,1.f);
    return T<=0 ? 0 : T>=1 ? 1 : MovementCurve
        ? FMath::Clamp(MovementCurve->GetFloatValue(T),0.f,1.f) : T*T*T*(T*(6*T-15)+10); // Zero velocity AND acceleration at the phase endpoints.
}
FRotator ABotanicusDeliveryShip::GetFlightAttitude(float NormalizedTime,double Age) const
{
    if(Flight.Duration<=0 || !Flight.CurveX || !Flight.CurveY || !Flight.CurveZ)return FRotator::ZeroRotator;
    const float Ahead=TrajectoryProgress(NormalizedTime+HeadingLookAhead/Flight.Duration);
    FVector Tangent=TrajectoryTangent(Ahead);
    if(Tangent.IsNearlyZero())Tangent=GetActorForwardVector();
    const FRotator Direction=Tangent.Rotation();
    const float Heading=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,Direction.Yaw);
    const bool bDeparting=Flight.Phase==EBotanicusDeliveryShipPhase::Departing;
    const float Before=FMath::Max(bDeparting?Flight.StopProgress:0.f,Ahead-.002f);
    const float After=FMath::Min(bDeparting?1.f:Flight.StopProgress,Ahead+.002f);
    const FVector In=TrajectoryTangent(Before).GetSafeNormal2D(),Out=TrajectoryTangent(After).GetSafeNormal2D();
    const float Turn=FMath::Atan2(FVector::CrossProduct(In,Out).Z,FVector::DotProduct(In,Out));
    const float A=FMath::Clamp(NormalizedTime-.001f,0.f,1.f),B=FMath::Clamp(NormalizedTime+.001f,0.f,1.f);
    const float Speed=B>A ? FVector::Distance(EvaluateTrajectory(TrajectoryProgress(A)),EvaluateTrajectory(TrajectoryProgress(B)))/((B-A)*Flight.Duration):0;
    const float TurnRate=FMath::RadiansToDegrees(Turn/FMath::Max(1.f,FVector::Distance(EvaluateTrajectory(Before),EvaluateTrajectory(After)))*Speed);
    const double Seed=(Flight.RandomSeed%1000)*.01;
    const float Sway=FMath::Sin(Age*.65+Seed)*.65f+FMath::Sin(Age*.31+Seed*.37)*.35f;
    // Leave a little headroom for the spring's gentle over-correction.
    const float Bank=FMath::Clamp(TurnRate*RollIntensity+.8f*Sway*OscillationIntensity,-FlightRoll*.9f,FlightRoll*.9f);
    const float Tilt=FMath::Clamp(Direction.Pitch*PitchIntensity+.6f*FMath::Sin(Age*.48+Seed)*OscillationIntensity,-FlightPitch*.9f,FlightPitch*.9f);
    return FRotator(Tilt,Heading, Bank);
}
void ABotanicusDeliveryShip::UpdateFlight(float DeltaSeconds)
{
    const bool bVisible=Flight.Phase!=EBotanicusDeliveryShipPhase::Inactive && Flight.Phase!=EBotanicusDeliveryShipPhase::Finished;
    SetActorHiddenInGame(!bVisible);
    if (!bVisible) return;
    const double Now=ServerNow();
    const float T=Flight.Duration>0 ? FMath::Clamp(float((Now-Flight.StartedAt)/Flight.Duration),0.f,1.f) : 1.f;
    const bool bMoving=Flight.Phase==EBotanicusDeliveryShipPhase::Arriving || Flight.Phase==EBotanicusDeliveryShipPhase::Departing;
    const FVector Position=EvaluateTrajectory(TrajectoryProgress(T));
    SetActorLocationAndRotation(Position,Flight.CurveFrame.GetRotation(),false,nullptr,ETeleportType::TeleportPhysics);
    if (GetNetMode()==NM_DedicatedServer) return;
    const double Age=Now-Flight.SequenceStartedAt,Seed=(Flight.RandomSeed%1000)*.01;
    auto Mix=[&](double Rate){return .67*FMath::Sin(Age*Rate+Seed)+.33*FMath::Sin(Age*Rate*1.73+Seed*.37);};
    const bool bHover=Flight.Phase==EBotanicusDeliveryShipPhase::Hovering || Flight.Phase==EBotanicusDeliveryShipPhase::Dropping ||
        Flight.Phase==EBotanicusDeliveryShipPhase::OpeningDoors || Flight.Phase==EBotanicusDeliveryShipPhase::ClosingDoors;
    const float HoverAge=float(Now-(HoverStartedAt>=0 ? HoverStartedAt:Flight.StartedAt));
    const float PhaseAge=FMath::Max(0.f,float(Now-Flight.StartedAt));
    const float HoverWeight=bHover ? FMath::SmoothStep(0.f,FMath::Max(.1f,HoverTransitionTime),HoverAge)
        : Flight.Phase==EBotanicusDeliveryShipPhase::Departing ? 1-FMath::SmoothStep(0.f,FMath::Max(.1f,FlightTransitionTime),PhaseAge) : 0;
    const float MovingWeight=bMoving ? FMath::SmoothStep(0.f,FMath::Max(.1f,FlightTransitionTime),PhaseAge)*
        FMath::SmoothStep(0.f,FMath::Max(.1f,HoverTransitionTime),FMath::Max(0.f,Flight.Duration-PhaseAge)) : 0;
    // Broad, deterministic compensation waves rather than fast angular kicks.
    const float Correction=FMath::Sin(Age*.8+Seed)*FMath::Sin(Age*.37+Seed*.73);
    const float Bias=(Flight.RandomSeed&1) ? 1.2f : -1.2f;
    const float HoverBank=FMath::Clamp(float(HoverRoll*Mix(1.07)+Bias+HoverCorrection*Correction),-HoverRoll,HoverRoll);
    const float HoverTilt=FMath::Clamp(float(HoverPitch*Mix(.83)-HoverCorrection*Correction),-HoverPitch,HoverPitch);
    const FRotator Flying=bMoving?GetFlightAttitude(T,Age):FRotator::ZeroRotator;
    // The BP mesh's -90 degree local yaw already maps imported +Y forward onto pivot +X.
    // One absolute quaternion target drives that existing pivot; door bones and logical cargo root stay independent.
    const FQuat TargetRotation=FRotator(HoverWeight*HoverTilt*OscillationIntensity+MovingWeight*Flying.Pitch,
        MovingWeight*Flying.Yaw+HoverWeight*.8f*Mix(.56)*OscillationIntensity,
        HoverWeight*HoverBank*OscillationIntensity+MovingWeight*Flying.Roll).Quaternion();
    const float Bob=FMath::Clamp(float(HoverBob*Mix(.71)+Correction*2),-HoverBob,HoverBob);
    const FVector TargetOffset=OscillationIntensity*FVector(HoverWeight*(5*Mix(.6)+Correction*2),HoverWeight*12*Mix(.95),HoverWeight*Bob);
    if (!bVisualPoseInitialized)
    {
        SmoothedVisualRotation=TargetRotation;SmoothedVisualOffset=TargetOffset;bVisualPoseInitialized=true;
    }
    else if (DeltaSeconds>0)
    {
        const float PositionWeight=1-FMath::Exp(-DeltaSeconds/FMath::Max(.05f,VisualPositionInertia));
        // Damped angular velocity supplies inertia and a slight over-correction.
        // Substeps keep the same response when recording or during a long frame.
        const int32 Steps=FMath::Clamp(FMath::CeilToInt(DeltaSeconds*60),1,64);
        const float Step=DeltaSeconds/Steps;
        for (int32 I=0;I<Steps;++I)
        {
            FQuat Error=(TargetRotation*SmoothedVisualRotation.Inverse()).GetNormalized();
            if (Error.W<0)Error=FQuat(-Error.X,-Error.Y,-Error.Z,-Error.W);
            FVector Axis;double Angle;Error.ToAxisAndAngle(Axis,Angle);
            const FVector ErrorVector=Axis*Angle;
            const FVector DesiredVelocity(ErrorVector.X/FMath::Max(.05f,RotationInertia),
                ErrorVector.Y/FMath::Max(.05f,RotationInertia),ErrorVector.Z/FMath::Max(.05f,YawInertia));
            const float AccelerationWeight=1-FMath::Exp(-Step/FMath::Max(.05f,AngularAccelerationInertia));
            SmoothedAngularVelocity=FMath::Lerp(SmoothedAngularVelocity,DesiredVelocity,AccelerationWeight)
                .GetClampedToMaxSize(FMath::DegreesToRadians(FMath::Max(1.f,MaximumAngularSpeed)));
            const FVector Turn=SmoothedAngularVelocity*Step;
            const double TurnAngle=Turn.Size();
            if (TurnAngle>KINDA_SMALL_NUMBER)
                SmoothedVisualRotation=(FQuat(Turn/TurnAngle,TurnAngle)*SmoothedVisualRotation).GetNormalized();
        }
        SmoothedVisualOffset=FMath::Lerp(SmoothedVisualOffset,TargetOffset,PositionWeight);
    }
    // Preserve the same filtered pose across all phase notifications. A late join
    // initializes to the current pose instead of replaying earlier corrections.
    VisualPivot->SetRelativeRotation(SmoothedVisualRotation);
    VisualPivot->SetRelativeLocation(SmoothedVisualOffset);
}
void ABotanicusDeliveryShip::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); UpdateFlight(DeltaSeconds); UpdateDoorAnimation(); if (!bPresentationStarted) StartPresentation(); }
FVector ABotanicusDeliveryShip::GetCargoLocation() const { return CargoDropPoint->GetComponentLocation(); }
void ABotanicusDeliveryShip::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().ClearTimer(SmokeTimer);
    EngineAudio->Stop();
    if (HasAuthority() && Reason == EEndPlayReason::Destroyed && IsValid(DeliveryZone)) DeliveryZone->OnShipFinished(this);
    Super::EndPlay(Reason);
}


void ABotanicusDeliveryShip::StartPresentation()
{
    if (bPresentationStarted || GetNetMode()==NM_DedicatedServer) return;
    if (!EngineAudio->IsRegistered() || !EngineAudio->Sound) return;
    bPresentationStarted=true;
    if (EngineAudio->Sound)
    {
        EngineAudio->FadeIn(.15f,EngineVolume,0.f);
    }
    if (LoadedSmoke) EmitSmoke();
}
void ABotanicusDeliveryShip::EmitSmoke()
{
    if (!LoadedSmoke || GetNetMode()==NM_DedicatedServer || Flight.Phase==EBotanicusDeliveryShipPhase::Finished) return;
    FRandomStream Random(Flight.RandomSeed+SmokeIndex*31);
    if (auto* Puff=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),LoadedSmoke,EngineSmokeEmitter->GetComponentLocation(),
        FRotator::ZeroRotator,FVector::OneVector,true,false,ENCPoolMethod::AutoRelease,true))
    {
        FLinearColor Color=FMath::Lerp(SmokeDarkColor,SmokeLightColor,Random.FRand());Color.A=SmokeOpacity;
        Puff->SetVariableInt(TEXT("User.BurstCount"),SmokeIndex%5==2 ? 4 : 3);
        const bool bLargePacket=SmokeIndex%5==2;
        const float PacketScale=bLargePacket ? FMath::Clamp(LargeSmokePuffScale,1.f,1.8f) : 1.f;
        const float Size=Random.FRandRange(FMath::Max(1.,SmokeSizeRange.X),FMath::Max(SmokeSizeRange.X,SmokeSizeRange.Y))*PacketScale;
        Puff->SetVariableFloat(TEXT("User.Lifetime"),SmokeLifetime*(bLargePacket ? 1.1f : 1.f));
        Puff->SetVariableFloat(TEXT("User.Size"),Size);
        Puff->SetVariableLinearColor(TEXT("User.Tint"),Color);
        const FVector Drift=GetActorRotation().RotateVector(SmokeVelocity)+FVector(Random.FRandRange(-8,8),Random.FRandRange(-8,8),0);
        Puff->SetVariableVec3(TEXT("User.Velocity"),Drift);Puff->SetVariableVec3(TEXT("User.Gravity"),FVector::ZeroVector);
        Puff->SetVariableFloat(TEXT("User.Radius"),Size*.16f);
        const float BoundsExtent=Size*.7f+SmokeLifetime*70.f;
        Puff->SetSystemFixedBounds(FBox(FVector(-BoundsExtent),FVector(BoundsExtent)));Puff->Activate(true);
    }
    const float Factor=SmokeIndex%3==0 ? .32f : SmokeIndex%3==1 ? 1.65f : Random.FRandRange(.8f,1.2f);
    ++SmokeIndex;
    GetWorldTimerManager().SetTimer(SmokeTimer,this,&ThisClass::EmitSmoke,FMath::Max(.1f,SmokeInterval*Factor),false);
}


float ABotanicusDeliveryShip::GetDoorOpenPosition() const
{
    return LoadedDoorAnimation?FMath::Clamp(DoorMotionEndTime,0.f,LoadedDoorAnimation->GetPlayLength()):0;
}
float ABotanicusDeliveryShip::GetDoorMotionDuration() const
{
    // Preserve the authored speed. Skip the held tail in both directions, not the movement itself.
    const float Rate=LoadedDoorAnimation?FMath::Abs(LoadedDoorAnimation->RateScale):0;
    return Rate>SMALL_NUMBER?GetDoorOpenPosition()/Rate:0;
}
void ABotanicusDeliveryShip::InitializeDoorAnimation()
{
    if(!LoadedDoorAnimation)return;
    auto Compatible=[this](USkeletalMeshComponent* Component)
    {
        const auto* Mesh=Component?Component->GetSkeletalMeshAsset():nullptr;
        return Mesh && Mesh->GetSkeleton()==LoadedDoorAnimation->GetSkeleton();
    };
    // The current Blueprint owns a separate SkeletalMesh component; its native slot is empty.
    if(Compatible(ShipSkeletalMesh))DoorAnimatedMesh=ShipSkeletalMesh;
    else
    {
        TInlineComponentArray<USkeletalMeshComponent*> Components(this);
        for(auto* Component:Components)if(Compatible(Component)){DoorAnimatedMesh=Component;break;}
    }
    if(!DoorAnimatedMesh || GetNetMode()==NM_DedicatedServer)return;
    DoorAnimatedMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    DoorAnimatedMesh->SetAnimation(LoadedDoorAnimation);
    DoorAnimatedMesh->AddTickPrerequisiteActor(this);
    if(auto* Instance=DoorAnimatedMesh->GetSingleNodeInstance())
    {
        Instance->SetLooping(false);Instance->SetPlaying(false);
    }
}
void ABotanicusDeliveryShip::UpdateDoorAnimation(bool bRefreshPose)
{
    if(!DoorAnimatedMesh || !LoadedDoorAnimation || GetNetMode()==NM_DedicatedServer)return;
    auto* Instance=DoorAnimatedMesh->GetSingleNodeInstance();if(!Instance)return;
    const float Length=GetDoorOpenPosition();
    const float Alpha=Flight.Duration>0?FMath::Clamp(float((ServerNow()-Flight.StartedAt)/Flight.Duration),0.f,1.f):1.f;
    float Position=0;
    if(Flight.Phase==EBotanicusDeliveryShipPhase::OpeningDoors)Position=Length*Alpha;
    else if(Flight.Phase==EBotanicusDeliveryShipPhase::Dropping)Position=Length;
    else if(Flight.Phase==EBotanicusDeliveryShipPhase::ClosingDoors)Position=Length*(1-Alpha);
    // Absolute server time drives both directions. No local accumulated time, looping, notifies or per-frame RPC.
    if(bRefreshPose || Position!=LastDoorPosition)
    {
        Instance->SetPosition(Position,false);LastDoorPosition=Position;
        if(bRefreshPose)
        {
            DoorAnimatedMesh->TickAnimation(0,false);
            DoorAnimatedMesh->RefreshBoneTransforms();
        }
    }
}
