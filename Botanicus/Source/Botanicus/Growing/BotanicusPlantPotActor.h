// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "BotanicusPlantPotActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UChildActorComponent;
class UBotanicusPlantEnvironmentAlertWidget;
class UBotanicusPlantEnvironmentDebugWidget;

UCLASS()
class BOTANICUS_API ABotanicusPlantPotActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusPlantPotActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

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
	virtual void ApplyItemDefinition() override;

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

	/** True only when there is enough soil to plant or repot. */
	bool HasSoil() const { return IsSoilFull(); }
	FName GetPlantKey() const { return PlantKey; }
	float GetWaterLevel() const { return WaterLevel; }
	float GetGrowthProgress() const { return GrowthProgress; }
	float GetCareScore() const { return CareScore; }
	UFUNCTION(BlueprintPure, Category="Botanicus|Growing|Environment")
	FBotanicusPlantEnvironmentState GetEnvironmentState() const
	{
		return EnvironmentState;
	}
	UFUNCTION(BlueprintPure, Category="Botanicus|Growing|Environment")
	float GetEnvironmentalComfort() const
	{
		return EnvironmentState.OverallComfort;
	}
	UFUNCTION(BlueprintPure, Category="Botanicus|Growing|Environment")
	FText GetEnvironmentDiagnostic() const;
	float GetSoilMaximumHeight() const;
	int32 GetWateringCount() const { return WateringCount; }
	bool IsElementalDead() const { return bElementalDead; }
	UFUNCTION(BlueprintPure, Category="Botanicus|Growing|Element")
	EBotanicusPlantElement GetPotElement() const;
	UFUNCTION(BlueprintPure, Category="Botanicus|Growing|Element")
	bool IsPlantElementCompatible() const;
	bool IsMature() const { return !PlantKey.IsNone() && GrowthProgress >= 0.999f; }
	void SetInspectionVisible(bool bVisible);
	bool IsInspectionVisible() const { return bInspectionVisible; }
	/** Harvest item representing the current plant and its present quality. */
	FName GetCurrentHarvestItemKey() const;

	void RestoreWateringCount(int32 InWateringCount);
	void ApplyElementalInfluence(
		EBotanicusPlantElement SourceElement,
		const FVector& SourceLocation);

protected:
	/** Whether this aimed location can currently receive the selected seed. */
	virtual bool CanPreviewSeedInteractionZone(AActor* Interactor) const;
	void RefreshSeedInteractionPreview();

