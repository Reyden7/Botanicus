// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusGameMode.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Building/BotanicusCatalogBuildingActor.h"
#include "Building/BotanicusCommunicationDoorActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Path/BotanicusPathActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Save/BotanicusWorldSaveGame.h"

namespace
{
	const FName PurchasedBuildingTag(TEXT("BotanicusPurchasedBuilding"));

	void ConfigureRestoredPurchasedActorForNetworking(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		Actor->SetOwner(nullptr);
		Actor->bOnlyRelevantToOwner = false;
		Actor->bAlwaysRelevant = true;
		Actor->SetNetUpdateFrequency(30.0f);
		Actor->SetMinNetUpdateFrequency(10.0f);
		Actor->SetReplicates(true);
		Actor->SetReplicateMovement(true);
		Actor->SetNetDormancy(DORM_Awake);
		Actor->FlushNetDormancy();
		Actor->ForceNetUpdate();
	}
}

ABotanicusGameMode::ABotanicusGameMode()
{
}

void ABotanicusGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		LoadAutosave();
		RestoreWorldState();
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator ControllerIt =
					 World->GetPlayerControllerIterator();
				 ControllerIt;
				 ++ControllerIt)
			{
				RestorePlayerEconomy(ControllerIt->Get());
			}
		}
	}
}

void ABotanicusGameMode::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		BotanicusSaveNow();
	}

	Super::EndPlay(EndPlayReason);
}

void ABotanicusGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (HasAuthority())
	{
		RestorePlayerEconomy(NewPlayer);
	}
}

void ABotanicusGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (HasAuthority())
	{
		RestorePlayerInventory(NewPlayer);
		RestorePlayerEconomy(NewPlayer);
		if (ABotanicusPlayerController* BotanicusController =
			Cast<ABotanicusPlayerController>(NewPlayer))
		{
			BotanicusController->ClientHideRemovedBuildingActors(
				RemovedBuildingActorNames.Array());
		}
	}
}

void ABotanicusGameMode::Logout(AController* Exiting)
{
	if (HasAuthority())
	{
		if (ABotanicusPlayerController* BotanicusController =
			Cast<ABotanicusPlayerController>(Exiting))
		{
			BotanicusController->
				CancelPendingBuildingPurchaseForLogout();
		}
		CapturePlayerInventory(Exiting);
		CapturePlayerEconomy(Exiting);
	}

	Super::Logout(Exiting);
}

void ABotanicusGameMode::RegisterRemovedBuildingActor(FName ActorName)
{
	if (HasAuthority() && !ActorName.IsNone())
	{
		RemovedBuildingActorNames.Add(ActorName);
	}
}

