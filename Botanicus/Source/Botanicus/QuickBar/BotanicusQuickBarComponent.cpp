// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuickBar/BotanicusQuickBarComponent.h"

#include "BotanicusPlayerController.h"
#include "GameFramework/Pawn.h"
#include "ItemDataAsset.h"
#include "ItemDataSubsystem.h"
#include "Net/UnrealNetwork.h"

UBotanicusQuickBarComponent::UBotanicusQuickBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(SlotCount);
}

void UBotanicusQuickBarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority() && Slots.Num() != SlotCount)
	{
		Slots.SetNum(SlotCount);
	}
}

void UBotanicusQuickBarComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// A player's complete quick bar is private. Equipped-world visuals will use
	// their own replicated state when the equipment system is implemented.
	DOREPLIFETIME_CONDITION(UBotanicusQuickBarComponent, Slots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBotanicusQuickBarComponent, SelectedSlotIndex, COND_OwnerOnly);
}

TArray<FBotanicusQuickBarSlot> UBotanicusQuickBarComponent::GetSlots() const
{
	return Slots;
}

FBotanicusQuickBarSlot UBotanicusQuickBarComponent::GetSlot(int32 SlotIndex) const
{
	return IsValidSlotIndex(SlotIndex) ? Slots[SlotIndex] : FBotanicusQuickBarSlot();
}

int32 UBotanicusQuickBarComponent::GetSelectedSlotIndex() const
{
	return SelectedSlotIndex;
}

FBotanicusQuickBarSlot UBotanicusQuickBarComponent::GetSelectedSlot() const
{
	return GetSlot(SelectedSlotIndex);
}

UItemDataAsset* UBotanicusQuickBarComponent::GetItemDataForSlot(int32 SlotIndex) const
{
	const FBotanicusQuickBarSlot Slot = GetSlot(SlotIndex);
	if (Slot.IsEmpty() || !GetWorld())
	{
		return nullptr;
	}

	return UItemDataSubsystem::Get(this).GetItemDataAsset(Slot.ItemKey);
}

UItemDataAsset* UBotanicusQuickBarComponent::GetSelectedItemData() const
{
	return GetItemDataForSlot(SelectedSlotIndex);
}

bool UBotanicusQuickBarComponent::SetSlotItem(int32 SlotIndex, FName ItemKey)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!IsValidSlotIndex(SlotIndex) ||
		!IsItemKeyValid(ItemKey))
	{
		return false;
	}

	FBotanicusQuickBarSlot& Slot = Slots[SlotIndex];
	if (Slot.ItemKey == ItemKey && !Slot.IsEmpty())
	{
		return true;
	}

	Slot.ItemKey = ItemKey;
	Slot.Quantity = 1;
	Slot.InstanceId = FGuid::NewGuid();
	OnQuickBarChanged.Broadcast();

	if (SlotIndex == SelectedSlotIndex)
	{
		BroadcastSelection();
	}

	OwnerActor->ForceNetUpdate();
	return true;
}

bool UBotanicusQuickBarComponent::ClearSlot(int32 SlotIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !IsValidSlotIndex(SlotIndex))
	{
		return false;
	}

	if (Slots[SlotIndex].IsEmpty())
	{
		return true;
	}

	Slots[SlotIndex] = FBotanicusQuickBarSlot();
	OnQuickBarChanged.Broadcast();

	if (SlotIndex == SelectedSlotIndex)
	{
		BroadcastSelection();
	}

	OwnerActor->ForceNetUpdate();
	return true;
}

bool UBotanicusQuickBarComponent::AddItem(
	FName ItemKey,
	int32 Quantity,
	int32& OutSlotIndex)
{
	OutSlotIndex = INDEX_NONE;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		Quantity <= 0 ||
		!IsItemKeyValid(ItemKey))
	{
		return false;
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex].IsEmpty())
		{
			continue;
		}

		FBotanicusQuickBarSlot& Slot = Slots[SlotIndex];
		Slot.ItemKey = ItemKey;
		Slot.Quantity = Quantity;
		Slot.InstanceId = FGuid::NewGuid();
		OutSlotIndex = SlotIndex;

		OnQuickBarChanged.Broadcast();
		if (SlotIndex == SelectedSlotIndex)
		{
			BroadcastSelection();
		}

		OwnerActor->ForceNetUpdate();
		return true;
	}

	return false;
}