private:
	FName GetSelectedItemKey(AActor* Interactor) const;
	const FBotanicusPlantDefinition* GetPlantDefinition() const;
	float GetCareRating() const;
	FName GetPlantQualityTag() const;
	FString GetPlantQualityLabel() const;
	FName GetQualityHarvestItemKey(
		const FBotanicusPlantDefinition& Definition) const;
	bool CanHarvestWithInteractor(AActor* Interactor) const;
	float GetSoilLevel() const;
	bool IsSoilFull() const;
	bool IsInteractorStillTargeting(AActor* Interactor) const;
	bool UpdatePrimaryUse(float DeltaSeconds);
	bool UpdateEnvironmentState(
		const FBotanicusPlantDefinition& Definition);
	bool ProcessElementalInteractions(
		const FBotanicusPlantDefinition& Definition);
	void RefreshLocalContextAction();
	FVector2D GetConfiguredSoilHorizontalOffset() const;
	float GetConfiguredSoilMaximumHeight() const;
	FVector2D GetConfiguredSoilBottomRadii() const;
	FVector2D GetConfiguredSoilTopRadii() const;
	float GetConfiguredSoilVolumeHeight() const;
	bool GetConfiguredSquareSoilProfile() const;
	float GetConfiguredPlantBaseHeight() const;
	void RefreshSoilVisual();
	void RefreshSeedInteractionZoneGeometry(
		float Radius,
		EBotanicusPlantElement Element,
		int32 CompatibleNeighbourCount,
		int32 IncompatibleNeighbourCount);
	void RefreshVisuals();
	void RefreshInspectionWidget();
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_GrowingState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SoilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> SoilShapeVisual;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	FVector2D SoilHorizontalOffset = FVector2D::ZeroVector;

	/** Final height of the soil surface relative to the pot actor, in centimetres. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category="Botanicus|Appearance|Soil",
		meta=(
			AllowPrivateAccess="true",
			DisplayName="Soil Maximum Height",
			Units="cm"))
	float SoilSurfaceHeight = 18.5f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	FVector2D SoilBottomRadii = FVector2D(23.0f, 23.0f);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	FVector2D SoilTopRadii = FVector2D(34.0f, 34.0f);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil", meta=(Units="cm", ClampMin="1.0"))
	float SoilVolumeHeight = 34.0f;

	/** Use a square soil surface for square culture-pot meshes. */
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	bool bSquareSoilProfile = false;

	/** Height where the seed/stem emerges from the soil surface. */
	UPROPERTY(
		EditDefaultsOnly,
		Category="Botanicus|Appearance|Plant",
		meta=(DisplayName="Plant Base Height", Units="cm"))
	float PlantBaseHeight = 18.0f;

	/** Growth retained when a plant is planted in another elemental pot. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Botanicus|Growing|Element",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float IncompatiblePotElementGrowthMultiplier = 0.05f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StemMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FoliageMesh;

	/** Local-only ring shown before planting the selected seed. */
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> SeedInteractionZoneParts;

	/** Explicit ring materials: blue neutral, green compatible, red incompatible. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> SeedInteractionNeutralMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SeedInteractionCompatibleMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SeedInteractionIncompatibleMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FoliageMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ContextActionText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PlantGrowthWidget;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> PlantInspectionWidget;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> EnvironmentAlertWidget;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> EnvironmentDebugWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Hauteur minimale UI croissance", Units="cm",
			ClampMin="0.0", AllowPrivateAccess="true"))
	float PlantGrowthWidgetHeight = 115.0f;

	/** Empty space kept between the highest leaf and the bottom of the UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Marge au-dessus de la plante", Units="cm",
			ClampMin="0.0", ClampMax="100.0", AllowPrivateAccess="true"))
	float PlantGrowthWidgetClearance = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Echelle UI croissance", ClampMin="0.05",
			ClampMax="1.0", AllowPrivateAccess="true"))
	float PlantGrowthWidgetScale = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Luminosite UI croissance", ClampMin="0.1",
			ClampMax="5.0", AllowPrivateAccess="true"))
	float PlantGrowthWidgetBrightness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Echelle icones environnement", ClampMin="0.05",
			ClampMax="1.0", AllowPrivateAccess="true"))
	float EnvironmentAlertWidgetScale = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Echelle debug environnement", ClampMin="0.05",
			ClampMax="1.0", AllowPrivateAccess="true"))
	float EnvironmentDebugWidgetScale = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Distance debug a droite", Units="cm",
			ClampMin="0.0", ClampMax="250.0", AllowPrivateAccess="true"))
	float EnvironmentDebugWidgetSideOffset = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Hauteur debug environnement", Units="cm",
			ClampMin="0.0", ClampMax="300.0", AllowPrivateAccess="true"))
	float EnvironmentDebugWidgetHeight = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Distance panneau devant le pot", Units="cm",
			ClampMin="0.0", ClampMax="150.0", AllowPrivateAccess="true"))
	float PlantInspectionWidgetSideOffset = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Decalage horizontal panneau inspection", Units="cm",
			ClampMin="-150.0", ClampMax="150.0", AllowPrivateAccess="true"))
	float PlantInspectionWidgetHorizontalOffset = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Plant UI",
		meta=(DisplayName="Hauteur panneau inspection", Units="cm",
			ClampMin="0.0", ClampMax="150.0", AllowPrivateAccess="true"))
	float PlantInspectionWidgetHeight = 28.0f;

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
	FBotanicusPlantEnvironmentState EnvironmentState;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	int32 WateringCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float PlantingStartServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bElementalDead = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float SoilFillProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	bool bWateringActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_GrowingState)
	float HarvestProgress = 0.0f;

	TWeakObjectPtr<class ABotanicusCharacter> ActivePrimaryUser;

	bool bInspectionVisible = false;

	enum class EPrimaryUseMode : uint8
	{
		None,
		FillSoil,
		RemoveSoil,
		Water,
		Harvest
	};

	EPrimaryUseMode PrimaryUseMode = EPrimaryUseMode::None;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Growing")
	float SoilFillDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Growing")
	float SoilRemovalDuration = 2.0f;
};
