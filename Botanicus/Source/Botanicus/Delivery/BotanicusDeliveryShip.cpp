#include "Delivery/BotanicusDeliveryShip.h"
#include "Delivery/BotanicusDeliveryZoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABotanicusDeliveryShip::ABotanicusDeliveryShip()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("FlightRoot"));
    SetRootComponent(Root);
    ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
    ShipMesh->SetupAttachment(Root);
    ShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShipMesh->SetGenerateOverlapEvents(false);
    ShipMesh->SetCanEverAffectNavigation(false);
    CargoDropPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CargoDropPoint"));
    CargoDropPoint->SetupAttachment(Root);
    CargoDropPoint->SetRelativeLocation(FVector(0,0,-150));
    SetActorEnableCollision(false);
}
void ABotanicusDeliveryShip::BeginPlay()
{
    Super::BeginPlay();
    // No collision even if the replacement mesh has an authored collision preset.
    SetActorEnableCollision(false);
    UpdateFlight();
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
    const FTransform Frame = Zone->GetDeliveryFrame();
    const FVector Origin = Frame.GetLocation() + FVector(0,0,FMath::Max(1000.f,HoverHeight));
    const FVector Entry = Origin + Frame.TransformVectorNoScale(ArrivalStartPoint);
    const FVector HoverCargo = Origin + Frame.TransformVectorNoScale(DeliveryHoverPoint);
    const FVector Exit = Origin + Frame.TransformVectorNoScale(DepartureEndPoint);
    const FQuat ArrivalFacing = FRotator(0,(HoverCargo-Entry).Rotation().Yaw,0).Quaternion();
    const FQuat ExitFacing = FRotator(0,(Exit-HoverCargo).Rotation().Yaw,0).Quaternion();
    const FVector CargoOffset = CargoDropPoint->GetRelativeLocation();
    HoverTransform = FTransform(ArrivalFacing, HoverCargo-ArrivalFacing.RotateVector(CargoOffset));
    ExitTransform = FTransform(ExitFacing, Exit-ExitFacing.RotateVector(CargoOffset));
    const FTransform EntryTransform(ArrivalFacing, Entry-ArrivalFacing.RotateVector(CargoOffset));
    SetPhase(EBotanicusDeliveryShipPhase::Arriving, FMath::Max(.1f,ArrivalDuration), EntryTransform, HoverTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Arrived,Flight.Duration,false);
}
void ABotanicusDeliveryShip::Arrived()
{
    SetPhase(EBotanicusDeliveryShipPhase::Hovering, FMath::Max(.01f,InitialPause), HoverTransform, HoverTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::DropNext,Flight.Duration,false);
}
void ABotanicusDeliveryShip::DropNext()
{
    if (!IsValid(DeliveryZone)) { TryDepart(); return; }
    if (Flight.Phase != EBotanicusDeliveryShipPhase::Dropping)
        SetPhase(EBotanicusDeliveryShipPhase::Dropping,0,HoverTransform,HoverTransform);
    // A blocked spawn retries; the paid order stays pending until a real carton exists.
    DeliveryZone->DropNextOrder(this);
    const bool bMore = DeliveryZone->HasQueuedOrders();
    GetWorldTimerManager().SetTimer(PhaseTimer,this,
        bMore ? &ThisClass::DropNext : &ThisClass::TryDepart,
        bMore ? FMath::Max(.1f,DropInterval) : FMath::Max(.01f,FinalPause),false);
}
void ABotanicusDeliveryShip::TryDepart()
{
    if (IsValid(DeliveryZone) && DeliveryZone->HasQueuedOrders()) { DropNext(); return; }
    SetPhase(EBotanicusDeliveryShipPhase::Departing,FMath::Max(.1f,DepartureDuration),HoverTransform,ExitTransform);
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Finish,Flight.Duration,false);
}
void ABotanicusDeliveryShip::Finish()
{
    SetPhase(EBotanicusDeliveryShipPhase::Finished,0,ExitTransform,ExitTransform);
    Destroy();
}
void ABotanicusDeliveryShip::OnRep_Flight()
{
    SetActorTickEnabled(Flight.Phase == EBotanicusDeliveryShipPhase::Arriving || Flight.Phase == EBotanicusDeliveryShipPhase::Departing);
    UpdateFlight();
}
void ABotanicusDeliveryShip::UpdateFlight()
{
    const bool bVisible = Flight.Phase != EBotanicusDeliveryShipPhase::Inactive && Flight.Phase != EBotanicusDeliveryShipPhase::Finished;
    SetActorHiddenInGame(!bVisible);
    if (!bVisible) return;
    const float T = Flight.Duration > 0 ? FMath::Clamp(float((ServerNow()-Flight.StartedAt)/Flight.Duration),0.f,1.f) : 1.f;
    // Curve endpoints are exact even with an imperfect user curve.
    const float Alpha = T <= 0 ? 0 : T >= 1 ? 1 : MovementCurve
        ? FMath::Clamp(MovementCurve->GetFloatValue(T),0.f,1.f) : T*T*(3-2*T);
    SetActorLocationAndRotation(FMath::Lerp(Flight.From.GetLocation(),Flight.To.GetLocation(),Alpha),
        FQuat::Slerp(Flight.From.GetRotation(),Flight.To.GetRotation(),Alpha),false,nullptr,ETeleportType::TeleportPhysics);
}
void ABotanicusDeliveryShip::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); UpdateFlight(); }
FVector ABotanicusDeliveryShip::GetCargoLocation() const { return CargoDropPoint->GetComponentLocation(); }
void ABotanicusDeliveryShip::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    if (HasAuthority() && Reason == EEndPlayReason::Destroyed && IsValid(DeliveryZone)) DeliveryZone->OnShipFinished(this);
    Super::EndPlay(Reason);
}