bool ABotanicusGameMode::BotanicusSaveNow()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return false;
	}

	if (!CurrentSaveGame)
	{
		CurrentSaveGame =
			Cast<UBotanicusWorldSaveGame>(
				UGameplayStatics::CreateSaveGameObject(
					UBotanicusWorldSaveGame::StaticClass()));
	}
	if (!CurrentSaveGame)
	{
		return false;
	}

	CurrentSaveGame->MapName =
		UGameplayStatics::GetCurrentLevelName(this, true);
	CurrentSaveGame->SaveVersion = 6;
	CurrentSaveGame->BuildingActors.Reset();
	CurrentSaveGame->Paths.Reset();
	CurrentSaveGame->WorldItems.Reset();
	CurrentSaveGame->RemovedBuildingActorNames =
		RemovedBuildingActorNames.Array();
	CurrentSaveGame->CommunicationDoors.Reset();

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!IsValid(Actor) ||
			(!Actor->ActorHasTag(PurchasedBuildingTag) &&
			 !Actor->GetClass()->GetPathName().Contains(
				 TEXT("/EasyBuildingSystem/Blueprints/BuildingObjects/"))))
		{
			continue;
		}

		FBotanicusSavedBuildingActor& SavedActor =
			CurrentSaveGame->BuildingActors.AddDefaulted_GetRef();
		SavedActor.ActorName = Actor->GetFName();
		SavedActor.ActorClass = FSoftClassPath(Actor->GetClass());
		SavedActor.Transform = Actor->GetActorTransform();
		SavedActor.bRuntimeSpawned =
			Actor->ActorHasTag(PurchasedBuildingTag);
	}

	for (TActorIterator<ABotanicusPathActor> PathIt(World);
		 PathIt;
		 ++PathIt)
	{
		if (PathIt->IsPreviewPath())
		{
			continue;
		}

		const TArray<FVector> WorldPoints = PathIt->GetPathWorldPoints();
		if (WorldPoints.Num() < 2)
		{
			continue;
		}

		FBotanicusSavedPath& SavedPath =
			CurrentSaveGame->Paths.AddDefaulted_GetRef();
		SavedPath.Points.Reserve(WorldPoints.Num());
		for (const FVector& Point : WorldPoints)
		{
			SavedPath.Points.Add(FVector_NetQuantize10(Point));
		}
		for (const FVector& JunctionPoint :
			 PathIt->GetJunctionWorldPoints())
		{
			SavedPath.JunctionPoints.Add(
				FVector_NetQuantize10(JunctionPoint));
		}
	}

	for (TActorIterator<ABotanicusCommunicationDoorActor> DoorIt(World);
		 DoorIt;
		 ++DoorIt)
	{
		CurrentSaveGame->CommunicationDoors.Add(
			DoorIt->GetActorTransform());
	}

	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(World);
		 ParcelIt;
		 ++ParcelIt)
	{
		if (!IsValid(*ParcelIt) || ParcelIt->IsActorBeingDestroyed())
		{
			continue;
		}

		FBotanicusSavedWorldItem& SavedItem =
			CurrentSaveGame->WorldItems.AddDefaulted_GetRef();
		SavedItem.ActorClass = FSoftClassPath(ParcelIt->GetClass());
		SavedItem.Transform = ParcelIt->GetActorTransform();
		SavedItem.ItemKey = ParcelIt->GetItemKey();
		SavedItem.Quantity = ParcelIt->GetQuantity();
	}

	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(World);
		 EquipmentIt;
		 ++EquipmentIt)
	{
		if (!IsValid(*EquipmentIt) ||
			EquipmentIt->IsActorBeingDestroyed())
		{
			continue;
		}

		FBotanicusSavedWorldItem& SavedItem =
			CurrentSaveGame->WorldItems.AddDefaulted_GetRef();
		SavedItem.ActorClass = FSoftClassPath(EquipmentIt->GetClass());
		SavedItem.Transform = EquipmentIt->GetActorTransform();
		SavedItem.ItemKey = EquipmentIt->GetItemKey();
		SavedItem.Quantity = 1;
	}

	for (TActorIterator<ABotanicusPlaceableItemActor> ItemIt(World);
		 ItemIt;
		 ++ItemIt)
	{
		if (!IsValid(*ItemIt) ||
			ItemIt->IsActorBeingDestroyed() ||
			ItemIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}

		FBotanicusSavedWorldItem& SavedItem =
			CurrentSaveGame->WorldItems.AddDefaulted_GetRef();
		SavedItem.ActorClass = FSoftClassPath(ItemIt->GetClass());
		SavedItem.Transform = ItemIt->GetActorTransform();
		SavedItem.ItemKey = ItemIt->GetItemKey();
		SavedItem.Quantity = ItemIt->GetQuantity();
		if (const ABotanicusPlantPotActor* PlantPot =
			Cast<ABotanicusPlantPotActor>(*ItemIt))
		{
			SavedItem.bPlantPotHasSoil = PlantPot->HasSoil();
			SavedItem.PlantKey = PlantPot->GetPlantKey();
			SavedItem.PlantWaterLevel = PlantPot->GetWaterLevel();
			SavedItem.PlantGrowthProgress =
				PlantPot->GetGrowthProgress();
			SavedItem.PlantWateringCount =
				PlantPot->GetWateringCount();
		}
	}

	for (FConstPlayerControllerIterator ControllerIt =
			 World->GetPlayerControllerIterator();
		 ControllerIt;
		 ++ControllerIt)
	{
		CapturePlayerInventory(ControllerIt->Get());
		CapturePlayerEconomy(ControllerIt->Get());
	}

	const FString SlotName = GetAutosaveSlotName();
	const bool bSaved =
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SlotName, 0);
	if (bSaved)
	{
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Botanicus autosave completed: slot '%s', %d building actors, %d player inventories, %d player economies, %d paths, %d world items."),
			*SlotName,
			CurrentSaveGame->BuildingActors.Num(),
			CurrentSaveGame->PlayerInventories.Num(),
			CurrentSaveGame->PlayerEconomies.Num(),
			CurrentSaveGame->Paths.Num(),
			CurrentSaveGame->WorldItems.Num());
	}
	else
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Botanicus autosave failed for slot '%s'."),
			*SlotName);
	}
	return bSaved;
}

