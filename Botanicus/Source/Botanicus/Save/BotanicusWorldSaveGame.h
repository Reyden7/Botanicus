// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "BotanicusWorldSaveGame.generated.h"

USTRUCT()
struct BOTANICUS_API FBotanicusSavedBuildingActor
{
	GENERATED_BODY()

	UPROPERTY()
	FName ActorName = NAME_None;

	UPROPERTY()
	FSoftClassPath ActorClass;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	bool bRuntimeSpawned = false;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedPlayerInventory
{
	GENERATED_BODY()

	UPROPERTY()
	FString PlayerKey;

	UPROPERTY()
	TArray<FBotanicusQuickBarSlot> Slots;

	UPROPERTY()
	int32 SelectedSlotIndex = 0;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedPendingOrder
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid OrderId;

	UPROPERTY()
	FName ItemKey = NAME_None;

	UPROPERTY()
	int32 Quantity = 1;

	UPROPERTY()
	int32 ChargedPrice = 0;

	/** Duration still to wait when the save was captured. */
	UPROPERTY()
	float RemainingDeliverySeconds = 0.1f;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedPlayerEconomy
{
	GENERATED_BODY()

	UPROPERTY()
	FString PlayerKey;

	UPROPERTY()
	int32 AvailableFunds = 0;

	UPROPERTY()
	int32 BuildingProgressionLevel = 1;

	UPROPERTY()
	TArray<FBotanicusSavedPendingOrder> PendingOrders;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedPath
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector_NetQuantize10> Points;

	UPROPERTY()
	TArray<FVector_NetQuantize10> JunctionPoints;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedWorldItem
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftClassPath ActorClass;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	FName ItemKey = NAME_None;

	UPROPERTY()
	int32 Quantity = 1;

	UPROPERTY()
	bool bPlantPotHasSoil = false;

	UPROPERTY()
	FName PlantKey = NAME_None;

	UPROPERTY()
	float PlantWaterLevel = 0.0f;

	UPROPERTY()
	float PlantGrowthProgress = 0.0f;

	UPROPERTY()
	int32 PlantWateringCount = 0;
};

/** Server-owned persistent state for one Botanicus map. */
UCLASS()
class BOTANICUS_API UBotanicusWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveVersion = 6;

	UPROPERTY()
	FString MapName;

	UPROPERTY()
	TArray<FBotanicusSavedBuildingActor> BuildingActors;

	UPROPERTY()
	TArray<FName> RemovedBuildingActorNames;

	UPROPERTY()
	TArray<FTransform> CommunicationDoors;

	UPROPERTY()
	TArray<FBotanicusSavedPlayerInventory> PlayerInventories;

	UPROPERTY()
	TArray<FBotanicusSavedPlayerEconomy> PlayerEconomies;

	UPROPERTY()
	TArray<FBotanicusSavedPath> Paths;

	UPROPERTY()
	TArray<FBotanicusSavedWorldItem> WorldItems;
};
