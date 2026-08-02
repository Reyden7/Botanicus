// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusVisitorManager.generated.h"

/** Server-only tycoon-style crowd generator for the nursery. */
UCLASS()
class BOTANICUS_API ABotanicusVisitorManager : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusVisitorManager();

	virtual void Tick(float DeltaSeconds) override;

private:
	bool BuildVisitorCircuit(
		TArray<FVector>& OutCircuit,
		int32& OutCheckoutWaypointIndex) const;
	void SpawnQueuedVisitor(
		const TArray<FVector>& VisitorCircuit,
		int32 CheckoutWaypointIndex);
	void RefreshQueuePositions(
		const TArray<FVector>& VisitorCircuit);
	int32 GetShopVisitorCapacity() const;
	int32 GetTargetVisitorPopulation() const;
	float GetReputationSpawnMultiplier() const;
	bool IsShopOpen() const;
	void SendAllVisitorsHome();

	UPROPERTY(EditAnywhere, Category="Botanicus|Visitors", meta=(ClampMin="0.5"))
	float VisitorSpawnInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Visitors", meta=(ClampMin="0.1"))
	float VisitorAdmissionInterval = 0.75f;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<class ABotanicusVisitorCharacter>>
		WaitingVisitors;

	float SpawnRemaining = 0.5f;
	float AdmissionRemaining = 0.0f;
};