bool UBotanicusQuickBarComponent::RemoveQuantity(int32 SlotIndex, int32 Quantity)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!IsValidSlotIndex(SlotIndex) ||
		Quantity <= 0)
	{
		return false;
	}

	FBotanicusQuickBarSlot& Slot = Slots[SlotIndex];
	if (Slot.IsEmpty() || Quantity > Slot.Quantity)
	{
		return false;
	}

	Slot.Quantity -= Quantity;
	if (Slot.Quantity == 0)
	{
		Slot = FBotanicusQuickBarSlot();
	}

	OnQuickBarChanged.Broadcast();
	if (SlotIndex == SelectedSlotIndex)
	{
		BroadcastSelection();
	}

	OwnerActor->ForceNetUpdate();
	return true;
}

bool UBotanicusQuickBarComponent::ConsumeSelectedItem(int32 Quantity)
{
	return RemoveQuantity(SelectedSlotIndex, Quantity);
}

void UBotanicusQuickBarComponent::RequestConsumeSelectedItem(int32 Quantity)
{
	if (Quantity <= 0 ||
		!CanLocallyControlQuickBar() ||
		GetSelectedSlot().ItemKey != TEXT("SeedPacket_Test"))
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ConsumeSelectedItem(Quantity);
	}
	else
	{
		ServerConsumeSelectedItem(Quantity);
	}
}

int32 UBotanicusQuickBarComponent::GetTotalQuantity(FName ItemKey) const
{
	if (ItemKey.IsNone())
	{
		return 0;
	}

	int32 TotalQuantity = 0;
	for (const FBotanicusQuickBarSlot& Slot : Slots)
	{
		if (!Slot.IsEmpty() && Slot.ItemKey == ItemKey)
		{
			TotalQuantity += Slot.Quantity;
		}
	}

	return TotalQuantity;
}

void UBotanicusQuickBarComponent::ApplySavedState(
	const TArray<FBotanicusQuickBarSlot>& SavedSlots,
	int32 SavedSelectedSlotIndex)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	Slots.SetNum(SlotCount);
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (!SavedSlots.IsValidIndex(SlotIndex) ||
			SavedSlots[SlotIndex].IsEmpty() ||
			!IsItemKeyValid(SavedSlots[SlotIndex].ItemKey))
		{
			Slots[SlotIndex] = FBotanicusQuickBarSlot();
			continue;
		}

		Slots[SlotIndex] = SavedSlots[SlotIndex];
		if (!Slots[SlotIndex].InstanceId.IsValid())
		{
			Slots[SlotIndex].InstanceId = FGuid::NewGuid();
		}
	}

	SelectedSlotIndex = FMath::Clamp(
		SavedSelectedSlotIndex,
		0,
		SlotCount - 1);
	OnQuickBarChanged.Broadcast();
	BroadcastSelection();
}

void UBotanicusQuickBarComponent::SelectSlot(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex) || !CanLocallyControlQuickBar())
	{
		return;
	}

	SetSelectedSlotInternal(SlotIndex);

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSelectSlot(SlotIndex);
	}
}

void UBotanicusQuickBarComponent::SelectNextSlot()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const ABotanicusPlayerController* BotanicusController =
		OwnerPawn
			? Cast<ABotanicusPlayerController>(OwnerPawn->GetController())
			: nullptr;
	if (BotanicusController &&
		BotanicusController->IsQuickBarInputBlocked())
	{
		return;
	}

	SelectSlot((SelectedSlotIndex + 1) % SlotCount);
}

void UBotanicusQuickBarComponent::SelectPreviousSlot()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const ABotanicusPlayerController* BotanicusController =
		OwnerPawn
			? Cast<ABotanicusPlayerController>(OwnerPawn->GetController())
			: nullptr;
	if (BotanicusController &&
		BotanicusController->IsQuickBarInputBlocked())
	{
		return;
	}

	SelectSlot((SelectedSlotIndex - 1 + SlotCount) % SlotCount);
}

