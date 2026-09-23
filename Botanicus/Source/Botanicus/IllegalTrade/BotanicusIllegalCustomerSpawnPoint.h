// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusIllegalCustomerSpawnPoint.generated.h"

class UArrowComponent;
class USceneComponent;

/** Designer marker containing the spawn, waiting and exit positions of night clients. */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusIllegalCustomerSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusIllegalCustomerSpawnPoint();

	FVector GetSpawnLocation() const;
	FVector GetWaitingLocation() const;
	FVector GetExitLocation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UArrowComponent> SpawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UArrowComponent> TargetPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UArrowComponent> ExitPoint;
};
