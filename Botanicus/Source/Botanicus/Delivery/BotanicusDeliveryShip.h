#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusDeliveryShip.generated.h"

class ABotanicusDeliveryZoneActor;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UNiagaraSystem;
class UCurveFloat;
class UAnimSequence;
class USplineComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class EBotanicusDeliveryShipPhase : uint8
{
    Inactive, Arriving, Hovering, Dropping, Departing, Finished,
    // Append to preserve the serialized values of the existing phases.
    OpeningDoors, ClosingDoors
};

/** One coherent replicated phase, also sufficient to reconstruct a flight on late join. */
USTRUCT()
struct FBotanicusDeliveryFlight
{
    GENERATED_BODY()
    UPROPERTY() EBotanicusDeliveryShipPhase Phase = EBotanicusDeliveryShipPhase::Inactive;
    UPROPERTY() double StartedAt = 0;
    UPROPERTY() float Duration = 0;
    UPROPERTY() int32 RandomSeed = 1;
    UPROPERTY() double SequenceStartedAt = 0;
    /** Asset references and an unscaled stop frame, sent once per phase; never sampled points. */
    UPROPERTY() TObjectPtr<UCurveFloat> CurveX;
    UPROPERTY() TObjectPtr<UCurveFloat> CurveY;
    UPROPERTY() TObjectPtr<UCurveFloat> CurveZ;
    UPROPERTY() float StopProgress = .5f;
    UPROPERTY() FTransform CurveFrame;
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
    virtual void OnConstruction(const FTransform& Transform) override;
    UFUNCTION(CallInEditor, Category="Delivery|Trajectory") void RefreshTrajectoryPreview();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Trajectory") TObjectPtr<UCurveFloat> TrajectoryX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Trajectory") TObjectPtr<UCurveFloat> TrajectoryY;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Trajectory") TObjectPtr<UCurveFloat> TrajectoryZ;
    /** Shared curve time at the cargo stop. Curve values are local centimetres, relative to this sample. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Trajectory", meta=(ClampMin=".01",ClampMax=".99")) float DeliveryStopProgress = .5f;
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category="Delivery|Trajectory|Preview") bool bShowTrajectoryPreview = true;
    /** Select the delivery platform for the editor preview; otherwise this actor is the stop frame. */
    UPROPERTY(EditInstanceOnly, Category="Delivery|Trajectory|Preview") TObjectPtr<ABotanicusDeliveryZoneActor> PreviewDeliveryZone;
    UPROPERTY() TObjectPtr<USplineComponent> TrajectoryPreview;
    UPROPERTY() TObjectPtr<USphereComponent> SpawnPreview;
    UPROPERTY() TObjectPtr<USphereComponent> StopPreview;
    UPROPERTY() TObjectPtr<USphereComponent> DespawnPreview;
