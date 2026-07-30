// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusGameMode.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Path/BotanicusPathActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Save/BotanicusWorldSaveGame.h"

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
	CurrentSaveGame->BuildingActors.Reset();
	CurrentSaveGame->Paths.Reset();

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
		SavedActor.Transform = Actor->GetActorTransform();
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
			TEXT("Botanicus autosave completed: slot '%s', %d building actors, %d player inventories, %d paths."),
			*SlotName,
			CurrentSaveGame->BuildingActors.Num(),
			CurrentSaveGame->PlayerInventories.Num(),
			CurrentSaveGame->Paths.Num());
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
		TEXT("Loaded Botanicus autosave '%s': %d building actors, %d player inventories, %d paths."),
		*SlotName,
		CurrentSaveGame->BuildingActors.Num(),
		CurrentSaveGame->PlayerInventories.Num(),
		CurrentSaveGame->Paths.Num());
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

	int32 RestoredCount = 0;
	for (const FBotanicusSavedBuildingActor& SavedActor :
		 CurrentSaveGame->BuildingActors)
	{
		AActor* const* FoundActor =
			BuildingActorsByName.Find(SavedActor.ActorName);
		if (!FoundActor || !IsValid(*FoundActor))
		{
			continue;
		}

		(*FoundActor)->SetActorTransform(
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
