// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusPlantPotActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class BOTANICUS_API ABotanicusPlantPotActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusPlantPotActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	/** Starts the left-mouse action selected by the player's quickbar item. */
	void BeginPrimaryUse(AActor* Interactor);

	/** Stops a held soil-filling or watering action. */
	void EndPrimaryUse(AActor* Interactor);

	void RestoreGrowingState(
		bool bInHasSoil,
		FName InPlantKey,
		float InWaterLevel,
		float InGrowthProgress);

	bool HasSoil() const { return bHasSoil; }
	FName GetPlantKey() const { return PlantKey; }
	float GetWaterLevel() const { return WaterLevel; }
	float GetGrowthProgress() const { return GrowthProgress; }
	int32 GetWateringCount() const { return WateringCount; }

	void RestoreWateringCount(int32 InWateringCount);

private:
	FName GetSelectedItemKey(AActor* Interactor) const;
	bool UpdatePrimaryUse(float DeltaSeconds);
	void RefreshVisuals();
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_GrowingState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SoilMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StemMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FoliageMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bHasSoil = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	FName PlantKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float WaterLevel = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float GrowthProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	int32 WateringCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float SoilFillProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bWateringActive = false;

	TWeakObjectPtr<class ABotanicusCharacter> ActivePrimaryUser;

	enum class EPrimaryUseMode : uint8
	{
		None,
		FillSoil,
		Water
	};

	EPrimaryUseMode PrimaryUseMode = EPrimaryUseMode::None;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Growing")
	float SoilFillDuration = 1.5f;
};
