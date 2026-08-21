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

/** Ideal and survivable greenhouse ranges authored independently per species. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusPlantEnvironmentRequirements
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temperature",
		meta=(Units="Celsius"))
	float MinimumToleratedTemperatureCelsius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temperature",
		meta=(Units="Celsius"))
	float MinimumIdealTemperatureCelsius = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temperature",
		meta=(Units="Celsius"))
	float MaximumIdealTemperatureCelsius = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temperature",
		meta=(Units="Celsius"))
	float MaximumToleratedTemperatureCelsius = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Humidity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MinimumToleratedAirHumidityPercent = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Humidity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MinimumIdealAirHumidityPercent = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Humidity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MaximumIdealAirHumidityPercent = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Humidity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MaximumToleratedAirHumidityPercent = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MinimumToleratedLuminosityPercent = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MinimumIdealLuminosityPercent = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MaximumIdealLuminosityPercent = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MaximumToleratedLuminosityPercent = 90.0f;
};

UENUM(BlueprintType)
enum class EBotanicusPlantEnvironmentCondition : uint8
{
	Unavailable,
	TooLow,
	Ideal,
	TooHigh
};

/** Replicated snapshot of the environment currently experienced by one plant. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusPlantEnvironmentState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Environment")
	bool bEnvironmentAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category="Environment")
	bool bInsideGreenhouse = false;

	UPROPERTY(BlueprintReadOnly, Category="Environment", meta=(Units="Celsius"))
	float TemperatureCelsius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment", meta=(Units="Percent"))
	float AirHumidityPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment", meta=(Units="Percent"))
	float LuminosityPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float TemperatureComfort = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment")
	EBotanicusPlantEnvironmentCondition TemperatureCondition =
		EBotanicusPlantEnvironmentCondition::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category="Environment",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float AirHumidityComfort = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment")
	EBotanicusPlantEnvironmentCondition AirHumidityCondition =
		EBotanicusPlantEnvironmentCondition::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category="Environment",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float LuminosityComfort = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment")
	EBotanicusPlantEnvironmentCondition LuminosityCondition =
		EBotanicusPlantEnvironmentCondition::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category="Environment",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float OverallComfort = 0.0f;

	/** Applied to growth in the current environment; never causes plant death. */
	UPROPERTY(BlueprintReadOnly, Category="Environment",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float GrowthRateMultiplier = 0.0f;

	bool IsNearlyEqual(
		const FBotanicusPlantEnvironmentState& Other,
		float Tolerance = 0.001f) const;
};

BOTANICUS_API FBotanicusPlantEnvironmentState
EvaluateBotanicusPlantEnvironment(
	const FBotanicusPlantEnvironmentRequirements& Requirements,
	bool bEnvironmentAvailable,
	bool bInsideGreenhouse,
	float TemperatureCelsius,
	float AirHumidityPercent,
	float LuminosityPercent);

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

	/** Botanical family; it no longer selects or restricts a greenhouse type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Element")
	EBotanicusPlantElement Element = EBotanicusPlantElement::Normal;

	/** Radius used by elemental reactions such as fire burning normal plants. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Element",
		meta=(ClampMin="0.0", Units="cm"))
	float ElementalInteractionRadius = 500.0f;

	/** Species that accelerate this plant while growing inside its interaction radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Neighbours")
	TArray<FName> CompatibleNeighbourPlantKeys;

	/** Species that slow this plant while growing inside its interaction radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Neighbours")
	TArray<FName> IncompatibleNeighbourPlantKeys;

	/** Growth-speed bonus contributed by each compatible neighbouring plant. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Plant Neighbours",
		meta=(ClampMin="0.0", ClampMax="1.0", Units="Percent"))
	float CompatibleNeighbourGrowthBonus = 0.10f;

	/** Growth-speed penalty contributed by each incompatible neighbouring plant. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Plant Neighbours",
		meta=(ClampMin="0.0", ClampMax="1.0", Units="Percent"))
	float IncompatibleNeighbourGrowthPenalty = 0.20f;

	/** Whether this species gently modifies the climate around living plants. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment Influence")
	bool bInfluencesEnvironment = true;

	/** Radius of the plant-created microclimate. Influence fades to zero at its edge. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Environment Influence",
		meta=(ClampMin="0.0", Units="cm"))
	float EnvironmentInfluenceRadius = 300.0f;

	/** Temperature variation in Celsius at the plant centre. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Environment Influence",
		meta=(Units="Celsius"))
	float EnvironmentTemperatureDelta = 0.0f;

	/** Air-humidity variation in percent at the plant centre. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Environment Influence",
		meta=(ClampMin="-100.0", ClampMax="100.0", Units="Percent"))
	float EnvironmentAirHumidityDelta = 0.0f;

	/** Luminosity variation in percent at the plant centre. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Environment Influence",
		meta=(ClampMin="-100.0", ClampMax="100.0", Units="Percent"))
	float EnvironmentLuminosityDelta = 0.0f;

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

	/** Climate ranges used to evaluate this species in the greenhouse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
	FBotanicusPlantEnvironmentRequirements Environment;

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

	/** Quality lost per second by a Fire plant while its soil is overwatered.
	 *  Set to zero on an individual Fire species to disable the reaction. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Species Behaviour|Fire",
		meta=(ClampMin="0.0", ClampMax="1.0", Units="Percent"))
	float FireOverwateringCareLossPerSecond = 0.005f;

	/** Horizontal size retained when a Shadow plant closes under excessive light. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Species Behaviour|Shadow",
		meta=(ClampMin="0.1", ClampMax="1.0"))
	float ShadowClosedHorizontalScale = 0.82f;

	/** Vertical size retained when a Shadow plant closes under excessive light. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Species Behaviour|Shadow",
		meta=(ClampMin="0.1", ClampMax="1.0"))
	float ShadowClosedVerticalScale = 0.55f;

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
