// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusGameMode.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Building/BotanicusCommunicationDoorActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "EngineUtils.h"
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

void ABotanicusGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (HasAuthority())
	{
		RestorePlayerInventory(NewPlayer);
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
		CapturePlayerInventory(Exiting);
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
	CurrentSaveGame->SaveVersion = 3;
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
			!Actor->GetClass()->GetPathName().Contains(
				TEXT("/EasyBuildingSystem/Blueprints/BuildingObjects/")))
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
		SavedItem.Quantity = 1;
	}

	for (FConstPlayerControllerIterator ControllerIt =
			 World->GetPlayerControllerIterator();
		 ControllerIt;
		 ++ControllerIt)
	{
		CapturePlayerInventory(ControllerIt->Get());
	}

	const FString SlotName = GetAutosaveSlotName();
	const bool bSaved =
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SlotName, 0);
	if (bSaved)
	{
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Botanicus autosave completed: slot '%s', %d building actors, %d player inventories, %d paths, %d world items."),
			*SlotName,
			CurrentSaveGame->BuildingActors.Num(),
			CurrentSaveGame->PlayerInventories.Num(),
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
	const AController* Controller) const
{
	if (!Controller)
	{
		return FString();
	}

	const APlayerState* PlayerState = Controller->PlayerState;
	if (PlayerState)
	{
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
		TEXT("Loaded Botanicus autosave '%s': %d building actors, %d player inventories, %d paths, %d world items."),
		*SlotName,
		CurrentSaveGame->BuildingActors.Num(),
		CurrentSaveGame->PlayerInventories.Num(),
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
				 ABotanicusLargeEquipmentActor::StaticClass())))
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
