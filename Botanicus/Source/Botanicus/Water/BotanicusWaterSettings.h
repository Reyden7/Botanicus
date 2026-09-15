// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusWaterSettings.generated.h"

/**
 * Global tuning for functional wetness, runoff and retained-water accumulation.
 * Editable in Project Settings > Game > Botanicus Water Surfaces.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Water Surfaces"))
class BOTANICUS_API UBotanicusWaterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Classification",
		meta=(ClampMin="0.0", ClampMax="80.0", Units="Degrees"))
	float MaximumPuddleSlopeDegrees = 28.0f;

	/** Normals with an upward dot at or below this value are vertical/overhanging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Classification",
		meta=(ClampMin="0.0", ClampMax="0.95"))
	float VerticalNormalDotThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Accumulation",
		meta=(ClampMin="0.01"))
	float PuddleThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Accumulation",
		meta=(ClampMin="0.01"))
	float MaximumAccumulatedWater = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Distribution",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float HorizontalRetentionFraction = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Distribution",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float InclinedRetentionFraction = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Distribution",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float VerticalRetentionFraction = 0.15f;

	/** Share of non-retained water reported as runoff; the remainder is splash/loss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Distribution",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float RunoffShare = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Grouping",
		meta=(ClampMin="1.0", Units="cm"))
	float ZoneMergeRadius = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Grouping",
		meta=(ClampMin="1", ClampMax="512"))
	int32 MaximumTrackedZones = 96;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Evaporation",
		meta=(ClampMin="0.0"))
	float PuddleEvaporationRate = 0.018f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Evaporation",
		meta=(ClampMin="0.0"))
	float WetnessEvaporationRate = 0.008f;

};
