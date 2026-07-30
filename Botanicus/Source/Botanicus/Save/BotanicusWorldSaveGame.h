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
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedPath
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector_NetQuantize10> Points;

	UPROPERTY()
	TArray<FVector_NetQuantize10> JunctionPoints;
};

/** Server-owned persistent state for one Botanicus map. */
UCLASS()
class BOTANICUS_API UBotanicusWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveVersion = 2;

	UPROPERTY()
	FString MapName;

	UPROPERTY()
	TArray<FBotanicusSavedBuildingActor> BuildingActors;

	UPROPERTY()
	TArray<FName> RemovedBuildingActorNames;

	UPROPERTY()
	TArray<FTransform> CommunicationDoors;

	UPROPERTY()
	TArray<FBotanicusSavedPlayerInventory> PlayerInventories;

	UPROPERTY()
	TArray<FBotanicusSavedPath> Paths;
};
