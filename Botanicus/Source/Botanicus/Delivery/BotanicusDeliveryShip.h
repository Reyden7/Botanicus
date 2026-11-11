#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusDeliveryShip.generated.h"

class ABotanicusDeliveryZoneActor;
class UStaticMeshComponent;
class UCurveFloat;

UENUM(BlueprintType)
enum class EBotanicusDeliveryShipPhase : uint8
{
    Inactive, Arriving, Hovering, Dropping, Departing, Finished
};

/** One coherent replicated phase, also sufficient to reconstruct a flight on late join. */
USTRUCT()
struct FBotanicusDeliveryFlight
{
    GENERATED_BODY()
    UPROPERTY() EBotanicusDeliveryShipPhase Phase = EBotanicusDeliveryShipPhase::Inactive;
    UPROPERTY() double StartedAt = 0;
    UPROPERTY() float Duration = 0;
    UPROPERTY() FTransform From;
    UPROPERTY() FTransform To;
};

/** Presentation only. Orders and carton creation remain authoritative on the delivery zone. */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusDeliveryShip : public AActor
{
    GENERATED_BODY()
public:
    ABotanicusDeliveryShip();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void StartDelivery(ABotanicusDeliveryZoneActor* Zone);
    EBotanicusDeliveryShipPhase GetPhase() const { return Flight.Phase; }
    FVector GetCargoLocation() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery") TObjectPtr<UStaticMeshComponent> ShipMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery") TObjectPtr<USceneComponent> CargoDropPoint;
    /** Unscaled local centimetres relative to DeliveryDropCenter + HoverHeight. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight") FVector ArrivalStartPoint = FVector(-7000,0,1800);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight") FVector DeliveryHoverPoint = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight") FVector DepartureEndPoint = FVector(7000,0,1800);
    /** Height of CargoDropPoint above the receiving surface. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight", meta=(ClampMin="1000", Units="cm")) float HoverHeight = 2000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight", meta=(ClampMin="0.1", Units="s")) float ArrivalDuration = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight", meta=(ClampMin="0.1", Units="s")) float DepartureDuration = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight") TObjectPtr<UCurveFloat> MovementCurve;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="0.01", Units="s")) float InitialPause = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="0.1", Units="s")) float DropInterval = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="0.01", Units="s")) float FinalPause = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="1", Units="cm")) FVector2D DropAreaSize = FVector2D(200,200);
private:
    UPROPERTY(ReplicatedUsing=OnRep_Flight) FBotanicusDeliveryFlight Flight;
    UPROPERTY() TObjectPtr<ABotanicusDeliveryZoneActor> DeliveryZone;
    FTransform HoverTransform, ExitTransform;
    FTimerHandle PhaseTimer;
    double ServerNow() const;
    void SetPhase(EBotanicusDeliveryShipPhase Phase, float Duration, const FTransform& From, const FTransform& To);
    void UpdateFlight();
    void Arrived();
    void DropNext();
    void TryDepart();
    void Finish();
    UFUNCTION() void OnRep_Flight();
};