FString ABotanicusGameMode::GetAutosaveSlotName() const
{
	const FString MapName =
		UGameplayStatics::GetCurrentLevelName(this, true);
	return FString::Printf(TEXT("Botanicus_Autosave_%s"), *MapName);
}

FString ABotanicusGameMode::GetPlayerSaveKey(
	const AController* Controller)
{
	if (!Controller)
	{
		return FString();
	}

	const APlayerState* PlayerState = Controller->PlayerState;
	if (PlayerState)
	{
		// OnlineSubsystemUtils creates a new synthetic Unique Net ID every
		// time a PIE session starts. It looks valid, but using it as a save
		// key creates a fresh wallet and inventory on every editor launch.
		// Player IDs also keep incrementing across PIE restarts. Use only
		// topology that is recreated deterministically for every test run.
		if (const UWorld* World = GetWorld();
			World && World->WorldType == EWorldType::PIE)
		{
			const APlayerController* PlayerController =
				Cast<APlayerController>(Controller);
			if (PlayerController &&
				PlayerController->IsLocalPlayerController())
			{
				const ULocalPlayer* LocalPlayer =
					PlayerController->GetLocalPlayer();
				return FString::Printf(
					TEXT("PIELocal_%d"),
					LocalPlayer ? LocalPlayer->GetControllerId() : 0);
			}

			AController* MutableController =
				const_cast<AController*>(Controller);
			if (const int32* ExistingSlot =
				PIERemotePlayerSlots.Find(MutableController))
			{
				return FString::Printf(
					TEXT("PIEClient_%d"),
					*ExistingSlot);
			}

			for (auto SlotIt = PIERemotePlayerSlots.CreateIterator();
				 SlotIt;
				 ++SlotIt)
			{
				if (!SlotIt.Key().IsValid())
				{
					SlotIt.RemoveCurrent();
				}
			}
			const int32 NewSlot = NextPIERemotePlayerSlot++;
			PIERemotePlayerSlots.Add(MutableController, NewSlot);
			return FString::Printf(
				TEXT("PIEClient_%d"),
				NewSlot);
		}

		const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
		if (UniqueId.IsValid())
		{
			return UniqueId.ToString();
		}

		return FString::Printf(
			TEXT("LocalPlayer_%d"),
			PlayerState->GetPlayerId());
	}

	return Controller->GetName();
}

void ABotanicusGameMode::LoadAutosave()
{
	const FString SlotName = GetAutosaveSlotName();
	CurrentSaveGame =
		Cast<UBotanicusWorldSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SlotName, 0));

	if (!CurrentSaveGame)
	{
		CurrentSaveGame =
			Cast<UBotanicusWorldSaveGame>(
				UGameplayStatics::CreateSaveGameObject(
					UBotanicusWorldSaveGame::StaticClass()));
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("No Botanicus autosave found for slot '%s'; starting fresh."),
			*SlotName);
		return;
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Loaded Botanicus autosave '%s' (version %d): %d building actors, %d player inventories, %d player economies, %d paths, %d world items."),
		*SlotName,
		CurrentSaveGame->SaveVersion,
		CurrentSaveGame->BuildingActors.Num(),
		CurrentSaveGame->PlayerInventories.Num(),
		CurrentSaveGame->PlayerEconomies.Num(),
		CurrentSaveGame->Paths.Num(),
		CurrentSaveGame->WorldItems.Num());
}

