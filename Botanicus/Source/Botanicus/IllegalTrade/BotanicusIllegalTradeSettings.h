// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusIllegalTradeSettings.generated.h"

class UStaticMesh;
class ABotanicusIllegalCustomerCharacter;

DECLARE_LOG_CATEGORY_EXTERN(LogBotanicusIllegalTrade, Log, All);

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusIllegalPlantDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FName PlantId = TEXT("Noctiflore");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName = NSLOCTEXT("BotanicusIllegalTrade", "Noctiflore", "Noctiflore");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FName SeedItemKey = TEXT("SeedPacket_Noctiflore");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FName ProductItemKey = TEXT("IllegalPlantProduct_Noctiflore");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth", meta=(ClampMin="1.0", Units="s"))
	float GrowthDurationSeconds = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth", meta=(ClampMin="0.0"))
	float WaterConsumptionPerSecond = 0.0008f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.1", Units="cm"))
	float MinimumGrowthVisualHeight = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.1", Units="cm"))
	float MatureGrowthVisualHeight = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest", meta=(ClampMin="1"))
	int32 HarvestQuantity = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest", meta=(ClampMin="0"))
	int32 UnitSaleValue = 150;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals")
	TSoftObjectPtr<UStaticMesh> SeedlingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals")
	TSoftObjectPtr<UStaticMesh> YoungMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals")
	TSoftObjectPtr<UStaticMesh> MatureMesh;
};

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusIllegalPlanterLevelDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Planter", meta=(ClampMin="1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Planter", meta=(ClampMin="1", ClampMax="12"))
	int32 SlotCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Planter", meta=(ClampMin="1"))
	int32 RequiredSoilUnits = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Planter", meta=(ClampMin="0.1"))
	/** Maximum water reserve of each plant slot. */
	float MaximumWater = 1.0f;
};

/** Data-driven tuning exposed in Project Settings > Game > Illegal Trade. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Illegal Trade"))
class BOTANICUS_API UBotanicusIllegalTradeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBotanicusIllegalTradeSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	const FBotanicusIllegalPlantDefinition* FindPlant(FName PlantId) const;
	const FBotanicusIllegalPlantDefinition* FindPlantBySeed(FName SeedItemKey) const;
	const FBotanicusIllegalPlanterLevelDefinition& GetPlanterLevel(int32 Level) const;
	bool IsIllegalTradeMinute(float DayMinute) const;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Plants")
	TArray<FBotanicusIllegalPlantDefinition> Plants;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Planter")
	TArray<FBotanicusIllegalPlanterLevelDefinition> PlanterLevels;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Planter", meta=(ClampMin="0.01"))
	float WaterPerInteraction = 0.25f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Schedule", meta=(ClampMin="0.0", ClampMax="1439.0"))
	float TradeStartMinute = 1260.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Schedule", meta=(ClampMin="0.0", ClampMax="1439.0"))
	float TradeEndMinute = 300.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Schedule", meta=(ClampMin="0.0", ClampMax="1439.0"))
	float ShopOpenMinute = 480.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Schedule", meta=(ClampMin="0.0", ClampMax="1439.0"))
	float ShopCloseMinute = 1140.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="1.0", Units="s"))
	float FirstCustomerDelaySeconds = 8.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="1.0", Units="s"))
	float MinimumSpawnDelaySeconds = 20.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="1.0", Units="s"))
	float MaximumSpawnDelaySeconds = 45.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="1", ClampMax="8"))
	int32 MaximumConcurrentCustomers = 1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers")
	TSoftClassPtr<ABotanicusIllegalCustomerCharacter> CustomerClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="1.0", Units="s"))
	float CustomerWaitSeconds = 120.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="1"))
	int32 CustomerOrderQuantity = 3;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="0"))
	int32 SuspicionPerSale = 8;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Customers", meta=(ClampMin="0"))
	int32 SuspicionOnIgnoredCustomer = 0;
};
