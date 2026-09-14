// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusGameState.h"

#include "BotanicusCharacter.h"
#include "SpecialOrders/BotanicusSpecialOrderComponent.h"
#include "BotanicusGameMode.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ABotanicusGameState::ABotanicusGameState()
{
	SpecialOrders = CreateDefaultSubobject<UBotanicusSpecialOrderComponent>(TEXT("SpecialOrders"));
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetNetUpdateFrequency(10.0f);
}

void ABotanicusGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	const float PreviousMinute = DayTimeMinutes;
	DayTimeMinutes =
		FMath::Fmod(
			DayTimeMinutes + FMath::Max(0.0f, DeltaSeconds),
			1440.0f);
	RefreshOutdoorEnvironment();
	if (DidClockCrossMinute(
			PreviousMinute,
			DayTimeMinutes,
			480.0f))
	{
		SetMainShopOpen(true);
	}
	if (DidClockCrossMinute(
			PreviousMinute,
			DayTimeMinutes,
			1140.0f))
	{
		SetMainShopOpen(false);
	}
}

void ABotanicusGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusGameState, SharedFunds);
	DOREPLIFETIME(ABotanicusGameState, MainShopLevel);
	DOREPLIFETIME(ABotanicusGameState, bMainShopOpen);
	DOREPLIFETIME(ABotanicusGameState, CurrentDayNumber);
	DOREPLIFETIME(ABotanicusGameState, bShopDayActive);
	DOREPLIFETIME(ABotanicusGameState, DailyPlantsSold);
	DOREPLIFETIME(ABotanicusGameState, DailyRevenue);
	DOREPLIFETIME(ABotanicusGameState, DailySatisfactionTotal);
	DOREPLIFETIME(ABotanicusGameState, DailyReviewCount);
	DOREPLIFETIME(ABotanicusGameState, DayStartReputation);
	DOREPLIFETIME(ABotanicusGameState, DaySummaryRevision);
	DOREPLIFETIME(ABotanicusGameState, LastCompletedDayNumber);
	DOREPLIFETIME(ABotanicusGameState, LastDayPlantsSold);
	DOREPLIFETIME(ABotanicusGameState, LastDayRevenue);
	DOREPLIFETIME(
		ABotanicusGameState,
		LastDayAverageSatisfaction);
	DOREPLIFETIME(ABotanicusGameState, LastDayReviewCount);
	DOREPLIFETIME(ABotanicusGameState, LastDayReputationDelta);
	DOREPLIFETIME(ABotanicusGameState, LastDaySalesTarget);
	DOREPLIFETIME(ABotanicusGameState, LastDayRevenueTarget);
	DOREPLIFETIME(ABotanicusGameState, DevelopmentTimeScale);
	DOREPLIFETIME(ABotanicusGameState, DayTimeMinutes);
	DOREPLIFETIME(ABotanicusGameState, OutdoorEnvironment);
	DOREPLIFETIME(ABotanicusGameState, DiscoveredDiseaseKeys);
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

void ABotanicusGameState::RegisterDiscoveredDisease(FName DiseaseKey)
{
	if (!HasAuthority() || DiseaseKey.IsNone() ||
		DiscoveredDiseaseKeys.Contains(DiseaseKey))
	{
		return;
	}
	DiscoveredDiseaseKeys.Add(DiseaseKey);
	ForceNetUpdate();
}

void ABotanicusGameState::InitializeDiscoveredDiseases(
	const TArray<FName>& DiseaseKeys)
{
	if (!HasAuthority())
	{
		return;
	}
	DiscoveredDiseaseKeys.Reset();
	for (const FName DiseaseKey : DiseaseKeys)
	{
		if (!DiseaseKey.IsNone())
		{
			DiscoveredDiseaseKeys.AddUnique(DiseaseKey);
		}
	}
	ForceNetUpdate();
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

int32 ABotanicusGameState::GetDailySalesTarget() const
{
	return 3 + FMath::Max(0, MainShopLevel - 1) * 3;
}

int32 ABotanicusGameState::GetDailyRevenueTarget() const
{
	return 180 + FMath::Max(0, MainShopLevel - 1) * 320;
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

void ABotanicusGameState::InitializeDayCycle(
	int32 InCurrentDayNumber,
	bool bInDayActive,
	int32 InDailyPlantsSold,
	int32 InDailyRevenue,
	int32 InDailySatisfactionTotal,
	int32 InDailyReviewCount,
	int32 InDayStartReputation,
	float InDayTimeMinutes)
{
	if (!HasAuthority() || bDayCycleInitialized)
	{
		return;
	}
	bDayCycleInitialized = true;
	CurrentDayNumber = FMath::Max(1, InCurrentDayNumber);
	bShopDayActive = bInDayActive && bMainShopOpen;
	DailyPlantsSold = FMath::Max(0, InDailyPlantsSold);
	DailyRevenue = FMath::Max(0, InDailyRevenue);
	DailySatisfactionTotal =
		FMath::Max(0, InDailySatisfactionTotal);
	DailyReviewCount = FMath::Max(0, InDailyReviewCount);
	DayStartReputation =
		FMath::Clamp(InDayStartReputation, 100, 500);
	DayTimeMinutes =
		FMath::Fmod(
			FMath::Max(0.0f, InDayTimeMinutes),
			1440.0f);
	RefreshOutdoorEnvironment();
	if (!bShopDayActive)
	{
		DailyPlantsSold = 0;
		DailyRevenue = 0;
		DailySatisfactionTotal = 0;
		DailyReviewCount = 0;
		DayStartReputation = ShopReputationPoints;
	}
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
	if (bMainShopOpen)
	{
		if (!bShopDayActive)
		{
			BeginShopDay();
		}
	}
	else if (bShopDayActive)
	{
		FinishShopDay();
	}
	NotifyFundsChanged();
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator ControllerIt =
				 World->GetPlayerControllerIterator();
			 ControllerIt;
			 ++ControllerIt)
		{
			if (APlayerController* Controller = ControllerIt->Get())
			{
				const int32 ClockHour =
					FMath::FloorToInt(DayTimeMinutes / 60.0f) %
					24;
				const int32 ClockMinute =
					FMath::FloorToInt(DayTimeMinutes) % 60;
				const FString ScheduleMessage =
					bMainShopOpen
						? FString::Printf(
							TEXT(
								"%02d:%02d - Le magasin ouvre ses portes."),
							ClockHour,
							ClockMinute)
						: FString::Printf(
							TEXT(
								"%02d:%02d - Le magasin ferme, les visiteurs repartent."),
							ClockHour,
							ClockMinute);
				Controller->ClientMessage(
					*ScheduleMessage);
			}
		}
		if (ABotanicusGameMode* GameMode =
				World->GetAuthGameMode<ABotanicusGameMode>())
		{
			GameMode->ScheduleInventoryAutosave();
		}
	}
}

