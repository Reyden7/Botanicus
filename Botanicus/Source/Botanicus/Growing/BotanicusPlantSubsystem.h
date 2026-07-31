// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BotanicusPlantSubsystem.generated.h"

UCLASS()
class BOTANICUS_API UBotanicusPlantSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(
		FSubsystemCollectionBase& Collection) override;

	const FBotanicusPlantDefinition* FindPlant(FName PlantKey) const;
	const FBotanicusPlantDefinition* FindPlantBySeed(
		FName SeedItemKey) const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Growing")
	TArray<FBotanicusPlantDefinition> GetAllPlants() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBotanicusPlantCatalog> LoadedCatalog;

	UPROPERTY(Transient)
	TArray<FBotanicusPlantDefinition> NativeFallbackPlants;
};
