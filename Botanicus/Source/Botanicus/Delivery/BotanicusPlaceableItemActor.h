// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusPlaceableItemActor.generated.h"

class UMaterialInterface;
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
	FVector GetPlacementBoxExtent() const;
	/** Signed vertical offset from the visual mesh bottom to the actor pivot. */
	float GetPlacementPivotToBottomOffset() const;

	FName GetItemKey() const { return ItemKey; }
	int32 GetQuantity() const { return Quantity; }

protected:
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
