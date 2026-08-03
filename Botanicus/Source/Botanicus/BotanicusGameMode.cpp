// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusGameMode.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "BotanicusPlayerController.h"
#include "Building/BotanicusCatalogBuildingActor.h"
#include "Building/BotanicusCommunicationDoorActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusDeliveryZoneActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Economy/BotanicusRefundZoneActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Path/BotanicusPathActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Save/BotanicusWorldSaveGame.h"
#include "TimerManager.h"
#include "Visitors/BotanicusVisitorManager.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "Preparation/BotanicusComputerActor.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"

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
	GameStateClass = ABotanicusGameState::StaticClass();
}

void ABotanicusGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	// Existing Blueprint game modes may still serialize the former base
	// GameState class. Enforce the shared Botanicus state before it is spawned.
	GameStateClass = ABotanicusGameState::StaticClass();
	Super::InitGame(MapName, Options, ErrorMessage);
}

void ABotanicusGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		LoadAutosave();
		InitializeSharedEconomy();
		RestoreWorldState();
		if (UWorld* World = GetWorld())
		{
			TActorIterator<ABotanicusVisitorManager> ManagerIt(World);
			if (!ManagerIt)
			{
				World->SpawnActor<ABotanicusVisitorManager>();
			}

			for (FConstPlayerControllerIterator ControllerIt =
					 World->GetPlayerControllerIterator();
				 ControllerIt;
				 ++ControllerIt)
			{
				// The listen-server pawn may have been restarted before
				// GameMode::BeginPlay loaded the autosave. Replay its
				// inventory/transform restoration now that data is ready.
				RestorePlayerInventory(ControllerIt->Get());
				RestorePlayerEconomy(ControllerIt->Get());
				EnsureStarterFixtures(ControllerIt->Get());
			}
		}
		bAutosaveReady = true;
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
		if (bAutosaveReady)
		{
			EnsureStarterFixtures(NewPlayer);
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
		if (ABotanicusCharacter* Character =
			Cast<ABotanicusCharacter>(Exiting->GetPawn()))
		{
			if (ABotanicusWateringCanActor* WateringCan =
				Character->GetHeldWateringCan())
			{
				// A handheld tool remains in the shared world when its
				// carrier disconnects and is then captured by the save.
				WateringCan->Drop();
			}
		}
		CapturePlayerInventory(Exiting);
		CapturePlayerEconomy(Exiting);
		BotanicusSaveNow();
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

void ABotanicusGameMode::ScheduleInventoryAutosave()
{
	if (!HasAuthority() || !bAutosaveReady ||
		bInventoryAutosaveScheduled || !GetWorld())
	{
		return;
	}
	bInventoryAutosaveScheduled = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusGameMode::FlushScheduledInventoryAutosave);
}

