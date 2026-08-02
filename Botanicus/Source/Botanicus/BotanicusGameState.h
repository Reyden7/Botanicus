// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BotanicusGameState.generated.h"

struct FBotanicusItemDefinition;

/** Replicated, server-authoritative state shared by the whole nursery. */
UCLASS()
class BOTANICUS_API ABotanicusGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABotanicusGameState();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Botanicus|Economy")
	int32 GetSharedFunds() const { return SharedFunds; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopLevel() const { return MainShopLevel; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	bool IsMainShopOpen() const { return bMainShopOpen; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	int32 GetCurrentDayNumber() const { return CurrentDayNumber; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	bool IsShopDayActive() const { return bShopDayActive; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	int32 GetDailyPlantsSold() const { return DailyPlantsSold; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	int32 GetDailyRevenue() const { return DailyRevenue; }
	int32 GetDailySatisfactionTotal() const
	{
		return DailySatisfactionTotal;
	}
	int32 GetDailyReviewCount() const { return DailyReviewCount; }
	int32 GetDayStartReputation() const
	{
		return DayStartReputation;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	int32 GetDailySalesTarget() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	int32 GetDailyRevenueTarget() const;

	int32 GetDaySummaryRevision() const { return DaySummaryRevision; }
	int32 GetLastCompletedDayNumber() const
	{
		return LastCompletedDayNumber;
	}
	int32 GetLastDayPlantsSold() const { return LastDayPlantsSold; }
	int32 GetLastDayRevenue() const { return LastDayRevenue; }
	int32 GetLastDayAverageSatisfaction() const
	{
		return LastDayAverageSatisfaction;
	}
	int32 GetLastDayReviewCount() const { return LastDayReviewCount; }
	int32 GetLastDayReputationDelta() const
	{
		return LastDayReputationDelta;
	}
	int32 GetLastDaySalesTarget() const { return LastDaySalesTarget; }
	int32 GetLastDayRevenueTarget() const
	{
		return LastDayRevenueTarget;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Development")
	float GetDevelopmentTimeScale() const
	{
		return DevelopmentTimeScale;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Day")
	float GetDayTimeMinutes() const { return DayTimeMinutes; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTotalPlantsSold() const { return TotalPlantsSold; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTotalCatalogOrders() const { return TotalCatalogOrders; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetShopReputationPoints() const { return ShopReputationPoints; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	float GetShopReputationStars() const
	{
		return ShopReputationPoints / 100.0f;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetLastVisitorSatisfaction() const
	{
		return LastVisitorSatisfaction;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTotalVisitorReviews() const { return TotalVisitorReviews; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Trends")
	FName GetTrendColorTag() const { return TrendColorTag; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Trends")
	FName GetTrendTypeTag() const { return TrendTypeTag; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Trends")
	FName GetTrendQualityTag() const { return TrendQualityTag; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Trends")
	float GetTrendRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopVisitorCapacity() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTargetVisitorPopulation() const;

	int32 GetMainShopUpgradeCost() const;
	int32 GetRequiredPlantSalesForUpgrade() const;
	int32 GetRequiredCatalogOrdersForUpgrade() const;
	int32 GetRequiredReputationForUpgrade() const;
	bool AreMainShopUpgradeTasksComplete() const;

	/** Initializes the shared wallet once after the autosave is loaded. */
	void InitializeSharedFunds(int32 InFunds);

	/** Initializes the shared shop progression after the autosave is loaded. */
	void InitializeMainShopProgression(
		int32 InShopLevel,
		int32 InTotalPlantsSold,
		int32 InTotalCatalogOrders);

	void InitializeMainShopOpen(bool bInOpen);
	void InitializeDayCycle(
		int32 InCurrentDayNumber,
		bool bInDayActive,
		int32 InDailyPlantsSold,
		int32 InDailyRevenue,
		int32 InDailySatisfactionTotal,
		int32 InDailyReviewCount,
		int32 InDayStartReputation,
		float InDayTimeMinutes);
	void SetMainShopOpen(bool bInOpen);
	void SetDevelopmentTimeScale(float InTimeScale);
	void SetMainShopLevelForDevelopment(int32 InLevel);

	void InitializeShopReputation(
		int32 InReputationPoints,
		int32 InLastVisitorSatisfaction,
		int32 InTotalVisitorReviews);

	void InitializeShopTrends(
		FName InColorTag,
		FName InTypeTag,
		FName InQualityTag,
		float InRemainingSeconds);

	int32 CountMatchingTrends(
		const FBotanicusItemDefinition& Definition) const;
	int32 GetTrendAdjustedSalePrice(
		const FBotanicusItemDefinition& Definition) const;

	/** Server-only atomic debit. */
	bool TrySpendSharedFunds(int32 Amount);

	/** Server-only credit used by sales, rewards and refunds. */
	void AddSharedFunds(int32 Amount);

	void RecordPlantSale(int32 SaleRevenue);
	void RecordCatalogOrder();
	void RecordVisitorSatisfaction(int32 Satisfaction);
	bool TryUpgradeMainShop();

private:
	UFUNCTION()
	void OnRep_SharedFunds();

	UFUNCTION()
	void OnRep_DevelopmentTimeScale();

	void NotifyFundsChanged();
	void RotateShopTrends();
	void BeginShopDay();
	void FinishShopDay();
	void ApplyDevelopmentTimeScale();
	bool DidClockCrossMinute(
		float PreviousMinute,
		float CurrentMinute,
		float TargetMinute) const;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 SharedFunds = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 MainShopLevel = 1;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	bool bMainShopOpen = false;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 CurrentDayNumber = 1;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	bool bShopDayActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 DailyPlantsSold = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 DailyRevenue = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 DailySatisfactionTotal = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 DailyReviewCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 DayStartReputation = 300;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 DaySummaryRevision = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastCompletedDayNumber = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDayPlantsSold = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDayRevenue = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDayAverageSatisfaction = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDayReviewCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDayReputationDelta = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDaySalesTarget = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastDayRevenueTarget = 0;

	UPROPERTY(ReplicatedUsing=OnRep_DevelopmentTimeScale)
	float DevelopmentTimeScale = 1.0f;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	float DayTimeMinutes = 420.0f;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 TotalPlantsSold = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 TotalCatalogOrders = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 ShopReputationPoints = 300;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 LastVisitorSatisfaction = 60;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 TotalVisitorReviews = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	FName TrendColorTag = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	FName TrendTypeTag = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	FName TrendQualityTag = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	float TrendEndServerTime = 0.0f;

	bool bSharedFundsInitialized = false;
	bool bShopProgressionInitialized = false;
	bool bShopOpenInitialized = false;
	bool bDayCycleInitialized = false;
	bool bShopReputationInitialized = false;
	bool bShopTrendsInitialized = false;
	FTimerHandle TrendRotationTimer;
};
