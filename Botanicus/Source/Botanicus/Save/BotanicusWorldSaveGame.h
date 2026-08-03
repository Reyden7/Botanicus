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

	UPROPERTY()
	bool bHasPawnTransform = false;

	UPROPERTY()
	FTransform PawnTransform = FTransform::Identity;
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

	UPROPERTY()
	uint8 PathType = 0;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedVisitorZone
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	uint8 ZoneType = 1;

	UPROPERTY()
	FVector BoxExtent = FVector(500.0f, 400.0f, 8.0f);
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedRefundZone
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	FVector BoxExtent = FVector(220.0f, 150.0f, 6.0f);
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
	uint16 ParcelCutCoverageMask = 0;

	UPROPERTY()
	bool bParcelOpened = false;

	UPROPERTY()
	bool bPlantPotHasSoil = false;

	UPROPERTY()
	FName PlantKey = NAME_None;

	UPROPERTY()
	float PlantWaterLevel = 0.0f;

	UPROPERTY()
	float PlantGrowthProgress = 0.0f;

	UPROPERTY()
	float PlantCareScore = 0.0f;

	UPROPERTY()
	int32 PlantWateringCount = 0;

	UPROPERTY()
	int32 MultiPlanterSoilUnits = 0;

	UPROPERTY()
	TArray<FName> MultiPlanterPlantKeys;

	UPROPERTY()
	TArray<float> MultiPlanterWaterLevels;

	UPROPERTY()
	TArray<float> MultiPlanterGrowthProgress;

	UPROPERTY()
	TArray<float> MultiPlanterCareScores;

	UPROPERTY()
	TArray<int32> MultiPlanterWateringCounts;

	UPROPERTY()
	FName DisplayedPlantItemKey = NAME_None;

	UPROPERTY()
	float WateringCanWaterLevel = 1.0f;

	UPROPERTY()
	FName SalePotSoilItemKey = NAME_None;

	UPROPERTY()
	FName SalePotPlantItemKey = NAME_None;

	UPROPERTY()
	int32 PreparationWorkbenchLevel = 1;
};

/** Server-owned persistent state for one Botanicus map. */
UCLASS()
class BOTANICUS_API UBotanicusWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveVersion = 22;

	UPROPERTY()
	FString MapName;

	/** Nursery-wide wallet introduced in save version 10. */
	UPROPERTY()
	int32 SharedFunds = -1;

	UPROPERTY()
	int32 MainShopLevel = 1;

	UPROPERTY()
	bool bMainShopOpen = false;

	UPROPERTY()
	int32 CurrentDayNumber = 1;

	UPROPERTY()
	bool bShopDayActive = false;

	UPROPERTY()
	int32 DailyPlantsSold = 0;

	UPROPERTY()
	int32 DailyRevenue = 0;

	UPROPERTY()
	int32 DailySatisfactionTotal = 0;

	UPROPERTY()
	int32 DailyReviewCount = 0;

	UPROPERTY()
	int32 DayStartReputation = 300;

	UPROPERTY()
	float DayTimeMinutes = 420.0f;

	UPROPERTY()
	int32 TotalPlantsSold = 0;

	UPROPERTY()
	int32 TotalCatalogOrders = 0;

	UPROPERTY()
	int32 ShopReputationPoints = 300;

	UPROPERTY()
	int32 LastVisitorSatisfaction = 60;

	UPROPERTY()
	int32 TotalVisitorReviews = 0;

	UPROPERTY()
	FName TrendColorTag = NAME_None;

	UPROPERTY()
	FName TrendTypeTag = NAME_None;

	UPROPERTY()
	FName TrendQualityTag = NAME_None;

	UPROPERTY()
	float TrendRemainingSeconds = 600.0f;

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
	TArray<FBotanicusSavedVisitorZone> VisitorZones;

	UPROPERTY()
	TArray<FBotanicusSavedRefundZone> RefundZones;

	UPROPERTY()
	bool bHasDeliveryZone = false;

	UPROPERTY()
	FTransform DeliveryZoneTransform = FTransform::Identity;

	UPROPERTY()
	TArray<FBotanicusSavedWorldItem> WorldItems;
};