void ABotanicusGameMode::FlushScheduledInventoryAutosave()
{
	bInventoryAutosaveScheduled = false;
	BotanicusSaveNow();
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
	CurrentSaveGame->SaveVersion = 22;
	if (const ABotanicusGameState* BotanicusGameState =
		World->GetGameState<ABotanicusGameState>())
	{
		CurrentSaveGame->SharedFunds =
			BotanicusGameState->GetSharedFunds();
		CurrentSaveGame->MainShopLevel =
			BotanicusGameState->GetMainShopLevel();
		CurrentSaveGame->bMainShopOpen =
			BotanicusGameState->IsMainShopOpen();
		CurrentSaveGame->CurrentDayNumber =
			BotanicusGameState->GetCurrentDayNumber();
		CurrentSaveGame->bShopDayActive =
			BotanicusGameState->IsShopDayActive();
		CurrentSaveGame->DailyPlantsSold =
			BotanicusGameState->GetDailyPlantsSold();
		CurrentSaveGame->DailyRevenue =
			BotanicusGameState->GetDailyRevenue();
		CurrentSaveGame->DailySatisfactionTotal =
			BotanicusGameState->GetDailySatisfactionTotal();
		CurrentSaveGame->DailyReviewCount =
			BotanicusGameState->GetDailyReviewCount();
		CurrentSaveGame->DayStartReputation =
			BotanicusGameState->GetDayStartReputation();
		CurrentSaveGame->DayTimeMinutes =
			BotanicusGameState->GetDayTimeMinutes();
		CurrentSaveGame->TotalPlantsSold =
			BotanicusGameState->GetTotalPlantsSold();
		CurrentSaveGame->TotalCatalogOrders =
			BotanicusGameState->GetTotalCatalogOrders();
		CurrentSaveGame->ShopReputationPoints =
			BotanicusGameState->GetShopReputationPoints();
		CurrentSaveGame->LastVisitorSatisfaction =
			BotanicusGameState->GetLastVisitorSatisfaction();
		CurrentSaveGame->TotalVisitorReviews =
			BotanicusGameState->GetTotalVisitorReviews();
		CurrentSaveGame->TrendColorTag =
			BotanicusGameState->GetTrendColorTag();
		CurrentSaveGame->TrendTypeTag =
			BotanicusGameState->GetTrendTypeTag();
		CurrentSaveGame->TrendQualityTag =
			BotanicusGameState->GetTrendQualityTag();
		CurrentSaveGame->TrendRemainingSeconds =
			BotanicusGameState->GetTrendRemainingSeconds();
	}
	CurrentSaveGame->BuildingActors.Reset();
	CurrentSaveGame->Paths.Reset();
	CurrentSaveGame->VisitorZones.Reset();
	CurrentSaveGame->RefundZones.Reset();
	CurrentSaveGame->bHasDeliveryZone = false;
	CurrentSaveGame->DeliveryZoneTransform = FTransform::Identity;
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
		SavedPath.PathType =
			static_cast<uint8>(PathIt->GetPathType());
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

	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		FBotanicusSavedVisitorZone& SavedZone =
			CurrentSaveGame->VisitorZones.AddDefaulted_GetRef();
		SavedZone.Transform = ZoneIt->GetActorTransform();
		SavedZone.ZoneType =
			static_cast<uint8>(ZoneIt->GetZoneType());
		SavedZone.BoxExtent = ZoneIt->GetZoneExtent();
	}

	for (TActorIterator<ABotanicusRefundZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		FBotanicusSavedRefundZone& SavedZone =
			CurrentSaveGame->RefundZones.AddDefaulted_GetRef();
		SavedZone.Transform = ZoneIt->GetActorTransform();
		SavedZone.BoxExtent = ZoneIt->GetZoneExtent();
	}

	for (TActorIterator<ABotanicusDeliveryZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		CurrentSaveGame->bHasDeliveryZone = true;
		CurrentSaveGame->DeliveryZoneTransform =
			ZoneIt->GetActorTransform();
		break;
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
		SavedItem.ParcelCutCoverageMask =
			ParcelIt->GetCutCoverageMask();
		SavedItem.bParcelOpened = ParcelIt->IsOpened();
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
		if (const ABotanicusPreparationWorkbenchActor* Workbench =
				Cast<ABotanicusPreparationWorkbenchActor>(
					*EquipmentIt))
		{
			SavedItem.PreparationWorkbenchLevel =
				Workbench->GetWorkbenchLevel();
		}
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
		if (const ABotanicusMultiPlantPotActor* MultiPlanter =
			Cast<ABotanicusMultiPlantPotActor>(*ItemIt))
		{
			SavedItem.MultiPlanterSoilUnits =
				MultiPlanter->GetSoilUnits();
			for (const FBotanicusMultiPlantSlotState& Slot :
				MultiPlanter->GetPlantSlots())
			{
				SavedItem.MultiPlanterPlantKeys.Add(Slot.PlantKey);
				SavedItem.MultiPlanterWaterLevels.Add(
					Slot.WaterLevel);
				SavedItem.MultiPlanterGrowthProgress.Add(
					Slot.GrowthProgress);
				SavedItem.MultiPlanterCareScores.Add(
					Slot.CareScore);
				SavedItem.MultiPlanterWateringCounts.Add(
					Slot.WateringCount);
			}
		}
		else if (const ABotanicusPlantPotActor* PlantPot =
			Cast<ABotanicusPlantPotActor>(*ItemIt))
		{
			SavedItem.bPlantPotHasSoil = PlantPot->HasSoil();
			SavedItem.PlantKey = PlantPot->GetPlantKey();
			SavedItem.PlantWaterLevel = PlantPot->GetWaterLevel();
			SavedItem.PlantGrowthProgress =
				PlantPot->GetGrowthProgress();
			SavedItem.PlantCareScore =
				PlantPot->GetCareScore();
			SavedItem.PlantWateringCount =
				PlantPot->GetWateringCount();
		}
		else if (const ABotanicusSalesDisplayActor* SalesDisplay =
			Cast<ABotanicusSalesDisplayActor>(*ItemIt))
		{
			SavedItem.DisplayedPlantItemKey =
				SalesDisplay->GetDisplayedPlantItemKey();
		}
		else if (const ABotanicusWateringCanActor* WateringCan =
			Cast<ABotanicusWateringCanActor>(*ItemIt))
		{
			SavedItem.WateringCanWaterLevel =
				WateringCan->GetWaterLevel();
		}
		else if (const ABotanicusSalePotActor* SalePot =
			Cast<ABotanicusSalePotActor>(*ItemIt))
		{
			SavedItem.SalePotSoilItemKey =
				SalePot->GetSoilItemKey();
			SavedItem.SalePotPlantItemKey =
				SalePot->GetPlantItemKey();
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

void ABotanicusGameMode::InitializeSharedEconomy()
{
	UWorld* World = GetWorld();
	ABotanicusGameState* BotanicusGameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!BotanicusGameState || !CurrentSaveGame)
	{
		return;
	}

	int32 RestoredSharedFunds = 2000;
	if (CurrentSaveGame->SaveVersion >= 10 &&
		CurrentSaveGame->SharedFunds >= 0)
	{
		RestoredSharedFunds = CurrentSaveGame->SharedFunds;
	}
	else if (CurrentSaveGame->PlayerEconomies.Num() > 0)
	{
		// One-time migration: the first legacy wallet becomes the nursery
		// wallet. We deliberately do not add all players' balances together.
		RestoredSharedFunds =
			CurrentSaveGame->PlayerEconomies[0].AvailableFunds;
	}
	else
	{
		for (FConstPlayerControllerIterator ControllerIt =
				 World->GetPlayerControllerIterator();
			 ControllerIt;
			 ++ControllerIt)
		{
			if (const ABotanicusPlayerController* Controller =
				Cast<ABotanicusPlayerController>(ControllerIt->Get()))
			{
				RestoredSharedFunds =
					Controller->GetDefaultStartingFunds();
				break;
			}
		}
	}

	BotanicusGameState->InitializeSharedFunds(RestoredSharedFunds);
	BotanicusGameState->InitializeMainShopProgression(
		CurrentSaveGame->SaveVersion >= 12
			? CurrentSaveGame->MainShopLevel
			: 1,
		CurrentSaveGame->SaveVersion >= 12
			? CurrentSaveGame->TotalPlantsSold
			: 0,
		CurrentSaveGame->SaveVersion >= 12
			? CurrentSaveGame->TotalCatalogOrders
			: 0);
	BotanicusGameState->InitializeMainShopOpen(
		CurrentSaveGame->SaveVersion >= 17
			? CurrentSaveGame->bMainShopOpen
			: true);
	BotanicusGameState->InitializeShopReputation(
		CurrentSaveGame->SaveVersion >= 14
			? CurrentSaveGame->ShopReputationPoints
			: 300,
		CurrentSaveGame->SaveVersion >= 14
			? CurrentSaveGame->LastVisitorSatisfaction
			: 60,
		CurrentSaveGame->SaveVersion >= 14
			? CurrentSaveGame->TotalVisitorReviews
			: 0);
	BotanicusGameState->InitializeShopTrends(
		CurrentSaveGame->SaveVersion >= 15
			? CurrentSaveGame->TrendColorTag
			: NAME_None,
		CurrentSaveGame->SaveVersion >= 15
			? CurrentSaveGame->TrendTypeTag
			: NAME_None,
		CurrentSaveGame->SaveVersion >= 15
			? CurrentSaveGame->TrendQualityTag
			: NAME_None,
		CurrentSaveGame->SaveVersion >= 15
			? CurrentSaveGame->TrendRemainingSeconds
			: 600.0f);
	BotanicusGameState->InitializeDayCycle(
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->CurrentDayNumber
			: 1,
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->bShopDayActive
			: BotanicusGameState->IsMainShopOpen(),
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->DailyPlantsSold
			: 0,
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->DailyRevenue
			: 0,
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->DailySatisfactionTotal
			: 0,
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->DailyReviewCount
			: 0,
		CurrentSaveGame->SaveVersion >= 18
			? CurrentSaveGame->DayStartReputation
			: BotanicusGameState->GetShopReputationPoints(),
		CurrentSaveGame->SaveVersion >= 19
			? CurrentSaveGame->DayTimeMinutes
			: 420.0f);
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Shared nursery wallet initialized with %d credits."),
		FMath::Max(0, RestoredSharedFunds));
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
		const EBotanicusPathType RestoredPathType =
			CurrentSaveGame->SaveVersion >= 11 &&
				SavedPath.PathType ==
					static_cast<uint8>(
						EBotanicusPathType::VisitorRoute)
				? EBotanicusPathType::VisitorRoute
				: EBotanicusPathType::Standard;
		RestoredPath->InitializeConfirmedPath(
			WorldPoints,
			RestoredPathType);
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

	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		ZoneIt->Destroy();
	}
	if (CurrentSaveGame->SaveVersion >= 11)
	{
		for (const FBotanicusSavedVisitorZone& SavedZone :
			 CurrentSaveGame->VisitorZones)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABotanicusVisitorZoneActor* Zone =
				World->SpawnActor<ABotanicusVisitorZoneActor>(
					ABotanicusVisitorZoneActor::StaticClass(),
					SavedZone.Transform,
					SpawnParameters);
			if (Zone)
			{
				const EBotanicusVisitorZoneType ZoneType =
					static_cast<EBotanicusVisitorZoneType>(
						FMath::Clamp<int32>(
							SavedZone.ZoneType,
							0,
							2));
				FVector RestoredExtent = SavedZone.BoxExtent;
				if (ZoneType ==
					EBotanicusVisitorZoneType::Checkout)
				{
					// Migrate old saves whose checkout planning surface
					// only had room for the starter manual register.
					RestoredExtent.X =
						FMath::Max(RestoredExtent.X, 500.0f);
					RestoredExtent.Y =
						FMath::Max(RestoredExtent.Y, 300.0f);
				}
				Zone->InitializeZone(
					ZoneType,
					RestoredExtent);
			}
		}
	}

	for (TActorIterator<ABotanicusRefundZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		ZoneIt->Destroy();
	}
	if (CurrentSaveGame->SaveVersion >= 21)
	{
		for (const FBotanicusSavedRefundZone& SavedZone :
			 CurrentSaveGame->RefundZones)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABotanicusRefundZoneActor* Zone =
				World->SpawnActor<ABotanicusRefundZoneActor>(
					ABotanicusRefundZoneActor::StaticClass(),
					SavedZone.Transform,
					SpawnParameters);
			if (Zone)
			{
				Zone->InitializeZone(SavedZone.BoxExtent);
			}
		}
	}

	TArray<ABotanicusDeliveryZoneActor*> ExistingDeliveryZones;
	for (TActorIterator<ABotanicusDeliveryZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		ExistingDeliveryZones.Add(*ZoneIt);
	}
	if (CurrentSaveGame->SaveVersion >= 22 &&
		CurrentSaveGame->bHasDeliveryZone)
	{
		ABotanicusDeliveryZoneActor* DeliveryZone =
			ExistingDeliveryZones.IsEmpty()
				? nullptr
				: ExistingDeliveryZones[0];
		if (!DeliveryZone)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			DeliveryZone =
				World->SpawnActor<ABotanicusDeliveryZoneActor>(
					ABotanicusDeliveryZoneActor::StaticClass(),
					CurrentSaveGame->DeliveryZoneTransform,
					SpawnParameters);
		}
		else
		{
			DeliveryZone->SetActorTransform(
				CurrentSaveGame->DeliveryZoneTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}

		for (int32 Index = 1;
			 Index < ExistingDeliveryZones.Num();
			 ++Index)
		{
			if (IsValid(ExistingDeliveryZones[Index]))
			{
				ExistingDeliveryZones[Index]->Destroy();
			}
		}
	}
	else
	{
		// A delivery zone placed directly in the level is already a valid
		// starter zone. Only remove accidental duplicates.
		for (int32 Index = 1;
			 Index < ExistingDeliveryZones.Num();
			 ++Index)
		{
			if (IsValid(ExistingDeliveryZones[Index]))
			{
				ExistingDeliveryZones[Index]->Destroy();
			}
		}
	}

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
			Parcel->RestoreParcelState(
				SavedItem.ItemKey,
				SavedItem.Quantity,
				CurrentSaveGame->SaveVersion >= 16
					? SavedItem.ParcelCutCoverageMask
					: 0,
				CurrentSaveGame->SaveVersion >= 16
					? SavedItem.bParcelOpened
					: true);
		}
		else if (ABotanicusPlaceableItemActor* PlacedItem =
			Cast<ABotanicusPlaceableItemActor>(RestoredItem))
		{
			PlacedItem->InitializePlacedItem(
				SavedItem.ItemKey,
				SavedItem.Quantity);
			if (ABotanicusMultiPlantPotActor* MultiPlanter =
				Cast<ABotanicusMultiPlantPotActor>(PlacedItem))
			{
				TArray<FBotanicusMultiPlantSlotState> Slots;
				for (int32 Index = 0;
					 Index <
					 SavedItem.MultiPlanterPlantKeys.Num();
					 ++Index)
				{
					FBotanicusMultiPlantSlotState& Slot =
						Slots.AddDefaulted_GetRef();
					Slot.PlantKey =
						SavedItem.MultiPlanterPlantKeys[Index];
					Slot.WaterLevel =
						SavedItem.MultiPlanterWaterLevels.
							IsValidIndex(Index)
							? SavedItem.
								MultiPlanterWaterLevels[Index]
							: 0.0f;
					Slot.GrowthProgress =
						SavedItem.MultiPlanterGrowthProgress.
							IsValidIndex(Index)
							? SavedItem.
								MultiPlanterGrowthProgress[Index]
							: 0.0f;
					Slot.CareScore =
						SavedItem.MultiPlanterCareScores.
							IsValidIndex(Index)
							? SavedItem.
								MultiPlanterCareScores[Index]
							: 0.0f;
					Slot.WateringCount =
						SavedItem.MultiPlanterWateringCounts.
							IsValidIndex(Index)
							? SavedItem.
								MultiPlanterWateringCounts[Index]
							: 0;
				}
				MultiPlanter->RestoreMultiPlantState(
					SavedItem.MultiPlanterSoilUnits,
					Slots);
			}
			else if (ABotanicusPlantPotActor* PlantPot =
				Cast<ABotanicusPlantPotActor>(PlacedItem))
			{
				PlantPot->RestoreGrowingState(
					SavedItem.bPlantPotHasSoil,
					SavedItem.PlantKey,
					SavedItem.PlantWaterLevel,
					SavedItem.PlantGrowthProgress,
					SavedItem.PlantCareScore);
				PlantPot->RestoreWateringCount(
					SavedItem.PlantWateringCount);
			}
			else if (ABotanicusSalesDisplayActor* SalesDisplay =
				Cast<ABotanicusSalesDisplayActor>(PlacedItem))
			{
				SalesDisplay->RestoreDisplayedPlant(
					SavedItem.DisplayedPlantItemKey);
			}
			else if (ABotanicusWateringCanActor* WateringCan =
				Cast<ABotanicusWateringCanActor>(PlacedItem))
			{
				WateringCan->RestoreWaterLevel(
					SavedItem.WateringCanWaterLevel);
			}
			else if (ABotanicusSalePotActor* SalePot =
				Cast<ABotanicusSalePotActor>(PlacedItem))
			{
				SalePot->RestoreSalePotState(
					SavedItem.SalePotSoilItemKey,
					SavedItem.SalePotPlantItemKey);
			}
		}
		else if (ABotanicusLargeEquipmentActor* Equipment =
			Cast<ABotanicusLargeEquipmentActor>(RestoredItem))
		{
			Equipment->InitializeEquipment(SavedItem.ItemKey);
			if (ABotanicusPreparationWorkbenchActor* Workbench =
					Cast<
						ABotanicusPreparationWorkbenchActor>(
						Equipment))
			{
				Workbench->RestoreWorkbenchLevel(
					SavedItem.PreparationWorkbenchLevel);
			}
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

void ABotanicusGameMode::EnsureStarterFixtures(
	AController* Controller)
{
	UWorld* World = GetWorld();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!HasAuthority() || !World || !Pawn)
	{
		return;
	}

	FVector Forward = Pawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	const auto ResolveFloorLocation =
		[World, Pawn](
			FVector Candidate,
			float RootHeight)
		{
			FCollisionQueryParams FloorQuery(
				SCENE_QUERY_STAT(BotanicusStarterFixtureFloor),
				false,
				Pawn);
			FHitResult FloorHit;
			if (World->LineTraceSingleByChannel(
					FloorHit,
					Candidate + FVector(0.0f, 0.0f, 220.0f),
					Candidate - FVector(0.0f, 0.0f, 500.0f),
					ECC_Visibility,
					FloorQuery))
			{
				Candidate.Z = FloorHit.ImpactPoint.Z + RootHeight;
			}
			return Candidate;
		};
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::
			AdjustIfPossibleButAlwaysSpawn;
	bool bCreatedFixture = false;

	TActorIterator<ABotanicusDeliveryZoneActor> DeliveryZoneIt(World);
	const bool bHasDeliveryZone = !!DeliveryZoneIt;
	if (!bHasDeliveryZone)
	{
		const FVector Location = ResolveFloorLocation(
			Pawn->GetActorLocation() -
				Forward * 650.0f +
				Right * 500.0f,
			0.0f);
		if (World->SpawnActor<ABotanicusDeliveryZoneActor>(
				ABotanicusDeliveryZoneActor::StaticClass(),
				Location,
				Pawn->GetActorRotation(),
				SpawnParameters))
		{
			bCreatedFixture = true;
		}
	}

	bool bHasComputer = false;
	for (TActorIterator<ABotanicusComputerActor> It(World); It; ++It)
	{
		bHasComputer |= !It->ActorHasTag(
			TEXT("BotanicusPlacementPreview"));
	}
	if (!bHasComputer)
	{
		const FVector Location = ResolveFloorLocation(
			Pawn->GetActorLocation() +
				Forward * 190.0f +
				Right * 110.0f,
			42.0f);
		if (ABotanicusComputerActor* Computer =
				World->SpawnActor<ABotanicusComputerActor>(
					ABotanicusComputerActor::StaticClass(),
					Location,
					Pawn->GetActorRotation(),
					SpawnParameters))
		{
			Computer->InitializePlacedItem(
				TEXT("CommandComputer"),
				1);
			Computer->ForceNetUpdate();
			bCreatedFixture = true;
		}
	}

	bool bHasWorkbench = false;
	for (TActorIterator<ABotanicusPreparationWorkbenchActor> It(World);
		 It;
		 ++It)
	{
		bHasWorkbench |= !It->ActorHasTag(
			TEXT("BotanicusPlacementPreview"));
	}
	if (!bHasWorkbench)
	{
		const FVector Location = ResolveFloorLocation(
			Pawn->GetActorLocation() +
				Forward * 320.0f -
				Right * 170.0f,
			0.0f);
		if (ABotanicusPreparationWorkbenchActor* Workbench =
				World->SpawnActor<
					ABotanicusPreparationWorkbenchActor>(
					ABotanicusPreparationWorkbenchActor::
						StaticClass(),
					Location,
					Pawn->GetActorRotation(),
					SpawnParameters))
		{
			Workbench->InitializeEquipment(
				TEXT("PreparationWorkbench"));
			Workbench->ForceNetUpdate();
			bCreatedFixture = true;
		}
	}

	bool bHasCashRegister = false;
	for (TActorIterator<ABotanicusCashRegisterActor> It(World);
		 It;
		 ++It)
	{
		bHasCashRegister |= !It->ActorHasTag(
			TEXT("BotanicusPlacementPreview"));
	}
	if (!bHasCashRegister)
	{
		FVector RegisterLocation =
			Pawn->GetActorLocation() +
			Forward * 320.0f +
			Right * 210.0f;
		FRotator RegisterRotation = Pawn->GetActorRotation();
		for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
			 ZoneIt;
			 ++ZoneIt)
		{
			if (ZoneIt->GetZoneType() ==
				EBotanicusVisitorZoneType::Checkout)
			{
				RegisterRotation = ZoneIt->GetActorRotation();
				RegisterLocation =
					ZoneIt->GetActorLocation() -
					ZoneIt->GetActorForwardVector() * 155.0f;
				break;
			}
		}
		RegisterLocation =
			ResolveFloorLocation(RegisterLocation, 0.0f);
		if (ABotanicusCashRegisterActor* CashRegister =
				World->SpawnActor<ABotanicusCashRegisterActor>(
					ABotanicusCashRegisterActor::StaticClass(),
					RegisterLocation,
					RegisterRotation,
					SpawnParameters))
		{
			CashRegister->InitializePlacedItem(
				TEXT("CashRegister"),
				1);
			CashRegister->ForceNetUpdate();
			bCreatedFixture = true;
		}
	}

	if (bCreatedFixture)
	{
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Created missing starter shop fixtures."));
		ScheduleInventoryAutosave();
	}
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
	if (const APawn* Pawn = Controller->GetPawn())
	{
		SavedInventory->bHasPawnTransform = true;
		SavedInventory->PawnTransform = Pawn->GetActorTransform();
	}

	int32 NonEmptySlotCount = 0;
	int32 TotalItemQuantity = 0;
	for (const FBotanicusQuickBarSlot& Slot : SavedInventory->Slots)
	{
		if (!Slot.IsEmpty())
		{
			++NonEmptySlotCount;
			TotalItemQuantity += Slot.Quantity;
		}
	}
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Captured inventory for '%s': %d occupied slot(s), %d total item(s), position=%s."),
		*PlayerKey,
		NonEmptySlotCount,
		TotalItemQuantity,
		SavedInventory->bHasPawnTransform
			? *SavedInventory->PawnTransform.GetLocation().ToCompactString()
			: TEXT("none"));
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
	const auto GrantStarterCutter =
		[QuickBar]()
		{
			const bool bAlreadyHasCutter =
				QuickBar->GetSlots().ContainsByPredicate(
					[](const FBotanicusQuickBarSlot& Slot)
					{
						return !Slot.IsEmpty() &&
							Slot.ItemKey == TEXT("BoxCutter");
					});
			if (!bAlreadyHasCutter)
			{
				int32 CutterSlot = INDEX_NONE;
				if (QuickBar->AddItem(
					TEXT("BoxCutter"),
					1,
					CutterSlot))
				{
					UE_LOG(
						LogBotanicus,
						Display,
						TEXT(
							"Added the starter cutter to quickbar slot %d."),
						CutterSlot + 1);
				}
				else
				{
					UE_LOG(
						LogBotanicus,
						Warning,
						TEXT(
							"Could not add the starter cutter: the quickbar has no free slot."));
				}
			}
		};

	FBotanicusSavedPlayerInventory* SavedInventory =
		CurrentSaveGame->PlayerInventories.FindByPredicate(
			[&PlayerKey](const FBotanicusSavedPlayerInventory& Entry)
			{
				return Entry.PlayerKey == PlayerKey;
			});
	if (!SavedInventory)
	{
		GrantStarterCutter();
		return;
	}
	if (SavedInventory->bHasPawnTransform)
	{
		Character->SetActorTransform(
			SavedInventory->PawnTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	// Version 7 and earlier stored the watering can as a hotbar item.
	// Convert it once into its version-8 physical world representation.
	if (CurrentSaveGame->SaveVersion < 8 && GetWorld())
	{
		int32 LegacyWateringCanCount = 0;
		for (FBotanicusQuickBarSlot& SavedSlot : SavedInventory->Slots)
		{
			if (SavedSlot.ItemKey == TEXT("WateringCan") &&
				SavedSlot.Quantity > 0)
			{
				LegacyWateringCanCount += SavedSlot.Quantity;
				SavedSlot = FBotanicusQuickBarSlot();
			}
		}

		for (int32 CanIndex = 0;
			 CanIndex < LegacyWateringCanCount;
			 ++CanIndex)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::
					AdjustIfPossibleButAlwaysSpawn;
			const FVector SpawnLocation =
				Character->GetActorLocation() +
				Character->GetActorForwardVector() * 110.0f +
				Character->GetActorRightVector() *
					(static_cast<float>(CanIndex) * 45.0f) +
				FVector(0.0f, 0.0f, 25.0f);
			if (ABotanicusWateringCanActor* WateringCan =
				GetWorld()->SpawnActor<ABotanicusWateringCanActor>(
					SpawnLocation,
					FRotator::ZeroRotator,
					SpawnParameters))
			{
				WateringCan->InitializePlacedItem(
					TEXT("WateringCan"),
					1);
			}
		}
		if (LegacyWateringCanCount > 0)
		{
			UE_LOG(
				LogBotanicus,
				Display,
				TEXT(
					"Migrated %d legacy hotbar watering can(s) into physical world tools."),
				LegacyWateringCanCount);
		}
	}

	QuickBar->ApplySavedState(
		SavedInventory->Slots,
		SavedInventory->SelectedSlotIndex);
	// Always ensure that every player owns the cutter required to open
	// delivery cartons. This also repairs version-16 saves made before the
	// cutter could be inserted into an available quickbar slot.
	GrantStarterCutter();

	int32 RestoredSlotCount = 0;
	int32 RestoredItemQuantity = 0;
	for (const FBotanicusQuickBarSlot& Slot : QuickBar->GetSlots())
	{
		if (!Slot.IsEmpty())
		{
			++RestoredSlotCount;
			RestoredItemQuantity += Slot.Quantity;
		}
	}
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Restored inventory for '%s': %d occupied slot(s), %d total item(s), position=%s."),
		*PlayerKey,
		RestoredSlotCount,
		RestoredItemQuantity,
		SavedInventory->bHasPawnTransform
			? *SavedInventory->PawnTransform.GetLocation().ToCompactString()
			: TEXT("none"));
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
