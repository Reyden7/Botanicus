// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusIllegalTradeManager.generated.h"

class ABotanicusIllegalCustomerCharacter;

/** Owns the one-customer-at-a-time nocturnal spawning loop. */
UCLASS()
class BOTANICUS_API ABotanicusIllegalTradeManager : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusIllegalTradeManager();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool DebugSpawnCustomer();

private:
	bool SpawnCustomer();
	bool ResolveRoute(FVector& OutSpawn, FVector& OutTarget, FVector& OutExit) const;

	TArray<TWeakObjectPtr<ABotanicusIllegalCustomerCharacter>> ActiveCustomers;
	float SpawnCountdown = 0.0f;
};