#endif
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void StartDelivery(ABotanicusDeliveryZoneActor* Zone);
    EBotanicusDeliveryShipPhase GetPhase() const { return Flight.Phase; }
    FVector GetCargoLocation() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery") TObjectPtr<UStaticMeshComponent> ShipMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery") TObjectPtr<USkeletalMeshComponent> ShipSkeletalMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery") TObjectPtr<USceneComponent> CargoDropPoint;
    /** Legacy endpoint settings retained for serialized compatibility; edit TrajectoryX/Y/Z instead. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Legacy") FVector ArrivalStartPoint = FVector(-12000,0,4500);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight") FVector DeliveryHoverPoint = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Legacy") FVector DepartureEndPoint = FVector(15000,0,10000);
    /** Height of CargoDropPoint above the receiving surface. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight", meta=(ClampMin="1000", Units="cm")) float HoverHeight = 2000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight", meta=(ClampMin="0.1", Units="s")) float ArrivalDuration = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight", meta=(ClampMin="0.1", Units="s")) float DepartureDuration = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Flight") TObjectPtr<UCurveFloat> MovementCurve;
    /** Legacy serialized setting; DoorAnimation length now gates the first carton. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="0.01", Units="s")) float InitialPause = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="0.1", Units="s")) float DropInterval = .6f;
    /** Legacy serialized setting; the last carton now triggers closing immediately. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="0.01", Units="s")) float FinalPause = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Drops", meta=(ClampMin="1", Units="cm")) FVector2D DropAreaSize = FVector2D(200,200);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery|Visual") TObjectPtr<USceneComponent> VisualPivot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery|Smoke") TObjectPtr<USceneComponent> EngineSmokeEmitter;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery|Audio") TObjectPtr<UAudioComponent> EngineAudio;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="8")) float HoverRoll = 7;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="6")) float HoverPitch = 4.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="20")) float HoverBob = 14;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Legacy",meta=(ClampMin="0",ClampMax="1600")) float RouteWander = 650;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Legacy",meta=(ClampMin="0",ClampMax="1200",Units="cm")) float RouteAltitudeWander = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="20",Units="deg")) float FlightRoll = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="20",Units="deg")) float FlightPitch = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="4",Units="deg")) float HoverCorrection = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke",meta=(ClampMin="1",ClampMax="1.8")) float LargeSmokePuffScale = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin=".05",ClampMax="2",Units="s")) float RotationInertia = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin=".05",ClampMax="1",Units="s")) float AngularAccelerationInertia = .22f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin="10",ClampMax="90")) float MaximumAngularSpeed = 70;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin=".05",ClampMax="2",Units="s")) float VisualPositionInertia = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin="0",ClampMax=".6",Units="s")) float HeadingLookAhead = .35f;
    /** Bank degrees per degree/second of signed path turn rate. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="1")) float RollIntensity=.16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="2")) float PitchIntensity=.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual",meta=(ClampMin="0",ClampMax="2")) float OscillationIntensity=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin=".05",ClampMax="2",Units="s")) float YawInertia=.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin=".1",ClampMax="3",Units="s")) float HoverTransitionTime=.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Visual|Smoothing",meta=(ClampMin=".1",ClampMax="3",Units="s")) float FlightTransitionTime=.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Doors") TSoftObjectPtr<UAnimSequence> DoorAnimation;
    /** Animation_doors: frame 49 at 24 fps is open; the remaining clip holds that pose. Asset seconds, not a speed multiplier. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Doors",meta=(ClampMin="0.01",Units="s")) float DoorMotionEndTime=49.f/24.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio") TSoftObjectPtr<USoundBase> EngineSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio") TSoftObjectPtr<USoundBase> HornSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio") TSoftObjectPtr<USoundBase> DoorSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio") TSoftObjectPtr<USoundAttenuation> ShipAttenuation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio",meta=(ClampMin="0",ClampMax="1")) float EngineVolume = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio",meta=(ClampMin="0",ClampMax="1")) float HornVolume = .85f;
    /** Approximately -10 dB relative to the source. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio",meta=(ClampMin="0",ClampMax="1")) float DoorVolume = .316f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Audio",meta=(ClampMin="0",Units="s")) float HornDelay = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke") TSoftObjectPtr<UNiagaraSystem> SmokeSystem;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke",meta=(ClampMin="0.1",ClampMax="3")) float SmokeInterval = .45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke",meta=(ClampMin="1")) FVector2D SmokeSizeRange = FVector2D(320,500);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke",meta=(ClampMin=".1",ClampMax="4")) float SmokeLifetime = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke",meta=(ClampMin="0",ClampMax="1")) float SmokeOpacity = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke") FVector SmokeVelocity = FVector(-25,0,35);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke") FLinearColor SmokeLightColor = FLinearColor(.65f,.63f,.59f,1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery|Smoke") FLinearColor SmokeDarkColor = FLinearColor(.38f,.37f,.35f,1);
private:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedDoorAnimation;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> DoorAnimatedMesh;
    float LastDoorPosition=-1;
    float GetDoorOpenPosition() const;
    float GetDoorMotionDuration() const;
    void InitializeDoorAnimation();
    void UpdateDoorAnimation(bool bRefreshPose=false);
    void DepartAfterDoorsClosed();
    UPROPERTY(Transient) TObjectPtr<USoundBase> LoadedHorn;
    UPROPERTY(Transient) TObjectPtr<USoundBase> LoadedDoors;
    UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedSmoke;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> LoadedAttenuation;
    FTimerHandle SmokeTimer;
    int32 SmokeIndex = 0;
    bool bPresentationStarted = false;
    double HoverStartedAt = -1;
    void StartPresentation();
    void EmitSmoke();
    void SoundHorn();
    void OpenDoors();
    UFUNCTION(NetMulticast,Reliable) void MulticastShipCue(uint8 Cue,double StartedAt);
    UPROPERTY(ReplicatedUsing=OnRep_Flight) FBotanicusDeliveryFlight Flight;
    UPROPERTY() TObjectPtr<ABotanicusDeliveryZoneActor> DeliveryZone;
    FTransform HoverTransform, ExitTransform;
    FTimerHandle PhaseTimer;
    double ServerNow() const;
    void SetPhase(EBotanicusDeliveryShipPhase Phase, float Duration, const FTransform& From, const FTransform& To);
    FTransform GetTrajectoryStopFrame(const ABotanicusDeliveryZoneActor* Zone) const;
    float TrajectoryProgress(float NormalizedTime) const;
    FVector EvaluateTrajectory(float Progress) const;
    FVector TrajectoryTangent(float Progress) const;
    float MovementProgress(float NormalizedTime) const;
    FRotator GetFlightAttitude(float NormalizedTime,double Age) const;
    void UpdateFlight(float DeltaSeconds=0);
    FQuat SmoothedVisualRotation = FQuat::Identity;
    FVector SmoothedVisualOffset = FVector::ZeroVector;
    FVector SmoothedAngularVelocity = FVector::ZeroVector;
    bool bVisualPoseInitialized = false;
    void Arrived();
    void DropNext();
    void TryDepart();
    void Finish();
    UFUNCTION() void OnRep_Flight();
};