void ABotanicusGameState::SetDevelopmentTimeScale(
	float InTimeScale)
{
	if (!HasAuthority())
	{
		return;
	}
	DevelopmentTimeScale =
		FMath::IsNearlyEqual(InTimeScale, 5.0f)
			? 5.0f
			: FMath::IsNearlyEqual(InTimeScale, 15.0f)
				? 15.0f
				: 1.0f;
	ApplyDevelopmentTimeScale();
	ForceNetUpdate();
}

void ABotanicusGameState::SetMainShopLevelForDevelopment(
	int32 InLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	MainShopLevel = FMath::Clamp(InLevel, 1, 99);
	NotifyFundsChanged();
}

void ABotanicusGameState::ApplyDevelopmentTimeScale()
{
	UGameplayStatics::SetGlobalTimeDilation(
		this,
		DevelopmentTimeScale);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float PlayerCompensation =
		1.0f / FMath::Max(1.0f, DevelopmentTimeScale);
	for (TActorIterator<ABotanicusCharacter> CharacterIt(World);
		 CharacterIt;
		 ++CharacterIt)
	{
		CharacterIt->CustomTimeDilation = PlayerCompensation;
	}
	for (FConstPlayerControllerIterator ControllerIt =
			 World->GetPlayerControllerIterator();
		 ControllerIt;
		 ++ControllerIt)
	{
		if (APlayerController* Controller = ControllerIt->Get())
		{
			Controller->CustomTimeDilation = PlayerCompensation;
		}
	}
}

void ABotanicusGameState::RefreshOutdoorEnvironment()
{
	const UBotanicusSeasonSettings* Settings =
		GetDefault<UBotanicusSeasonSettings>();
	if (!Settings)
	{
		return;
	}
	const FBotanicusOutdoorEnvironmentState NewEnvironment =
		Settings->EvaluateEnvironment(CurrentDayNumber, DayTimeMinutes);
	if (!OutdoorEnvironment.IsNearlyEqual(NewEnvironment))
	{
		OutdoorEnvironment = NewEnvironment;
		ForceNetUpdate();
	}
}

bool ABotanicusGameState::DidClockCrossMinute(
	float PreviousMinute,
	float CurrentMinute,
	float TargetMinute) const
{
	return CurrentMinute >= PreviousMinute
		? TargetMinute > PreviousMinute &&
			TargetMinute <= CurrentMinute
		: TargetMinute > PreviousMinute ||
			TargetMinute <= CurrentMinute;
}

void ABotanicusGameState::BeginShopDay()
{
	bDayCycleInitialized = true;
	bShopDayActive = true;
	DailyPlantsSold = 0;
	DailyRevenue = 0;
	DailySatisfactionTotal = 0;
	DailyReviewCount = 0;
	DayStartReputation = ShopReputationPoints;
	RotateShopTrends();
}

void ABotanicusGameState::FinishShopDay()
{
	LastCompletedDayNumber = CurrentDayNumber;
	LastDayPlantsSold = DailyPlantsSold;
	LastDayRevenue = DailyRevenue;
	LastDayReviewCount = DailyReviewCount;
	LastDayAverageSatisfaction =
		DailyReviewCount > 0
			? FMath::RoundToInt(
				static_cast<float>(DailySatisfactionTotal) /
					DailyReviewCount)
			: 0;
	LastDayReputationDelta =
		ShopReputationPoints - DayStartReputation;
	LastDaySalesTarget = GetDailySalesTarget();
	LastDayRevenueTarget = GetDailyRevenueTarget();
	++DaySummaryRevision;
	++CurrentDayNumber;
	RefreshOutdoorEnvironment();
	bShopDayActive = false;
	DailyPlantsSold = 0;
	DailyRevenue = 0;
	DailySatisfactionTotal = 0;
	DailyReviewCount = 0;
	DayStartReputation = ShopReputationPoints;
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

void ABotanicusGameState::RecordPlantSale(int32 SaleRevenue)
{
	if (!HasAuthority())
	{
		return;
	}
	++TotalPlantsSold;
	if (bShopDayActive)
	{
		++DailyPlantsSold;
		DailyRevenue += FMath::Max(0, SaleRevenue);
	}
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
	if (bShopDayActive)
	{
		DailySatisfactionTotal += LastVisitorSatisfaction;
		++DailyReviewCount;
	}
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

void ABotanicusGameState::OnRep_DevelopmentTimeScale()
{
	ApplyDevelopmentTimeScale();
}

void ABotanicusGameState::NotifyFundsChanged()
{
	ForceNetUpdate();
}
