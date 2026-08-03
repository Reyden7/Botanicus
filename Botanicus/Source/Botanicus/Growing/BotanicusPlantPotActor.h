// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusPlantPotActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
struct FBotanicusPlantDefinition;
enum class EBotanicusPlantElement : uint8;

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
	virtual void BeginPrimaryUse(AActor* Interactor);

	/** Stops a held soil-filling or watering action. */
	virtual void EndPrimaryUse(AActor* Interactor);

	void RestoreGrowingState(
		bool bInHasSoil,
		FName InPlantKey,
		float InWaterLevel,
		float InGrowthProgress,
		float InCareScore,
		bool bInElementalDead = false);

	bool HasSoil() const { return bHasSoil; }
	FName GetPlantKey() const { return PlantKey; }
	float GetWaterLevel() const { return WaterLevel; }
	float GetGrowthProgress() const { return GrowthProgress; }
	float GetCareScore() const { return CareScore; }
	int32 GetWateringCount() const { return WateringCount; }
	bool IsElementalDead() const { return bElementalDead; }
	bool IsMature() const { return !PlantKey.IsNone() && GrowthProgress >= 0.999f; }

	void RestoreWateringCount(int32 InWateringCount);
	void ApplyElementalInfluence(
		EBotanicusPlantElement SourceElement,
		const FVector& SourceLocation);

private:
	FName GetSelectedItemKey(AActor* Interactor) const;
	const FBotanicusPlantDefinition* GetPlantDefinition() const;
	float GetCareRating() const;
	FName GetPlantQualityTag() const;
	FString GetPlantQualityLabel() const;
	FName GetQualityHarvestItemKey(
		const FBotanicusPlantDefinition& Definition) const;
	bool CanHarvestWithInteractor(AActor* Interactor) const;
	bool IsInteractorStillTargeting(AActor* Interactor) const;
	bool UpdatePrimaryUse(float DeltaSeconds);
	bool IsInCompatibleGreenhouse(
		const FBotanicusPlantDefinition& Definition) const;
	bool ProcessElementalInteractions(
		const FBotanicusPlantDefinition& Definition);
	void RefreshLocalContextAction();
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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FoliageMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ContextActionText;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bHasSoil = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	FName PlantKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float WaterLevel = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float GrowthProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float CareScore = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	int32 WateringCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bElementalDead = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float SoilFillProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bWateringActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float HarvestProgress = 0.0f;

	TWeakObjectPtr<class ABotanicusCharacter> ActivePrimaryUser;

	enum class EPrimaryUseMode : uint8
	{
		None,
		FillSoil,
		Water,
		Harvest
	};

	EPrimaryUseMode PrimaryUseMode = EPrimaryUseMode::None;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Growing")
	float SoilFillDuration = 1.5f;
};
