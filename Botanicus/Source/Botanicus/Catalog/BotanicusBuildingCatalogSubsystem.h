// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Catalog/BotanicusBuildingCatalog.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BotanicusBuildingCatalogSubsystem.generated.h"

/** Runtime resolver for the configured building catalogue. */
UCLASS()
class BOTANICUS_API UBotanicusBuildingCatalogSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(
		FSubsystemCollectionBase& Collection) override;

	const FBotanicusBuildingDefinition* FindBuilding(
		FName BuildingKey) const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building Catalog")
	bool GetBuildingDefinition(
		FName BuildingKey,
		FBotanicusBuildingDefinition& OutDefinition) const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building Catalog")
	TArray<FBotanicusBuildingDefinition> GetAllBuildings() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBotanicusBuildingCatalog> LoadedCatalog;

	UPROPERTY(Transient)
	TArray<FBotanicusBuildingDefinition> NativeFallbackBuildings;
};
