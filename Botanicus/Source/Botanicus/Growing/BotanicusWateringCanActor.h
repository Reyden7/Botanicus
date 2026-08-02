// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWateringCanActor.generated.h"

class ABotanicusCharacter;
class UTextRenderComponent;

/** Replicated physical watering can that must stay in a player's hand to water. */
UCLASS()
class BOTANICUS_API ABotanicusWateringCanActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusWateringCanActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	bool TryPickUp(ABotanicusCharacter* Character);
	void Drop();
	bool ConsumeWater(float Amount);
	void Refill();
	void RestoreWaterLevel(float InWaterLevel);

	ABotanicusCharacter* GetCarrier() const { return Carrier; }
	float GetWaterLevel() const { return WaterLevel; }
	bool HasWater() const { return WaterLevel > KINDA_SMALL_NUMBER; }
	bool IsFull() const { return WaterLevel >= 1.0f - KINDA_SMALL_NUMBER; }

private:
	UFUNCTION()
	void OnRep_Carrier();

	UFUNCTION()
	void OnRep_WaterLevel();

	void ApplyCarrierState();
	void RefreshWaterDisplay();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> WaterLevelText;

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(ReplicatedUsing=OnRep_WaterLevel)
	float WaterLevel = 1.0f;
};
