// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "BotanicusStorageShelfActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class ABotanicusPlaceableItemActor;

/** Physical floor or wall shelf with fixed snap points for small stock items. */
UCLASS()
class BOTANICUS_API ABotanicusStorageShelfActor
	: public ABotanicusLargeEquipmentActor
{
	GENERATED_BODY()

public:
	ABotanicusStorageShelfActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual FVector GetPlacementBoxExtent() const override;
	virtual void BeginPlacement(ABotanicusCharacter* Character) override;
	virtual void UpdatePlacement(
		const FTransform& PlacementTransform,
		bool bIsValid) override;
	virtual void ConfirmPlacement() override;
	virtual void CancelPlacement() override;
	virtual void SetLocalPlacementPreview(
		const FTransform& PlacementTransform,
		bool bIsValid) override;

	static bool IsCatalogItemCompatible(
		const UObject* Context,
		FName InItemKey);
	static int32 GetStorageStackLimit(FName InItemKey);

	bool IsWallMountedShelf() const;
	int32 GetStorageSlotCount() const;
	bool HasStoredItems() const;
	void SetMoveContentsWithFurniture(bool bEnabled);
	void GetStoredItems(TArray<ABotanicusPlaceableItemActor*>& OutItems) const;
	bool FindClosestAvailableStorageSlot(
		FName InItemKey,
		const FVector& ReferenceLocation,
		const FVector& ItemExtent,
		FTransform& OutTransform,
		const AActor* IgnoredItem = nullptr,
		int32* OutSlotIndex = nullptr,
		ABotanicusPlaceableItemActor** OutExistingStack = nullptr,
		int32 InQuantity = 1) const;
	void ShowAvailableSlotsForLocalPlayer(
		FName InItemKey,
		const AActor* IgnoredItem = nullptr);
	int32 FindStorageSlotIndexForItem(
		const ABotanicusPlaceableItemActor* Item) const;
	int32 FindClosestStorageSlotIndex(
		const FVector& WorldLocation,
		float MaximumDistance = 60.0f) const;
	FVector GetStorageSlotAimLocation(int32 SlotIndex) const;
	bool GetStorageSlotPlacement(
		int32 SlotIndex,
		FName InItemKey,
		const FVector& ItemExtent,
		FTransform& OutTransform,
		const AActor* IgnoredItem = nullptr,
		ABotanicusPlaceableItemActor** OutExistingStack = nullptr,
		int32 InQuantity = 1) const;
	ABotanicusPlaceableItemActor* GetStoredItemInSlot(
		int32 SlotIndex,
		const AActor* IgnoredItem = nullptr) const;

protected:
	virtual void OnEquipmentDefinitionApplied() override;

private:
	void RefreshShelfConfiguration();
	void RefreshSlotPreviewVisibility();
	void RefreshLocalStockLabel();
	FVector GetSlotBaseLocalLocation(int32 SlotIndex) const;
	FTransform GetStorageSlotTransform(
		int32 SlotIndex,
		const FVector& ItemExtent) const;
	bool CanSlotAcceptItem(
		int32 SlotIndex,
		FName InItemKey,
		const AActor* IgnoredItem = nullptr,
		int32 InQuantity = 1) const;
	void CaptureStoredItemTransforms();
	void ApplyStoredItemTransforms();
	void ClearStoredItemTransforms();

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> ShelfBoards;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> SidePanels;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> SlotMarkers;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ShelfLabel;

	TArray<FVector> SlotBaseLocations;
	FVector ConfiguredExtent = FVector(30.0f, 85.0f, 75.0f);
	float LastLocalPreviewTime = -1000.0f;
	FName LocallyPreviewedItemKey = NAME_None;
	TWeakObjectPtr<const AActor> LocallyIgnoredItem;
	TArray<TWeakObjectPtr<ABotanicusPlaceableItemActor>>
		MovingStoredItems;
	TArray<FTransform> MovingStoredItemRelativeTransforms;
	bool bMoveContentsWithFurniture = false;
};
