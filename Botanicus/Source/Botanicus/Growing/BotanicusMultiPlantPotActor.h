// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "BotanicusMultiPlantPotActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class ABotanicusCharacter;
struct FBotanicusPlantDefinition;

USTRUCT()
struct BOTANICUS_API FBotanicusMultiPlantSlotState
{
	GENERATED_BODY()

	UPROPERTY()
	FName PlantKey = NAME_None;

	UPROPERTY()
	float WaterLevel = 0.0f;

	UPROPERTY()
	float GrowthProgress = 0.0f;

	UPROPERTY()
	float CareScore = 0.0f;

	UPROPERTY()
	int32 WateringCount = 0;
};

/** Rectangular preparation planter containing two to four independent plants. */
UCLASS()
class BOTANICUS_API ABotanicusMultiPlantPotActor
	: public ABotanicusPlantPotActor
{
	GENERATED_BODY()

public:
	ABotanicusMultiPlantPotActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void BeginPrimaryUse(AActor* Interactor) override;
	virtual void EndPrimaryUse(AActor* Interactor) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	int32 GetPlantCapacity() const;
	int32 GetSoilUnits() const { return SoilUnits; }
	const TArray<FBotanicusMultiPlantSlotState>&
		GetPlantSlots() const { return PlantSlots; }
	void RestoreMultiPlantState(
		int32 InSoilUnits,
		const TArray<FBotanicusMultiPlantSlotState>& InSlots);

private:
	enum class EUseMode : uint8
	{
		None,
		FillSoil,
		Water,
		Harvest
	};

	int32 ResolveAimedSlot(AActor* Interactor) const;
	FVector GetSlotLocalLocation(int32 SlotIndex) const;
	const FBotanicusPlantDefinition* FindPlant(
		FName InPlantKey) const;
	bool IsMature(int32 SlotIndex) const;
	bool CanHarvest(
		int32 SlotIndex,
		AActor* Interactor) const;
	FName GetSelectedItemKey(AActor* Interactor) const;
	FName GetHarvestItemKey(
		int32 SlotIndex,
		const FBotanicusPlantDefinition& Definition) const;
	bool IsInteractorStillTargeting(
		AActor* Interactor,
		int32 SlotIndex) const;
	bool UpdateActiveUse(float DeltaSeconds);
	void RefreshVisuals();
	void SendMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_MultiPlantState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MultiSoilMesh;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> MultiStemMeshes;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> MultiFlowerMeshes;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> MultiStatusText;

	UPROPERTY(ReplicatedUsing=OnRep_MultiPlantState)
	int32 SoilUnits = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MultiPlantState)
	TArray<FBotanicusMultiPlantSlotState> PlantSlots;

	TWeakObjectPtr<ABotanicusCharacter> ActiveUser;
	EUseMode ActiveUseMode = EUseMode::None;
	int32 ActiveSlotIndex = INDEX_NONE;
	float ActiveProgress = 0.0f;
};
