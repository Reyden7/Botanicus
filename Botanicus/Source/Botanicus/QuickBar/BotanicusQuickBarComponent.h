// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BotanicusQuickBarComponent.generated.h"

class UItemDataAsset;

/**
 * One private inventory/hotbar slot backed by Item Data Framework.
 *
 * InstanceId keeps two packets of the same seed species distinct, while
 * Quantity stores the number of uses/seeds remaining in that physical item.
 */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusQuickBarSlot
{
	GENERATED_BODY()

	/** Stable key of a UItemDataAsset. NAME_None represents an empty slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Bar")
	FName ItemKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Bar", meta=(ClampMin="0"))
	int32 Quantity = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quick Bar")
	FGuid InstanceId;

	bool IsEmpty() const
	{
		return ItemKey.IsNone() || Quantity <= 0;
	}

	bool operator==(const FBotanicusQuickBarSlot& Other) const
	{
		return ItemKey == Other.ItemKey &&
			Quantity == Other.Quantity &&
			InstanceId == Other.InstanceId;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBotanicusQuickBarChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FBotanicusSelectedSlotChangedSignature,
	int32, SelectedSlotIndex,
	FBotanicusQuickBarSlot, SelectedSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FBotanicusQuickBarActivatedSignature,
	int32, SlotIndex,
	FBotanicusQuickBarSlot, Slot);

/**
 * Ten-slot, keyboard-first personal inventory with owner-only replication.
 *
 * Slot contents can only be changed by authoritative gameplay code. Clients may
 * select and activate slots, but the server validates every request.
 */
UCLASS(ClassGroup=(Botanicus), meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusQuickBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 SlotCount = 10;

	UBotanicusQuickBarComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns a copy of all ten slots for UI rendering. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Quick Bar")
	TArray<FBotanicusQuickBarSlot> GetSlots() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Quick Bar")
	FBotanicusQuickBarSlot GetSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Quick Bar")
	int32 GetSelectedSlotIndex() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Quick Bar")
	FBotanicusQuickBarSlot GetSelectedSlot() const;

	/** Resolves a slot through Item Data Framework. Returns null for an empty/invalid slot. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Quick Bar")
	UItemDataAsset* GetItemDataForSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Quick Bar")
	UItemDataAsset* GetSelectedItemData() const;

	/**
	 * Assigns an Item Data key. This intentionally works only on the server so the
	 * future inventory remains the authority over which items a player owns.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Botanicus|Quick Bar")
	bool SetSlotItem(int32 SlotIndex, FName ItemKey);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Botanicus|Quick Bar")
	bool ClearSlot(int32 SlotIndex);

	/** Creates one distinct item instance in the first empty slot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Botanicus|Inventory")
	bool AddItem(FName ItemKey, int32 Quantity, int32& OutSlotIndex);

	/** Removes a quantity from one slot, clearing it when it reaches zero. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Botanicus|Inventory")
	bool RemoveQuantity(int32 SlotIndex, int32 Quantity);

	/** Convenience operation used by seed packets and consumable tools. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Botanicus|Inventory")
	bool ConsumeSelectedItem(int32 Quantity = 1);

	/** Requests an authoritative consumption from the owning local player. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Inventory")
	void RequestConsumeSelectedItem(int32 Quantity = 1);

	UFUNCTION(BlueprintPure, Category="Botanicus|Inventory")
	int32 GetTotalQuantity(FName ItemKey) const;

	/** Restores an authoritative save snapshot and replicates it to the owner. */
	void ApplySavedState(
		const TArray<FBotanicusQuickBarSlot>& SavedSlots,
		int32 SavedSelectedSlotIndex);

	/** Selects a slot locally and synchronizes the selection with the server. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Quick Bar")
	void SelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Quick Bar")
	void SelectNextSlot();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Quick Bar")
	void SelectPreviousSlot();

	/** Requests activation of the selected item. Gameplay listeners run on the server. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Quick Bar")
	void ActivateSelectedSlot();

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Quick Bar")
	FBotanicusQuickBarChangedSignature OnQuickBarChanged;

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Quick Bar")
	FBotanicusSelectedSlotChangedSignature OnSelectedSlotChanged;

	/** Server-side event used by equipment/tool gameplay systems. */
	UPROPERTY(BlueprintAssignable, Category="Botanicus|Quick Bar")
	FBotanicusQuickBarActivatedSignature OnSlotActivated;

	// Parameterless input handlers used by UInputComponent key bindings.
	void SelectSlot1();
	void SelectSlot2();
	void SelectSlot3();
	void SelectSlot4();
	void SelectSlot5();
	void SelectSlot6();
	void SelectSlot7();
	void SelectSlot8();
	void SelectSlot9();
	void SelectSlot10();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Slots)
	TArray<FBotanicusQuickBarSlot> Slots;

	UPROPERTY(ReplicatedUsing=OnRep_SelectedSlotIndex)
	int32 SelectedSlotIndex = 0;

	bool IsValidSlotIndex(int32 SlotIndex) const;
	bool IsItemKeyValid(FName ItemKey) const;
	bool CanLocallyControlQuickBar() const;
	void SetSelectedSlotInternal(int32 SlotIndex);
	void BroadcastSelection();
	void ActivateSelectedSlotOnServer();

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_SelectedSlotIndex();

	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerActivateSelectedSlot();

	UFUNCTION(Server, Reliable)
	void ServerConsumeSelectedItem(int32 Quantity);
};
