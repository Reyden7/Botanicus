#include "BotanicusGameMode.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "Delivery/BotanicusBotanistBookActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Save/BotanicusWorldSaveGame.h"

bool ABotanicusGameMode::IsSavedBotanistBook(const FBotanicusSavedWorldItem& Item)
{
	// Older saves recorded the unique item by key alone.
	return Item.BotanistBookInstanceId == ABotanicusBotanistBookActor::BookId() ||
		Item.ItemKey == ABotanicusBotanistBookActor::BookKey();
}

bool ABotanicusGameMode::HasSavedBotanistBookInInventory() const
{
	const FName BookKey = ABotanicusBotanistBookActor::BookKey();
	return CurrentSaveGame->PlayerInventories.ContainsByPredicate([BookKey](const auto& Inventory)
	{
		return Inventory.Slots.ContainsByPredicate([BookKey](const auto& Slot)
		{
			return Slot.ItemKey == BookKey && !Slot.IsEmpty();
		});
	});
}

int32 ABotanicusGameMode::RestoreBotanistBook()
{
	UWorld* World = GetWorld();
	const FName BookKey = ABotanicusBotanistBookActor::BookKey();
	const FGuid BookId = ABotanicusBotanistBookActor::BookId();
	bool bBookInInventory = false;
	for (auto& Inventory : CurrentSaveGame->PlayerInventories)
	{
		for (int32 Index = 0; Index < Inventory.Slots.Num(); ++Index)
		{
			auto& Slot = Inventory.Slots[Index];
			if (Slot.ItemKey != BookKey || Slot.IsEmpty())
			{
				continue;
			}
			if (bBookInInventory || Inventory.PlayerKey.IsEmpty() || Index >= UBotanicusQuickBarComponent::SlotCount)
			{
				Slot = FBotanicusQuickBarSlot{};
				continue;
			}
			Slot.InstanceId = BookId;
			Slot.Quantity = 1;
			bBookInInventory = true;
		}
	}

	ABotanicusBotanistBookActor* LevelBook = nullptr;
	TArray<ABotanicusBotanistBookActor*> PlacedBooks;
	for (TActorIterator<ABotanicusBotanistBookActor> It(World); It; ++It)
	{
		if (It->IsHeldPresentation() || It->IsActorBeingDestroyed() || It->HasAnyFlags(RF_Transient) ||
			It->ActorHasTag(TEXT("BotanicusPlacementPreview")) || It->ActorHasTag(TEXT("BotanicusJuiceRemotePreview")))
		{
			continue;
		}
		PlacedBooks.Add(*It);
		if (!LevelBook || It->GetFName().LexicalLess(LevelBook->GetFName()))
		{
			LevelBook = *It;
		}
	}

	if (PlacedBooks.Num() > 1)
	{
		UE_LOG(LogBotanicus, Warning, TEXT("Multiple botanist books placed in %s; keeping at most one."), *World->GetMapName());
	}

	const auto* SavedBook = bBookInInventory ? nullptr : CurrentSaveGame->WorldItems.FindByPredicate(IsSavedBotanistBook);
	ABotanicusBotanistBookActor* BookToKeep = bBookInInventory ? nullptr : LevelBook;
	if (BookToKeep)
	{
		BookToKeep->ConfirmInitialPlacement(SavedBook ? &SavedBook->Transform : nullptr);
	}
	else if (SavedBook)
	{
		// A saved world book can survive removal of the original level actor.
		UClass* BookClass = SavedBook->ActorClass.IsNull() ? nullptr
			: SavedBook->ActorClass.TryLoadClass<ABotanicusBotanistBookActor>();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
		BookToKeep = World->SpawnActor<ABotanicusBotanistBookActor>(
			BookClass ? BookClass : ABotanicusBotanistBookActor::ResolveBookClass(), SavedBook->Transform, Params);
	}

	for (auto* PlacedBook : PlacedBooks)
	{
		if (PlacedBook != BookToKeep)
		{
			PlacedBook->Destroy();
		}
	}
	return SavedBook && BookToKeep ? 1 : 0;
}

void ABotanicusGameMode::ReturnBotanistBookFrom(AController* Controller)
{
	auto* Character = Controller ? Cast<ABotanicusCharacter>(Controller->GetPawn()) : nullptr;
	auto* Bar = Character ? Character->GetQuickBarComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!HasAuthority() || !Bar || !World || World->bIsTearingDown)
		return;

	int32 BookSlot = INDEX_NONE;
	for (int32 Index = 0; Index < UBotanicusQuickBarComponent::SlotCount; ++Index)
	{
		const auto Slot = Bar->GetSlot(Index);
		if (Slot.ItemKey == ABotanicusBotanistBookActor::BookKey() &&
			Slot.InstanceId == ABotanicusBotanistBookActor::BookId() && Slot.Quantity == 1)
		{
			BookSlot = Index;
			break;
		}
	}
	if (BookSlot == INDEX_NONE)
		return;

	for (TActorIterator<ABotanicusBotanistBookActor> It(World); It; ++It)
		if (!It->IsHeldPresentation() && It->GetIsReplicated() && !It->IsActorBeingDestroyed())
			return;

	UClass* Class = ABotanicusBotanistBookActor::ResolveBookClass();
	const FBox Bounds = Class->GetDefaultObject<ABotanicusBotanistBookActor>()->GetBookLocalBounds();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusBookReturn), false, Character);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);

	auto TrySurface = [&](const FVector& Location, const FQuat& Rotation, double StartOffset, double TraceLength)
	{
		FHitResult Hit;
		const FVector Start = Location + FVector(0, 0, StartOffset);
		if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, TraceLength), ECC_Visibility, Query) ||
			Hit.ImpactNormal.Z < .65f || Cast<ABotanicusPlaceableItemActor>(Hit.GetActor()))
		{
			return false;
		}
		const FTransform Pose(Rotation, Hit.ImpactPoint + FVector(0, 0, 0.5 - Bounds.Min.Z));
		if (World->OverlapAnyTestByObjectType(Pose.TransformPosition(Bounds.GetCenter()), Rotation, Objects,
			FCollisionShape::MakeBox((Bounds.GetExtent() - FVector(0.25)).ComponentMax(FVector(.25))), Query))
			return false;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Book = World->SpawnActor<ABotanicusBotanistBookActor>(Class, Pose, Params);
		if (!Book)
			return false;
		if (!Bar->RemoveQuantity(BookSlot, 1))
		{
			Book->Destroy();
			return false;
		}
		// Persist the removal while the departing controller still owns its pawn.
		CapturePlayerInventory(Controller);
		ScheduleInventoryAutosave();
		return true;
	};

	const FVector Origin = Character->GetActorLocation();
	const FQuat Facing = FRotator(0, Character->GetActorRotation().Yaw, 0).Quaternion();
	// Prefer a reachable horizontal surface beside the departing player.
	for (const FVector& Offset : {FVector(45,0,0), FVector(-45,0,0), FVector(0,45,0), FVector(0,-45,0)})
		if (TrySurface(Origin + Facing.RotateVector(Offset), Facing, 30., 190.))
			return;
	// Keep the inventory entry if every candidate was obstructed.
}
