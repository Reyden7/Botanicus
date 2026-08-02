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

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Botanicus|Economy")
	int32 GetSharedFunds() const { return SharedFunds; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopLevel() const { return MainShopLevel; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	bool IsMainShopOpen() const { return bMainShopOpen; }

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
	void SetMainShopOpen(bool bInOpen);

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

	void RecordPlantSale();
	void RecordCatalogOrder();
	void RecordVisitorSatisfaction(int32 Satisfaction);
	bool TryUpgradeMainShop();

private:
	UFUNCTION()
	void OnRep_SharedFunds();

	void NotifyFundsChanged();
	void RotateShopTrends();

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 SharedFunds = 0;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	int32 MainShopLevel = 1;

	UPROPERTY(ReplicatedUsing=OnRep_SharedFunds)
	bool bMainShopOpen = true;

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
	bool bShopReputationInitialized = false;
	bool bShopTrendsInitialized = false;
	FTimerHandle TrendRotationTimer;
};
