// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusIllegalPlanterActor.generated.h"

class UChildActorComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;
class ABotanicusCharacter;

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusIllegalPlantSlotState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PlantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0"))
	float GrowthProgress = 0.0f;

	/** Water belongs to this plant, never to the whole planter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
	float WaterAmount = 0.0f;
};

/** Four-slot MVP planter for the simplified clandestine crop loop. */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusIllegalPlanterActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusIllegalPlanterActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	void BeginPrimaryUse(AActor* Interactor);
	void EndPrimaryUse(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	int32 GetPlanterLevel() const { return PlanterLevel; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	int32 GetSoilUnits() const { return SoilUnits; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	/** Total water retained for legacy saves and aggregate status displays. */
	float GetWaterAmount() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	TArray<FBotanicusIllegalPlantSlotState> GetPlantSlots() const { return PlantSlots; }

	void RestoreIllegalPlanterState(
		int32 InLevel,
		int32 InSoilUnits,
		float InWaterAmount,
		const TArray<FBotanicusIllegalPlantSlotState>& InSlots);

	void SetAllGrowthForDevelopment(float NormalizedGrowth);
	void FillWaterForDevelopment();

protected:
	virtual void ApplyItemDefinition() override;

private:
	enum class EPrimaryUseMode : uint8
	{
		None,
		FillSoil,
		Water
	};

	UFUNCTION()
	void OnRep_PlanterState();

	void EnsureSlotCount();
	float GetPlantSlotSpacing() const;
	void RefreshVisuals();
	void RefreshGrowthWidgets();
	void ScheduleAutosave();
	int32 ResolveAimedSlot(AActor* Interactor) const;
	bool IsInteractorStillTargeting(AActor* Interactor) const;
	bool UpdatePrimaryUse(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> SoilShapeVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LeftWallVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RightWallVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> FrontWallVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BackWallVisual;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> PlantVisuals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<UWidgetComponent>> PlantGrowthWidgets;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> StatusText;

	/** Interior dimensions of the soil surface. Editable on BP_Item_IllegalPlanter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Illegal Planter|Visuals", meta=(AllowPrivateAccess="true", ClampMin="1.0", Units="cm"))
	FVector2D SoilHalfExtent = FVector2D(84.0f, 44.0f);

	/** Height of a completely filled soil volume. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Illegal Planter|Visuals", meta=(AllowPrivateAccess="true", ClampMin="1.0", Units="cm"))
	float SoilVolumeHeight = 65.0f;

	/** Local Z at the bottom of the interior planter volume. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Illegal Planter|Visuals", meta=(AllowPrivateAccess="true", Units="cm"))
	float SoilBottomHeight = 15.0f;

	/** Height of each plant information widget above its planter slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Illegal Planter|UI", meta=(AllowPrivateAccess="true", Units="cm"))
	float PlantGrowthWidgetHeight = 175.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Illegal Planter|UI", meta=(AllowPrivateAccess="true", ClampMin="0.01", ClampMax="1.0"))
	float PlantGrowthWidgetScale = 0.12f;

	UPROPERTY(ReplicatedUsing=OnRep_PlanterState)
	int32 PlanterLevel = 1;

	UPROPERTY(ReplicatedUsing=OnRep_PlanterState)
	int32 SoilUnits = 0;

	UPROPERTY(ReplicatedUsing=OnRep_PlanterState)
	TArray<FBotanicusIllegalPlantSlotState> PlantSlots;

	TWeakObjectPtr<ABotanicusCharacter> ActivePrimaryUser;
	EPrimaryUseMode ActivePrimaryUseMode = EPrimaryUseMode::None;
	float ActiveSoilFillProgress = 0.0f;
	int32 ActiveWaterSlotIndex = INDEX_NONE;

	int32 LastReplicatedGrowthBucket = INDEX_NONE;
	int32 LastAutosaveGrowthBucket = INDEX_NONE;
	int32 LastAutosaveWaterBucket = INDEX_NONE;
};
