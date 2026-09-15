// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWateringCanActor.generated.h"

class ABotanicusCharacter;
class USceneComponent;
class UBotanicusWaterSourceComponent;

/** Replicated physical watering can that must stay in a player's hand to water. */
UCLASS()
class BOTANICUS_API ABotanicusWateringCanActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusWateringCanActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	bool TryPickUp(ABotanicusCharacter* Character);
	void Drop();
	bool ConsumeWater(float Amount);
	bool AddWater(float Amount);
	void Refill();
	void RestoreWaterLevel(float InWaterLevel);

	/** Activates the water source. Visual rendering is intentionally separate. */
	void SetWateringEffectActive(bool bActive);

	ABotanicusCharacter* GetCarrier() const { return Carrier; }
	float GetWaterLevel() const { return WaterLevel; }
	bool HasWater() const { return WaterLevel > KINDA_SMALL_NUMBER; }
	bool IsFull() const { return WaterLevel >= 1.0f - KINDA_SMALL_NUMBER; }
	UBotanicusWaterSourceComponent* GetWaterSourceComponent() const
	{
		return WaterSourceComponent;
	}

private:
	UFUNCTION()
	void OnRep_Carrier();

	void ApplyCarrierState();

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(Replicated)
	float WaterLevel = 1.0f;

	/** Single official origin and orientation of emitted water. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Water",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> WaterNozzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Water",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBotanicusWaterSourceComponent> WaterSourceComponent;
};