void ABotanicusGameMode::RestoreWorldState()
{
	UWorld* World = GetWorld();
	if (!World || !CurrentSaveGame)
	{
		return;
	}

	TMap<FName, AActor*> BuildingActorsByName;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (IsValid(Actor) &&
			Actor->GetClass()->GetPathName().Contains(
				TEXT("/EasyBuildingSystem/Blueprints/BuildingObjects/")))
		{
			BuildingActorsByName.Add(Actor->GetFName(), Actor);
		}
	}

	RemovedBuildingActorNames.Reset();
	for (const FName RemovedActorName :
		 CurrentSaveGame->RemovedBuildingActorNames)
	{
		RemovedBuildingActorNames.Add(RemovedActorName);
		if (AActor* const* FoundActor =
			BuildingActorsByName.Find(RemovedActorName))
		{
			if (IsValid(*FoundActor))
			{
				(*FoundActor)->Destroy();
			}
			BuildingActorsByName.Remove(RemovedActorName);
		}
	}

	int32 RestoredCount = 0;
	for (const FBotanicusSavedBuildingActor& SavedActor :
		 CurrentSaveGame->BuildingActors)
	{
		AActor* const* FoundActor =
			BuildingActorsByName.Find(SavedActor.ActorName);
		AActor* RestoredActor =
			FoundActor && IsValid(*FoundActor) ? *FoundActor : nullptr;
		if (!RestoredActor &&
			SavedActor.bRuntimeSpawned &&
			!SavedActor.ActorClass.IsNull())
		{
			if (UClass* ActorClass =
				SavedActor.ActorClass.TryLoadClass<AActor>())
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = SavedActor.ActorName;
				SpawnParameters.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				RestoredActor = World->SpawnActor<AActor>(
					ActorClass,
					SavedActor.Transform,
					SpawnParameters);
				if (RestoredActor)
				{
					RestoredActor->Tags.AddUnique(PurchasedBuildingTag);
					ConfigureRestoredPurchasedActorForNetworking(
						RestoredActor);
					BuildingActorsByName.Add(
						RestoredActor->GetFName(),
						RestoredActor);
				}
			}
		}
		if (!RestoredActor)
		{
			continue;
		}

		RestoredActor->SetActorTransform(
			SavedActor.Transform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		++RestoredCount;
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Restored %d/%d saved building actors."),
		RestoredCount,
		CurrentSaveGame->BuildingActors.Num());

	for (TActorIterator<ABotanicusCommunicationDoorActor> DoorIt(World);
		 DoorIt;
		 ++DoorIt)
	{
		DoorIt->Destroy();
	}
	for (const FTransform& DoorTransform :
		 CurrentSaveGame->CommunicationDoors)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ABotanicusCommunicationDoorActor>(
			ABotanicusCommunicationDoorActor::StaticClass(),
			DoorTransform,
			SpawnParameters);
	}

	TArray<ABotanicusPathActor*> ExistingPaths;
	for (TActorIterator<ABotanicusPathActor> PathIt(World);
		 PathIt;
		 ++PathIt)
	{
		ExistingPaths.Add(*PathIt);
	}
	for (ABotanicusPathActor* ExistingPath : ExistingPaths)
	{
		if (IsValid(ExistingPath))
		{
			ExistingPath->Destroy();
		}
	}

	int32 RestoredPathCount = 0;
	for (const FBotanicusSavedPath& SavedPath : CurrentSaveGame->Paths)
	{
		if (SavedPath.Points.Num() < 2)
		{
			continue;
		}

		TArray<FVector> WorldPoints;
		WorldPoints.Reserve(SavedPath.Points.Num());
		for (const FVector_NetQuantize10& Point : SavedPath.Points)
		{
			WorldPoints.Add(FVector(Point));
		}

		ABotanicusPathActor* RestoredPath =
			World->SpawnActor<ABotanicusPathActor>();
		if (RestoredPath)
		{
			RestoredPath->InitializeConfirmedPath(WorldPoints);
			TArray<FVector> JunctionPoints;
			JunctionPoints.Reserve(SavedPath.JunctionPoints.Num());
			for (const FVector_NetQuantize10& JunctionPoint :
				 SavedPath.JunctionPoints)
			{
				JunctionPoints.Add(FVector(JunctionPoint));
			}
			RestoredPath->RestoreJunctionPoints(JunctionPoints);
			++RestoredPathCount;
		}
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Restored %d/%d saved paths."),
		RestoredPathCount,
		CurrentSaveGame->Paths.Num());

	TArray<AActor*> ExistingWorldItems;
	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(World);
		 ParcelIt;
		 ++ParcelIt)
	{
		ExistingWorldItems.Add(*ParcelIt);
	}
	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(World);
		 EquipmentIt;
		 ++EquipmentIt)
	{
		ExistingWorldItems.Add(*EquipmentIt);
	}
	for (TActorIterator<ABotanicusPlaceableItemActor> ItemIt(World);
		 ItemIt;
		 ++ItemIt)
	{
		ExistingWorldItems.Add(*ItemIt);
	}
	for (AActor* ExistingWorldItem : ExistingWorldItems)
	{
		if (IsValid(ExistingWorldItem))
		{
			ExistingWorldItem->Destroy();
		}
	}

	int32 RestoredWorldItemCount = 0;
	for (const FBotanicusSavedWorldItem& SavedItem :
		 CurrentSaveGame->WorldItems)
	{
		if (SavedItem.ActorClass.IsNull())
		{
			continue;
		}

		UClass* ItemClass = SavedItem.ActorClass.TryLoadClass<AActor>();
		if (!ItemClass ||
			(!ItemClass->IsChildOf(
				 ABotanicusDeliveryParcelActor::StaticClass()) &&
			 !ItemClass->IsChildOf(
				 ABotanicusLargeEquipmentActor::StaticClass()) &&
			 !ItemClass->IsChildOf(
				 ABotanicusPlaceableItemActor::StaticClass())))
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* RestoredItem = World->SpawnActor<AActor>(
			ItemClass,
			SavedItem.Transform,
			SpawnParameters);
		if (!RestoredItem)
		{
			continue;
		}

		if (ABotanicusDeliveryParcelActor* Parcel =
			Cast<ABotanicusDeliveryParcelActor>(RestoredItem))
		{
			Parcel->InitializeParcel(
				SavedItem.ItemKey,
				SavedItem.Quantity);
		}
		else if (ABotanicusPlaceableItemActor* PlacedItem =
			Cast<ABotanicusPlaceableItemActor>(RestoredItem))
		{
			PlacedItem->InitializePlacedItem(
				SavedItem.ItemKey,
				SavedItem.Quantity);
			if (ABotanicusPlantPotActor* PlantPot =
				Cast<ABotanicusPlantPotActor>(PlacedItem))
			{
				PlantPot->RestoreGrowingState(
					SavedItem.bPlantPotHasSoil,
					SavedItem.PlantKey,
					SavedItem.PlantWaterLevel,
					SavedItem.PlantGrowthProgress);
				PlantPot->RestoreWateringCount(
					SavedItem.PlantWateringCount);
			}
		}
		else if (ABotanicusLargeEquipmentActor* Equipment =
			Cast<ABotanicusLargeEquipmentActor>(RestoredItem))
		{
			Equipment->InitializeEquipment(SavedItem.ItemKey);
		}

		RestoredItem->SetOwner(nullptr);
		RestoredItem->SetNetDormancy(DORM_Awake);
		RestoredItem->FlushNetDormancy();
		RestoredItem->ForceNetUpdate();
		++RestoredWorldItemCount;
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Restored %d/%d saved world items."),
		RestoredWorldItemCount,
		CurrentSaveGame->WorldItems.Num());
}

