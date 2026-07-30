// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusPlaceableItemActor.generated.h"

class UMaterialInterface;

/** Replicated small equipment or decoration placed from a private hotbar. */
UCLASS()
class BOTANICUS_API ABotanicusPlaceableItemActor
	: public ABotanicusInteractableActor
{
	GENERATED_BODY()

public:
	ABotanicusPlaceableItemActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	void InitializePlacedItem(FName InItemKey, int32 InQuantity = 1);
	void ConfigureAsLocalPreview(bool bIsValid);
	FVector GetPlacementBoxExtent() const;

	FName GetItemKey() const { return ItemKey; }
	int32 GetQuantity() const { return Quantity; }

private:
	void ApplyItemDefinition();

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
};