void UBotanicusQuickBarComponent::ActivateSelectedSlot()
{
	if (!CanLocallyControlQuickBar() || GetSelectedSlot().IsEmpty())
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ActivateSelectedSlotOnServer();
	}
	else
	{
		ServerActivateSelectedSlot();
	}
}

void UBotanicusQuickBarComponent::SelectSlot1()
{
	SelectSlot(0);
}

void UBotanicusQuickBarComponent::SelectSlot2()
{
	SelectSlot(1);
}

void UBotanicusQuickBarComponent::SelectSlot3()
{
	SelectSlot(2);
}

void UBotanicusQuickBarComponent::SelectSlot4()
{
	SelectSlot(3);
}

void UBotanicusQuickBarComponent::SelectSlot5()
{
	SelectSlot(4);
}

void UBotanicusQuickBarComponent::SelectSlot6()
{
	SelectSlot(5);
}

void UBotanicusQuickBarComponent::SelectSlot7()
{
	SelectSlot(6);
}

void UBotanicusQuickBarComponent::SelectSlot8()
{
	SelectSlot(7);
}

void UBotanicusQuickBarComponent::SelectSlot9()
{
	SelectSlot(8);
}

void UBotanicusQuickBarComponent::SelectSlot10()
{
	SelectSlot(9);
}

bool UBotanicusQuickBarComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) && SlotIndex >= 0 && SlotIndex < SlotCount;
}

bool UBotanicusQuickBarComponent::IsItemKeyValid(FName ItemKey) const
{
	return !ItemKey.IsNone() && GetWorld() && UItemDataSubsystem::Get(this).IsKeyValid(ItemKey);
}

bool UBotanicusQuickBarComponent::CanLocallyControlQuickBar() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return false;
	}

	return true;
}

void UBotanicusQuickBarComponent::SetSelectedSlotInternal(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex) || SelectedSlotIndex == SlotIndex)
	{
		return;
	}

	SelectedSlotIndex = SlotIndex;
	BroadcastSelection();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UBotanicusQuickBarComponent::BroadcastSelection()
{
	OnSelectedSlotChanged.Broadcast(SelectedSlotIndex, GetSelectedSlot());
}

void UBotanicusQuickBarComponent::ActivateSelectedSlotOnServer()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!IsValidSlotIndex(SelectedSlotIndex))
	{
		return;
	}

	const FBotanicusQuickBarSlot Slot = Slots[SelectedSlotIndex];
	if (Slot.IsEmpty() || !IsItemKeyValid(Slot.ItemKey))
	{
		return;
	}

	OnSlotActivated.Broadcast(SelectedSlotIndex, Slot);
}

void UBotanicusQuickBarComponent::OnRep_Slots()
{
	if (Slots.Num() != SlotCount)
	{
		Slots.SetNum(SlotCount);
	}

	for (FBotanicusQuickBarSlot& Slot : Slots)
	{
		if (Slot.IsEmpty())
		{
			Slot = FBotanicusQuickBarSlot();
		}
	}

	OnQuickBarChanged.Broadcast();
	BroadcastSelection();
}

void UBotanicusQuickBarComponent::OnRep_SelectedSlotIndex()
{
	SelectedSlotIndex = FMath::Clamp(SelectedSlotIndex, 0, SlotCount - 1);
	BroadcastSelection();
}

void UBotanicusQuickBarComponent::ServerSelectSlot_Implementation(int32 SlotIndex)
{
	if (IsValidSlotIndex(SlotIndex))
	{
		SetSelectedSlotInternal(SlotIndex);
	}
}

void UBotanicusQuickBarComponent::ServerActivateSelectedSlot_Implementation()
{
	ActivateSelectedSlotOnServer();
}

void UBotanicusQuickBarComponent::ServerConsumeSelectedItem_Implementation(
	int32 Quantity)
{
	if (GetSelectedSlot().ItemKey == TEXT("SeedPacket_Test"))
	{
		ConsumeSelectedItem(Quantity);
	}
}