void ABotanicusGameMode::CapturePlayerInventory(
	const AController* Controller)
{
	if (!CurrentSaveGame || !Controller)
	{
		return;
	}

	const ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Controller->GetPawn());
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	const FString PlayerKey = GetPlayerSaveKey(Controller);
	if (!QuickBar || PlayerKey.IsEmpty())
	{
		return;
	}

	FBotanicusSavedPlayerInventory* SavedInventory =
		CurrentSaveGame->PlayerInventories.FindByPredicate(
			[&PlayerKey](const FBotanicusSavedPlayerInventory& Entry)
			{
				return Entry.PlayerKey == PlayerKey;
			});
	if (!SavedInventory)
	{
		SavedInventory =
			&CurrentSaveGame->PlayerInventories.AddDefaulted_GetRef();
		SavedInventory->PlayerKey = PlayerKey;
	}

	SavedInventory->Slots = QuickBar->GetSlots();
	SavedInventory->SelectedSlotIndex =
		QuickBar->GetSelectedSlotIndex();
}

void ABotanicusGameMode::RestorePlayerInventory(AController* Controller)
{
	if (!CurrentSaveGame || !Controller)
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Controller->GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	const FString PlayerKey = GetPlayerSaveKey(Controller);
	if (!QuickBar || PlayerKey.IsEmpty())
	{
		return;
	}

	const FBotanicusSavedPlayerInventory* SavedInventory =
		CurrentSaveGame->PlayerInventories.FindByPredicate(
			[&PlayerKey](const FBotanicusSavedPlayerInventory& Entry)
			{
				return Entry.PlayerKey == PlayerKey;
			});
	if (!SavedInventory)
	{
		return;
	}

	QuickBar->ApplySavedState(
		SavedInventory->Slots,
		SavedInventory->SelectedSlotIndex);
}

