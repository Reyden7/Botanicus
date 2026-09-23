// Copyright Epic Games, Inc. All Rights Reserved.

#include "IllegalTrade/BotanicusIllegalTradeManager.h"

#include "BotanicusGameState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "IllegalTrade/BotanicusIllegalCustomerCharacter.h"
#include "IllegalTrade/BotanicusIllegalCustomerSpawnPoint.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"

ABotanicusIllegalTradeManager::ABotanicusIllegalTradeManager()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

void ABotanicusIllegalTradeManager::BeginPlay()
{
	Super::BeginPlay();
	SpawnCountdown = GetDefault<UBotanicusIllegalTradeSettings>()->FirstCustomerDelaySeconds;
}

void ABotanicusIllegalTradeManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	ABotanicusGameState* State = GetWorld()->GetGameState<ABotanicusGameState>();
	if (!State || !State->IsIllegalTradeActive())
	{
		for (const TWeakObjectPtr<ABotanicusIllegalCustomerCharacter>& Customer : ActiveCustomers)
		{
			if (Customer.IsValid()) Customer->ForceCustomerToLeave();
		}
		ActiveCustomers.RemoveAll([](const TWeakObjectPtr<ABotanicusIllegalCustomerCharacter>& Customer)
		{
			return !Customer.IsValid();
		});
		SpawnCountdown = GetDefault<UBotanicusIllegalTradeSettings>()->FirstCustomerDelaySeconds;
		return;
	}
	ActiveCustomers.RemoveAll([](const TWeakObjectPtr<ABotanicusIllegalCustomerCharacter>& Customer)
	{
		return !Customer.IsValid();
	});
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	if (ActiveCustomers.Num() >= FMath::Max(1, Settings->MaximumConcurrentCustomers)) return;
	SpawnCountdown -= DeltaSeconds;
	if (SpawnCountdown <= 0.0f)
	{
		if (SpawnCustomer())
		{
			SpawnCountdown = FMath::FRandRange(
				FMath::Min(Settings->MinimumSpawnDelaySeconds, Settings->MaximumSpawnDelaySeconds),
				FMath::Max(Settings->MinimumSpawnDelaySeconds, Settings->MaximumSpawnDelaySeconds));
		}
		else
		{
			SpawnCountdown = 3.0f;
		}
	}
}

bool ABotanicusIllegalTradeManager::DebugSpawnCustomer()
{
	ActiveCustomers.RemoveAll([](const TWeakObjectPtr<ABotanicusIllegalCustomerCharacter>& Customer)
	{
		return !Customer.IsValid();
	});
	const int32 Maximum = FMath::Max(1, GetDefault<UBotanicusIllegalTradeSettings>()->MaximumConcurrentCustomers);
	return HasAuthority() && ActiveCustomers.Num() < Maximum && SpawnCustomer();
}

bool ABotanicusIllegalTradeManager::SpawnCustomer()
{
	FVector SpawnLocation;
	FVector TargetLocation;
	FVector ExitLocation;
	if (!ResolveRoute(SpawnLocation, TargetLocation, ExitLocation)) return false;
	const UBotanicusIllegalTradeSettings* Settings =
		GetDefault<UBotanicusIllegalTradeSettings>();
	UClass* CustomerClass = Settings->CustomerClass.LoadSynchronous();
	if (!CustomerClass ||
		!CustomerClass->IsChildOf(ABotanicusIllegalCustomerCharacter::StaticClass()))
	{
		CustomerClass = ABotanicusIllegalCustomerCharacter::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABotanicusIllegalCustomerCharacter* Customer = GetWorld()->SpawnActor<ABotanicusIllegalCustomerCharacter>(
		CustomerClass, SpawnLocation, FRotator::ZeroRotator, Params);
	if (!Customer) return false;

	if (Settings->Plants.IsEmpty())
	{
		Customer->Destroy();
		return false;
	}
	const FBotanicusIllegalPlantDefinition& Plant = Settings->Plants[0];
	const int32 Quantity = FMath::Max(1, Settings->CustomerOrderQuantity);
	const int32 Reward = Quantity * FMath::Max(1, Plant.UnitSaleValue);
	Customer->InitializeIllegalOrder(
		Plant.ProductItemKey, Plant.DisplayName, Quantity, Reward,
		Settings->CustomerWaitSeconds, TargetLocation, ExitLocation);
	ActiveCustomers.Add(Customer);
	UE_LOG(LogBotanicusIllegalTrade, Display,
		TEXT("Spawned illegal customer %s ordering %d x %s."),
		*GetNameSafe(Customer->GetClass()), Quantity,
		*Plant.ProductItemKey.ToString());
	return true;
}

bool ABotanicusIllegalTradeManager::ResolveRoute(
	FVector& OutSpawn,
	FVector& OutTarget,
	FVector& OutExit) const
{
	for (TActorIterator<ABotanicusIllegalCustomerSpawnPoint> It(GetWorld()); It; ++It)
	{
		OutSpawn = It->GetSpawnLocation();
		OutTarget = It->GetWaitingLocation();
		OutExit = It->GetExitLocation();
		return true;
	}

	// The rear entrance is an authored route. A player-relative fallback can
	// spawn a clandestine customer inside the daytime shop or behind walls.
	return false;
}
