// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWateringCanActor.generated.h"

class ABotanicusCharacter;
class UNiagaraComponent;

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
	bool AddWater(float Amount);
	void Refill();
	void RestoreWaterLevel(float InWaterLevel);

	/** Local cosmetic stream shown while this watering can is being used. */
	void SetWateringEffectActive(
		bool bActive,
		const FVector& TargetWorldLocation = FVector::ZeroVector);

	ABotanicusCharacter* GetCarrier() const { return Carrier; }
	float GetWaterLevel() const { return WaterLevel; }
	bool HasWater() const { return WaterLevel > KINDA_SMALL_NUMBER; }
	bool IsFull() const { return WaterLevel >= 1.0f - KINDA_SMALL_NUMBER; }

private:
	UFUNCTION()
	void OnRep_Carrier();

	void ApplyCarrierState();
	void RefreshWateringEffect(float DeltaSeconds);

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(Replicated)
	float WaterLevel = 1.0f;

	/** Continuous Niagara stream emitted from the watering-can spout. */
	UPROPERTY(VisibleAnywhere, Category="Botanicus|Water Effect")
	TObjectPtr<UNiagaraComponent> WaterJetEffect;

	/** Niagara splash played where the stream touches the soil. */
	UPROPERTY(VisibleAnywhere, Category="Botanicus|Water Effect")
	TObjectPtr<UNiagaraComponent> WaterImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Water Effect", meta=(AllowPrivateAccess="true", Units="cm"))
	float WaterStreamThickness = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Water Effect", meta=(AllowPrivateAccess="true", Units="cm"))
	float WaterStreamArcHeight = 18.0f;

	bool bWateringEffectActive = false;
	FVector WateringEffectTarget = FVector::ZeroVector;
};