void ABotanicusGameMode::CapturePlayerEconomy(
	const AController* Controller)
{
	if (!CurrentSaveGame || !Controller || !GetWorld())
	{
		return;
	}

	const ABotanicusPlayerController* BotanicusController =
		Cast<ABotanicusPlayerController>(Controller);
	const FString PlayerKey = GetPlayerSaveKey(Controller);
	if (!BotanicusController || PlayerKey.IsEmpty())
	{
		return;
	}

	FBotanicusSavedPlayerEconomy* SavedEconomy =
		CurrentSaveGame->PlayerEconomies.FindByPredicate(
			[&PlayerKey](const FBotanicusSavedPlayerEconomy& Entry)
			{
				return Entry.PlayerKey == PlayerKey;
			});
	if (!SavedEconomy)
	{
		SavedEconomy =
			&CurrentSaveGame->PlayerEconomies.AddDefaulted_GetRef();
		SavedEconomy->PlayerKey = PlayerKey;
	}

	SavedEconomy->AvailableFunds =
		BotanicusController->GetAvailableFunds();
	SavedEconomy->BuildingProgressionLevel =
		BotanicusController->GetBuildingProgressionLevel();
	SavedEconomy->PendingOrders.Reset();

	const AGameStateBase* CurrentGameState = GetWorld()->GetGameState();
	const float ServerTime = CurrentGameState
		? CurrentGameState->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds();
	for (const FBotanicusPendingOrder& PendingOrder :
		 BotanicusController->GetPendingOrders())
	{
		if (PendingOrder.ItemKey.IsNone())
		{
			continue;
		}

		FBotanicusSavedPendingOrder& SavedOrder =
			SavedEconomy->PendingOrders.AddDefaulted_GetRef();
		SavedOrder.OrderId = PendingOrder.OrderId;
		SavedOrder.ItemKey = PendingOrder.ItemKey;
		SavedOrder.Quantity = FMath::Max(1, PendingOrder.Quantity);
		SavedOrder.ChargedPrice =
			FMath::Max(0, PendingOrder.ChargedPrice);
		SavedOrder.RemainingDeliverySeconds =
			FMath::Max(
				0.1f,
				PendingOrder.DeliveryServerTime - ServerTime);
	}
}

