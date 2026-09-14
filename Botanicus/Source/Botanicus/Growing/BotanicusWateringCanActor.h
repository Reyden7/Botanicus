// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWateringCanActor.generated.h"

class ABotanicusCharacter;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USplineMeshComponent;
class UStaticMesh;
class UBotanicusWaterSourceComponent;

/** Replicated physical watering can that must stay in a player's hand to water. */
UCLASS()
class BOTANICUS_API ABotanicusWateringCanActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusWateringCanActor();
	virtual void BeginPlay() override;
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
	void RefreshWateringEffect(float DeltaSeconds);
	void CreateWaterStreamVisualPool();
	void HideWaterStreamVisuals();

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(Replicated)
	float WaterLevel = 1.0f;

	/** Secondary cosmetic droplets emitted from the watering-can spout. */
	UPROPERTY(VisibleAnywhere, Category="Botanicus|Water Effect")
	TObjectPtr<UNiagaraComponent> WaterJetEffect;

	/** Niagara splash played where the stream touches the soil. */
	UPROPERTY(VisibleAnywhere, Category="Botanicus|Water Effect")
	TObjectPtr<UNiagaraComponent> WaterImpactEffect;

	/** Single origin and orientation shared by gameplay and all stream visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Water Effect",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> WaterNozzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Water Effect",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBotanicusWaterSourceComponent> WaterSourceComponent;

	/** Fixed pool: no component allocation or destruction while spraying. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> WaterStreamSegments;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> WaterStreamSegmentMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WaterStreamMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> WaterDropletSystem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> WaterImpactSystem;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Water Effect", meta=(ClampMin="4", ClampMax="32"))
	int32 MaximumVisualSegments = 24;

	bool bWateringEffectActive = false;
};
