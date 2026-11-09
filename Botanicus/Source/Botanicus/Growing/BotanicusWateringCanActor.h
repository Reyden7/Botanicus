// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWateringCanActor.generated.h"

class ABotanicusCharacter;
class ABotanicusWaterReserveActor;
class USceneComponent;
struct FBotanicusItemDefinition;

/** Replicated physical watering can that must stay in a player's hand to water. */
UCLASS()
class BOTANICUS_API ABotanicusWateringCanActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusWateringCanActor();
    virtual void OnConstruction(const FTransform& Transform) override;
    /** Shared calibrated ground geometry, independent of a local preview or held pose. */
    static bool ResolveGroundGeometry(const UObject* Context,const FBotanicusItemDefinition& Definition,const FVector& ActorScale,FVector& Extent,float& BaseOffset);
    static constexpr float GroundClearance=.25f;
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    FVector GetFillTargetLocation() const;
    ABotanicusWaterReserveActor* GetRefillStation() const;
    void SetRefillStation(ABotanicusWaterReserveActor* Station);
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Botanicus|Water") TObjectPtr<USceneComponent> WateringCanFillTarget;
	FVector GetJuiceNozzleLocation() const;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	bool TryPickUp(ABotanicusCharacter* Character);
	void Drop();
	bool ConsumeWater(float Amount);
	bool AddWater(float Amount);
	void Refill();
	void RestoreWaterLevel(float InWaterLevel);

	/**
	 * Compatibility entry point kept for existing callers.
	 * Water VFX are driven centrally from accepted watering state; this legacy hook stays inert.
	 */
	void SetWateringEffectActive(
		bool bActive,
		const FVector& TargetWorldLocation = FVector::ZeroVector);

	/** Authored nozzle; the mesh-local calibration is used when this is left at the actor origin. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Water")
	TObjectPtr<USceneComponent> WaterNozzle;

	ABotanicusCharacter* GetCarrier() const { return Carrier; }
	float GetWaterLevel() const { return WaterLevel; }
	bool HasWater() const { return WaterLevel > KINDA_SMALL_NUMBER; }
	bool IsFull() const { return WaterLevel >= 1.0f - KINDA_SMALL_NUMBER; }

protected:
    virtual void ApplyItemDefinition() override;
private:
    void UpdatePresentationPose();
    void QueueGroundSettling();
    void SettleAfterSpawn();
    FTimerHandle GroundSettleTimer;
    TWeakObjectPtr<ABotanicusWaterReserveActor> RefillStation;
	bool bUseMeshNozzleFallback=false;
	UFUNCTION()
	void OnRep_Carrier();

	void ApplyCarrierState();

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(Replicated)
	float WaterLevel = 1.0f;

};