void ABotanicusGameMode::RestorePlayerEconomy(
	AController* Controller)
{
	if (!CurrentSaveGame || !Controller)
	{
		return;
	}

	ABotanicusPlayerController* BotanicusController =
		Cast<ABotanicusPlayerController>(Controller);
	const FString PlayerKey = GetPlayerSaveKey(Controller);
	if (!BotanicusController || PlayerKey.IsEmpty())
	{
		return;
	}

	const FBotanicusSavedPlayerEconomy* SavedEconomy =
		CurrentSaveGame->SaveVersion >= 4
			? CurrentSaveGame->PlayerEconomies.FindByPredicate(
				[&PlayerKey](
					const FBotanicusSavedPlayerEconomy& Entry)
				{
					return Entry.PlayerKey == PlayerKey;
				})
			: nullptr;

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Economy lookup for %s uses save key '%s' (%s)."),
		Controller->PlayerState
			? *Controller->PlayerState->GetPlayerName()
			: TEXT("UnknownPlayer"),
		*PlayerKey,
		SavedEconomy ? TEXT("found") : TEXT("new"));

	TArray<FBotanicusPendingOrder> RestoredOrders;
	int32 RestoredFunds =
		BotanicusController->GetDefaultStartingFunds();
	int32 RestoredBuildingProgressionLevel = 1;
	if (SavedEconomy)
	{
		RestoredFunds = FMath::Max(0, SavedEconomy->AvailableFunds);
		if (CurrentSaveGame->SaveVersion >= 5)
		{
			RestoredBuildingProgressionLevel =
				FMath::Max(
					1,
					SavedEconomy->BuildingProgressionLevel);
		}
		RestoredOrders.Reserve(SavedEconomy->PendingOrders.Num());
		for (const FBotanicusSavedPendingOrder& SavedOrder :
			 SavedEconomy->PendingOrders)
		{
			FBotanicusPendingOrder& RestoredOrder =
				RestoredOrders.AddDefaulted_GetRef();
			RestoredOrder.OrderId = SavedOrder.OrderId;
			RestoredOrder.ItemKey = SavedOrder.ItemKey;
			RestoredOrder.Quantity = SavedOrder.Quantity;
			RestoredOrder.ChargedPrice = SavedOrder.ChargedPrice;
			// RestoreCatalogOrderState interprets this field as duration.
			RestoredOrder.DeliveryServerTime =
				SavedOrder.RemainingDeliverySeconds;
		}
	}

	// Version-4 saves can already contain a confirmed compact greenhouse even
	// though player progression did not exist yet. Preserve that achievement
	// during the one-time migration and grant the same level-up reward.
	if (CurrentSaveGame->SaveVersion == 4)
	{
		bool bHasLegacyCompactGreenhouse = false;
		for (TActorIterator<ABotanicusCompactGreenhouseActor> BuildingIt(
				 GetWorld());
			 BuildingIt;
			 ++BuildingIt)
		{
			if (BuildingIt->ActorHasTag(PurchasedBuildingTag))
			{
				bHasLegacyCompactGreenhouse = true;
				break;
			}
		}
		if (bHasLegacyCompactGreenhouse)
		{
			RestoredBuildingProgressionLevel = 2;
			RestoredFunds += 500;
			UE_LOG(
				LogBotanicus,
				Display,
				TEXT(
					"Migrated legacy compact greenhouse achievement to development level 2 with a 500-credit grant."));
		}
	}

	// Calling this even without a version-4 entry locks in the default starting
	// balance and prevents a later pawn restart from replaying stale save data.
	BotanicusController->RestoreCatalogOrderState(
		RestoredFunds,
		RestoredOrders,
		RestoredBuildingProgressionLevel);
}
