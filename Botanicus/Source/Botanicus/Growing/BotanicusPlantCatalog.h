// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusPlantCatalog.generated.h"

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusPlantDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FName PlantKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FName SeedItemKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Growth",
		meta=(ClampMin="1.0", Units="s"))
	float GrowthDurationSeconds = 120.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Water",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumHealthyWater = 0.25f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Water",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumHealthyWater = 0.9f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Water",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float WaterAddedPerUse = 0.35f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Water",
		meta=(ClampMin="0.0"))
	float WaterConsumptionPerSecond = 0.003f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	FLinearColor MatureColor =
		FLinearColor(0.12f, 0.55f, 0.08f, 1.0f);
};

UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusPlantCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	const FBotanicusPlantDefinition* FindPlant(FName PlantKey) const;
	const FBotanicusPlantDefinition* FindPlantBySeed(
		FName SeedItemKey) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Catalog")
	TArray<FBotanicusPlantDefinition> Plants;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Plant Catalog"))
class BOTANICUS_API UBotanicusPlantCatalogSettings
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, Category="Catalog")
	TSoftObjectPtr<UBotanicusPlantCatalog> Catalog;
};
