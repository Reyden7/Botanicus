// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "Presentation/BotanicusHeldItemProfile.h"
#include "BotanicusPlaceableItemActor.generated.h"

class UMaterialInterface;
class ABotanicusCharacter;
class UBotanicusHeldItemComponent;
enum class EBotanicusHeldItemSource : uint8;
class UMeshComponent;

/** Replicated small equipment or decoration placed from a private hotbar. */
UCLASS()
class BOTANICUS_API ABotanicusPlaceableItemActor
	: public ABotanicusInteractableActor
{
	GENERATED_BODY()

public:
	ABotanicusPlaceableItemActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	void InitializePlacedItem(FName InItemKey, int32 InQuantity = 1);
	virtual void ConfigureAsLocalPreview(bool bIsValid);
	void ConfigureAsLocalInspection();
	void LaunchItem(const FVector& InitialVelocity);
	virtual FVector GetPlacementBoxExtent() const;
	/** Signed vertical offset from the visual mesh bottom to the actor pivot. */
	virtual float GetPlacementPivotToBottomOffset() const;

	UBotanicusHeldItemComponent* GetHeldItemGrip() const { return HeldItemGrip; }
	virtual bool UsesDedicatedHeldPresentation() const { return false; }
	virtual void EquipForRobot(ABotanicusCharacter* Carrier, EBotanicusHeldItemSource Source,
		const FBotanicusHeldItemSettings& Catalogue = {});

	FName GetItemKey() const { return ItemKey; }
	int32 GetQuantity() const { return Quantity; }

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Soil Bag Stack")
	bool bCanStackSoilBags = true;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Soil Bag Stack", meta=(ClampMin="1", EditCondition="bCanStackSoilBags"))
	int32 MaxSoilBagStackCount = 10;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Soil Bag Stack", meta=(ClampMin="-2", ClampMax="4", Units="cm", EditCondition="bCanStackSoilBags"))
	float StackVerticalOffset = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Soil Bag Stack", meta=(ClampMin="0", Units="cm", EditCondition="bCanStackSoilBags"))
	float StackSnapDistance = 45.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Held Item")
	TObjectPtr<UBotanicusHeldItemComponent> HeldItemGrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Held Item")
	TObjectPtr<USceneComponent> RightHandGrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Held Item")
	TObjectPtr<USceneComponent> LeftHandGrip;

	/** Lets specialized placeable actors refine the catalogue appearance. */
	virtual void ApplyItemDefinition();

private:
	void ApplyPlacementMaterial(UMaterialInterface* Material);
	void RestorePlacementMaterials();

	UFUNCTION()
	void OnRep_ItemKey();

	UPROPERTY(ReplicatedUsing=OnRep_ItemKey)
	FName ItemKey = NAME_None;

	UPROPERTY(Replicated)
	int32 Quantity = 1;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> PreviewMaterialMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> PreviewOriginalMaterials;

	TArray<int32> PreviewMaterialCounts;

	float ThrowElapsedTime = 0.0f;
};
