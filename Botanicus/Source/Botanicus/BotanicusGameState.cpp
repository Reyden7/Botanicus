// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusGameState.h"

#include "Catalog/BotanicusItemCatalog.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ABotanicusGameState::ABotanicusGameState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ABotanicusGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusGameState, SharedFunds);
	DOREPLIFETIME(ABotanicusGameState, MainShopLevel);
	DOREPLIFETIME(ABotanicusGameState, bMainShopOpen);
	DOREPLIFETIME(ABotanicusGameState, TotalPlantsSold);
	DOREPLIFETIME(ABotanicusGameState, TotalCatalogOrders);
	DOREPLIFETIME(ABotanicusGameState, ShopReputationPoints);
	DOREPLIFETIME(ABotanicusGameState, LastVisitorSatisfaction);
	DOREPLIFETIME(ABotanicusGameState, TotalVisitorReviews);
	DOREPLIFETIME(ABotanicusGameState, TrendColorTag);
	DOREPLIFETIME(ABotanicusGameState, TrendTypeTag);
	DOREPLIFETIME(ABotanicusGameState, TrendQualityTag);
	DOREPLIFETIME(ABotanicusGameState, TrendEndServerTime);
}

float ABotanicusGameState::GetTrendRemainingSeconds() const
{
	return FMath::Max(
		0.0f,
		TrendEndServerTime - GetServerWorldTimeSeconds());
}

int32 ABotanicusGameState::GetMainShopVisitorCapacity() const
{
	if (MainShopLevel <= 1)
	{
		return 4;
	}
	if (MainShopLevel == 2)
	{
		return 10;
	}
	return 20 + (MainShopLevel - 3) * 10;
}

int32 ABotanicusGameState::GetTargetVisitorPopulation() const
{
	if (MainShopLevel <= 1)
	{
		return 16;
	}
	if (MainShopLevel == 2)
	{
		return 28;
	}
	return 45 + (MainShopLevel - 3) * 15;
}

int32 ABotanicusGameState::GetMainShopUpgradeCost() const
{
	if (MainShopLevel <= 1)
	{
		return 1500;
	}
	if (MainShopLevel == 2)
	{
		return 5000;
	}
	return 5000 + (MainShopLevel - 2) * 5000;
}

int32 ABotanicusGameState::GetRequiredPlantSalesForUpgrade() const
{
	if (MainShopLevel <= 1)
	{
		return 3;
	}
	if (MainShopLevel == 2)
	{
		return 12;
	}
	return 12 + (MainShopLevel - 2) * 15;
}

int32 ABotanicusGameState::GetRequiredCatalogOrdersForUpgrade() const
{
	if (MainShopLevel <= 1)
	{
		return 5;
	}
	if (MainShopLevel == 2)
	{
		return 20;
	}
	return 20 + (MainShopLevel - 2) * 20;
}

int32 ABotanicusGameState::GetRequiredReputationForUpgrade() const
{
	if (MainShopLevel <= 1)
	{
		return 320;
	}
	if (MainShopLevel == 2)
	{
		return 380;
	}
	return FMath::Min(480, 420 + (MainShopLevel - 3) * 20);
}

bool ABotanicusGameState::AreMainShopUpgradeTasksComplete() const
{
	return TotalPlantsSold >= GetRequiredPlantSalesForUpgrade() &&
		TotalCatalogOrders >= GetRequiredCatalogOrdersForUpgrade() &&
		ShopReputationPoints >= GetRequiredReputationForUpgrade();
}

void ABotanicusGameState::InitializeSharedFunds(int32 InFunds)
{
	if (!HasAuthority() || bSharedFundsInitialized)
	{
		return;
	}

	bSharedFundsInitialized = true;
	SharedFunds = FMath::Max(0, InFunds);
	NotifyFundsChanged();
}

void ABotanicusGameState::InitializeMainShopProgression(
	int32 InShopLevel,
	int32 InTotalPlantsSold,
	int32 InTotalCatalogOrders)
{
	if (!HasAuthority() || bShopProgressionInitialized)
	{
		return;
	}

	bShopProgressionInitialized = true;
	MainShopLevel = FMath::Max(1, InShopLevel);
	TotalPlantsSold = FMath::Max(0, InTotalPlantsSold);
	TotalCatalogOrders = FMath::Max(0, InTotalCatalogOrders);
	NotifyFundsChanged();
}

void ABotanicusGameState::InitializeMainShopOpen(bool bInOpen)
{
	if (!HasAuthority() || bShopOpenInitialized)
	{
		return;
	}
	bShopOpenInitialized = true;
	bMainShopOpen = bInOpen;
	NotifyFundsChanged();
}

void ABotanicusGameState::SetMainShopOpen(bool bInOpen)
{
	if (!HasAuthority() || bMainShopOpen == bInOpen)
	{
		return;
	}
	bShopOpenInitialized = true;
	bMainShopOpen = bInOpen;
	NotifyFundsChanged();
}

void ABotanicusGameState::InitializeShopReputation(
	int32 InReputationPoints,
	int32 InLastVisitorSatisfaction,
	int32 InTotalVisitorReviews)
{
	if (!HasAuthority() || bShopReputationInitialized)
	{
		return;
	}
	bShopReputationInitialized = true;
	ShopReputationPoints =
		FMath::Clamp(InReputationPoints, 100, 500);
	LastVisitorSatisfaction =
		FMath::Clamp(InLastVisitorSatisfaction, 0, 100);
	TotalVisitorReviews = FMath::Max(0, InTotalVisitorReviews);
	NotifyFundsChanged();
}

