// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "BotanicusMultiPlantPotActor.generated.h"

class UChildActorComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ABotanicusCharacter;
struct FBotanicusPlantDefinition;

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusMultiPlantSlotState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Plant")
	FName PlantKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Plant")
	float WaterLevel = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Plant")
	float GrowthProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Plant")
	float CareScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Plant")
	int32 WateringCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Plant")
	bool bElementalDead = false;

	UPROPERTY(BlueprintReadOnly, Category="Environment")
	FBotanicusPlantEnvironmentState EnvironmentState;

	UPROPERTY(BlueprintReadOnly, Category="Disease")
	FBotanicusPlantDiseaseState DiseaseState;
};

/** Rectangular preparation planter containing two to four independent plants. */
UCLASS()
class BOTANICUS_API ABotanicusMultiPlantPotActor
	: public ABotanicusPlantPotActor
{
	GENERATED_BODY()

public:
    virtual int32 GetAutomaticWateringSlotCount() const override {return PlantSlots.Num();}
    virtual bool IsAutomaticWateringMature(int32 Slot) const override {return IsMature(Slot);}
    virtual bool GetAutomaticWateringNeed(int32 Slot,float Ratio,FBotanicusAutomaticWateringNeed& Out) const override;
    virtual float ApplyAutomaticWater(int32 Slot,float Ratio,float AvailableUnits,bool bFirstPulse) override;
	ABotanicusMultiPlantPotActor();
	virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

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

	virtual bool ResolveHarvestTool(const AActor* Interactor,FName& OutTool,int32& OutSlot) const override;
	int32 GetPlantCapacity() const;
	int32 GetSoilUnits() const { return SoilUnits; }
    TArray<float> GetSoilLevels() const override { return SlotSoilLevels; }
    void RestoreSoilLevels(const TArray<float>& Levels) override;
    UFUNCTION(BlueprintPure, Category="Botanicus|Soil") float GetSoilFillRatio() const;
    UFUNCTION(BlueprintPure, Category="Botanicus|Soil") float GetSoilVisualHeight() const;
    FVector GetSoilPourTarget(const APawn* Source) const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Soil", meta=(Units="cm", ToolTip="Hauteur locale du fond visuel, sans changer les emplacements logiques."))
    float SoilVisualMinHeight = -14.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Soil", meta=(Units="cm", ToolTip="Hauteur locale maximale du terreau. Choisir une valeur à l'intérieur des parois; ne change ni leur taille ni la quantité stockée."))
    float SoilVisualMaxHeight = 18.5f;
    UPROPERTY(EditAnywhere, Category="Botanicus|Soil|Editor Preview", meta=(ClampMin="0",ClampMax="1",ToolTip="Prévisualisation dans l'éditeur uniquement. En jeu, la quantité réelle est utilisée."))
    float SoilVisualPreviewFillRatio = 1.f;
    UPROPERTY(EditDefaultsOnly, Category="Botanicus|Soil|Editor Preview", meta=(ClampMin="2",ClampMax="4",ToolTip="Capacité de prévisualisation du Blueprint. En jeu, la clé du catalogue détermine toujours le nombre d'emplacements."))
    int32 PreviewPlantCapacity=2;
    FVector GetSoilSurfaceForSlot(int32 Slot) const override;
	const TArray<FBotanicusMultiPlantSlotState>&
		GetPlantSlots() const { return PlantSlots; }
	UFUNCTION(BlueprintPure, Category="Botanicus|Growing|Environment")
	FBotanicusPlantEnvironmentState GetSlotEnvironmentState(
		int32 SlotIndex) const;
	void RestoreMultiPlantState(
		int32 InSoilUnits,
		const TArray<FBotanicusMultiPlantSlotState>& InSlots);
	void ApplyElementalInfluence(
		EBotanicusPlantElement SourceElement,
		const FVector& SourceLocation);

protected:
	virtual bool CanPreviewSeedInteractionZone(AActor* Interactor) const override;

private:
    friend class UBotanicusPlantContextSubsystem;
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
	bool ProcessElementalInteractions();
	FBotanicusPlantEnvironmentState EvaluateEnvironmentState(
		const FBotanicusPlantDefinition& Definition) const;
	void RefreshVisuals();
	void SendMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_MultiPlantState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MultiSoilMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MultiSoilMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MultiSoilDynamicMaterial;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> MultiStemMeshes;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> MultiFlowerMeshes;

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Plant UI")
	TArray<TObjectPtr<UWidgetComponent>> SlotGrowthWidgets;

	UPROPERTY(EditAnywhere, Category="Botanicus|Plant UI",
		meta=(DisplayName="Marge au-dessus de chaque plante", Units="cm",
			ClampMin="0.0"))
	float SlotGrowthWidgetClearance = 10.0f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Plant UI",
		meta=(DisplayName="Echelle UI de chaque plante", ClampMin="0.05"))
	float SlotGrowthWidgetScale = 0.12f;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UWidgetComponent>> EnvironmentAlertWidgets;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UWidgetComponent>> EnvironmentDebugWidgets;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> MultiStatusText;

	UPROPERTY(ReplicatedUsing=OnRep_MultiPlantState)
	int32 SoilUnits = 0;
    UPROPERTY(ReplicatedUsing=OnRep_MultiPlantState) TArray<float> SlotSoilLevels;
    UPROPERTY(VisibleAnywhere, Category="Botanicus|Soil") TArray<TObjectPtr<UChildActorComponent>> SlotSoilVisuals;
    bool IsSlotSoilFull(int32 Slot) const { return SlotSoilLevels.IsValidIndex(Slot) && SlotSoilLevels[Slot]>=1.f; }
    float SoilReplicationElapsed=0;
    void RefreshSlotSoil();
    UPROPERTY(VisibleAnywhere, Category="Botanicus|Soil") TArray<TObjectPtr<UStaticMeshComponent>> SoilContainerWalls;
    void RefreshSoilContainer();

	UPROPERTY(ReplicatedUsing=OnRep_MultiPlantState)
	TArray<FBotanicusMultiPlantSlotState> PlantSlots;

	TWeakObjectPtr<ABotanicusCharacter> ActiveUser;
	EUseMode ActiveUseMode = EUseMode::None;
	int32 ActiveSlotIndex = INDEX_NONE;
	float ActiveProgress = 0.0f;
};
