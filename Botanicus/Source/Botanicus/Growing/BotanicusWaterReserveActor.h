#pragma once
#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWaterReserveActor.generated.h"
class ABotanicusWateringCanActor;
class ABotanicusPlayerController;
class UBoxComponent;
class UTextRenderComponent;
class UNiagaraComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
USTRUCT()
struct FBotanicusRefillStationState
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<ABotanicusWateringCanActor> Can;
    UPROPERTY() bool bOpen=false;
    UPROPERTY() uint32 OpenCount=0;
};
/** Existing unlimited water supply, operated through a single placed-can slot. */
UCLASS()
class BOTANICUS_API ABotanicusWaterReserveActor : public ABotanicusPlaceableItemActor
{
    GENERATED_BODY()
public:
    friend class UBotanicusRefillValidation;
    ABotanicusWaterReserveActor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void ConfigureAsLocalPreview(bool bIsValid) override;
    static bool AcceptPlacement(ABotanicusWateringCanActor* Can);
    bool TryDock(ABotanicusWateringCanActor* Can,bool bSnap);
    void ReleaseCan(ABotanicusWateringCanActor* Can);
    bool RequestFaucet(ABotanicusWateringCanActor* ExpectedCan,bool bOpen);
    bool ContainsCan(const ABotanicusWateringCanActor* Can) const;
    bool IsOpen() const {return State.bOpen;}
    ABotanicusWateringCanActor* GetDockedCan() const;
    uint32 GetOpenCount() const {return State.OpenCount;}
    FVector GetDockGroundLocation() const;
    bool IsRealStation() const;
    UPROPERTY(EditAnywhere,Category="Refill|Zone") FVector ZoneOffset=FVector(95,0,0);
    /** cuve_d_eau calibration: XY in mesh space, Z clearance above its bottom. */
    UPROPERTY(EditAnywhere,Category="Refill|Zone") FVector FillZoneLocalOffset=FVector(0,105,0);
    UPROPERTY(EditAnywhere,Category="Refill|Zone",meta=(ClampMin="10")) FVector ZoneHalfSize=FVector(55,45,65);
    UPROPERTY(EditAnywhere,Category="Refill|Zone") FRotator CanDockRotation=FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere,Category="Refill|Zone",meta=(ClampMin="100",ClampMax="1000")) float ZoneDisplayDistance=500;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Refill") TObjectPtr<USceneComponent> FaucetOutlet;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Refill") TObjectPtr<UBoxComponent> FillZone;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Refill") TObjectPtr<UBoxComponent> FaucetInteraction;
private:
    UFUNCTION() void OnRep_Station();
    virtual void ApplyItemDefinition() override;
    void BuildZone();
    void UpdateStation();
    void SetOpen(bool bOpen);
    void UpdateCosmetics();
    void StopCosmetics();
    void CanTransformUpdated(USceneComponent* Component,EUpdateTransformFlags Flags,ETeleportType Teleport);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> ReserveText;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ZoneFloor;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> ZoneBorder;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Stream;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Splash;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ZoneMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BorderMaterial;
    UPROPERTY(ReplicatedUsing=OnRep_Station) FBotanicusRefillStationState State;
    TWeakObjectPtr<UAudioComponent> FlowVoice;
    FTimerHandle StationTimer;
    FDelegateHandle CanTransformHandle;
    double LastTransferTime=0;
    double LastSplashTime=-1;
    bool bEnding=false;
    bool bWaterChanged=false;
};