void ABotanicusGameState::InitializeShopTrends(
	FName InColorTag,
	FName InTypeTag,
	FName InQualityTag,
	float InRemainingSeconds)
{
	if (!HasAuthority() || bShopTrendsInitialized)
	{
		return;
	}
	bShopTrendsInitialized = true;
	if (InColorTag.IsNone() ||
		InTypeTag.IsNone() ||
		InQualityTag.IsNone())
	{
		RotateShopTrends();
		return;
	}
	TrendColorTag = InColorTag;
	TrendTypeTag = InTypeTag;
	TrendQualityTag = InQualityTag;
	const float Remaining =
		FMath::Clamp(InRemainingSeconds, 1.0f, 600.0f);
	TrendEndServerTime = GetServerWorldTimeSeconds() + Remaining;
	GetWorldTimerManager().SetTimer(
		TrendRotationTimer,
		this,
		&ABotanicusGameState::RotateShopTrends,
		Remaining,
		false);
	NotifyFundsChanged();
}

int32 ABotanicusGameState::CountMatchingTrends(
	const FBotanicusItemDefinition& Definition) const
{
	int32 Matches = 0;
	Matches += Definition.PlantColorTag == TrendColorTag ? 1 : 0;
	Matches += Definition.PlantTypeTag == TrendTypeTag ? 1 : 0;
	Matches += Definition.PlantQualityTag == TrendQualityTag ? 1 : 0;
	return Matches;
}

int32 ABotanicusGameState::GetTrendAdjustedSalePrice(
	const FBotanicusItemDefinition& Definition) const
{
	const int32 Matches = CountMatchingTrends(Definition);
	const float Multiplier = 1.0f + Matches * 0.10f;
	return FMath::RoundToInt(
		FMath::Max(0, Definition.SalePrice) * Multiplier);
}

void ABotanicusGameState::RotateShopTrends()
{
	if (!HasAuthority())
	{
		return;
	}
	static const FName Colors[] = {
		TEXT("Green"),
		TEXT("Pink"),
		TEXT("Purple")};
	static const FName Types[] = {
		TEXT("Aromatic"),
		TEXT("Flowering"),
		TEXT("Foliage")};
	static const FName Qualities[] = {
		TEXT("Standard"),
		TEXT("Beautiful"),
		TEXT("Exceptional")};

	TrendColorTag =
		Colors[FMath::RandRange(0, UE_ARRAY_COUNT(Colors) - 1)];
	TrendTypeTag =
		Types[FMath::RandRange(0, UE_ARRAY_COUNT(Types) - 1)];
	TrendQualityTag =
		Qualities[FMath::RandRange(
			0,
			UE_ARRAY_COUNT(Qualities) - 1)];
	constexpr float TrendDurationSeconds = 600.0f;
	TrendEndServerTime =
		GetServerWorldTimeSeconds() + TrendDurationSeconds;
	GetWorldTimerManager().SetTimer(
		TrendRotationTimer,
		this,
		&ABotanicusGameState::RotateShopTrends,
		TrendDurationSeconds,
		false);
	NotifyFundsChanged();
}

bool ABotanicusGameState::TrySpendSharedFunds(int32 Amount)
{
	if (!HasAuthority() || Amount < 0 || SharedFunds < Amount)
	{
		return false;
	}

	SharedFunds -= Amount;
	NotifyFundsChanged();
	return true;
}

void ABotanicusGameState::AddSharedFunds(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}

	SharedFunds = FMath::Max(0, SharedFunds + Amount);
	NotifyFundsChanged();
}

void ABotanicusGameState::RecordPlantSale()
{
	if (!HasAuthority())
	{
		return;
	}
	++TotalPlantsSold;
	NotifyFundsChanged();
}

void ABotanicusGameState::RecordCatalogOrder()
{
	if (!HasAuthority())
	{
		return;
	}
	++TotalCatalogOrders;
	NotifyFundsChanged();
}

void ABotanicusGameState::RecordVisitorSatisfaction(
	int32 Satisfaction)
{
	if (!HasAuthority())
	{
		return;
	}
	LastVisitorSatisfaction = FMath::Clamp(Satisfaction, 0, 100);
	++TotalVisitorReviews;
	const int32 ReputationDelta =
		FMath::Clamp(
			FMath::RoundToInt(
				(LastVisitorSatisfaction - 60) * 0.16f),
			-5,
			7);
	ShopReputationPoints = FMath::Clamp(
		ShopReputationPoints + ReputationDelta,
		100,
		500);
	NotifyFundsChanged();
}

bool ABotanicusGameState::TryUpgradeMainShop()
{
	if (!HasAuthority() ||
		!AreMainShopUpgradeTasksComplete())
	{
		return false;
	}

	const int32 UpgradeCost = GetMainShopUpgradeCost();
	if (SharedFunds < UpgradeCost)
	{
		return false;
	}

	SharedFunds -= UpgradeCost;
	++MainShopLevel;
	NotifyFundsChanged();
	return true;
}

void ABotanicusGameState::OnRep_SharedFunds()
{
	NotifyFundsChanged();
}

void ABotanicusGameState::NotifyFundsChanged()
{
	ForceNetUpdate();
}
