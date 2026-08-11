// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusPlantCatalog.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EBotanicusPlantElement : uint8
{
	Normal,
	Fire,
	Water,
	Ice,
	Shadow
};

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

	/** Elemental plants only grow inside a greenhouse of the same element. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Element")
	EBotanicusPlantElement Element = EBotanicusPlantElement::Normal;

	/** Radius used by elemental reactions such as fire burning normal plants. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Element",
		meta=(ClampMin="0.0", Units="cm"))
	float ElementalInteractionRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	FName HarvestToolItemKey = TEXT("GardenTrowel");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	FName HarvestItemKey = NAME_None;

	/** Soil required when repotting this whole plant into a sale pot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sales")
	FName CompatibleSaleSoilItemKey = TEXT("PottingSoil");

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Harvest",
		meta=(ClampMin="1"))
	int32 HarvestQuantity = 1;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Harvest",
		meta=(ClampMin="0.1", Units="s"))
	float HarvestDurationSeconds = 1.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Growth",
		meta=(ClampMin="1.0", Units="s"))
	float GrowthDurationSeconds = 120.0f;

	/** Mesh displayed from 0% up to (but excluding) 30% growth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth|Visual Stages",
		meta=(DisplayName="Mesh stade 1 - Petite plante (0-30%)"))
	TSoftObjectPtr<UStaticMesh> SmallGrowthMesh;

	/** Mesh displayed from 30% up to (but excluding) 70% growth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth|Visual Stages",
		meta=(DisplayName="Mesh stade 2 - Plante moyenne (30-70%)"))
	TSoftObjectPtr<UStaticMesh> MediumGrowthMesh;

	/** Mesh displayed from 70% up to (but excluding) 100% growth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth|Visual Stages",
		meta=(DisplayName="Mesh stade 3 - Grande plante (70-99%)"))
	TSoftObjectPtr<UStaticMesh> LargeGrowthMesh;

	/** Final mesh displayed when growth reaches 100%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth|Visual Stages",
		meta=(DisplayName="Mesh stade 4 - Croissance terminee (100%)"))
	TSoftObjectPtr<UStaticMesh> MatureGrowthMesh;

	/** Visual height of the plant when growth begins, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth|Visual Stages",
		meta=(DisplayName="Hauteur visuelle a 0%", Units="cm",
			ClampMin="0.1", UIMin="1.0", UIMax="30.0"))
	float MinimumGrowthVisualHeight = 8.0f;

	/** Visual height shared by the large and mature meshes at 100%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Growth|Visual Stages",
		meta=(DisplayName="Hauteur visuelle a 100%", Units="cm",
			ClampMin="1.0", UIMin="30.0", UIMax="250.0"))
	float MatureGrowthVisualHeight = 100.0f;

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
	const FBotanicusPlantDefinition* FindPlantByHarvestItem(
		FName HarvestItemKey) const;

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
