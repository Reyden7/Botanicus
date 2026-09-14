// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusWaterSettings.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/**
 * Global tuning for lightweight wetness, runoff and puddle accumulation.
 * Editable in Project Settings > Game > Botanicus Water Surfaces.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Water Surfaces"))
class BOTANICUS_API UBotanicusWaterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBotanicusWaterSettings();

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Accumulation",
		meta=(ClampMin="0.01"))
	float WetnessSaturationAmount = 0.22f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Wetness Visual",
		meta=(ClampMin="1", ClampMax="256"))
	int32 MaximumWetnessVisuals = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Wetness Visual",
		meta=(ClampMin="1.0", Units="cm"))
	float MinimumWetnessRadius = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Wetness Visual",
		meta=(ClampMin="1.0", Units="cm"))
	float MaximumWetnessRadius = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Wetness Visual")
	TSoftObjectPtr<UMaterialInterface> WetnessDecalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="1", ClampMax="128"))
	int32 MaximumVisiblePuddles = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="1.0", Units="cm"))
	float MinimumPuddleRadius = 18.0f;

	/** Readable radius used on the very first retained drop, before the gameplay puddle threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="0.5", Units="cm"))
	float InitialPuddleRadius = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="1.0", Units="cm"))
	float MaximumPuddleRadius = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="0.01"))
	float PuddleGrowthSpeed = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="0.01"))
	float PuddleShrinkSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="1", ClampMax="6"))
	int32 PuddleShapeVariants = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="0.5", ClampMax="1.5"))
	float MinimumPuddleAspectScale = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual",
		meta=(ClampMin="0.5", ClampMax="1.5"))
	float MaximumPuddleAspectScale = 1.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual")
	TSoftObjectPtr<UStaticMesh> PuddleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Puddle Visual")
	TSoftObjectPtr<UMaterialInterface> PuddleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Impact Visual",
		meta=(ClampMin="0", ClampMax="32"))
	int32 MaximumImpactEffects = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Impact Visual")
	TSoftObjectPtr<UNiagaraSystem> ImpactNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Runoff Visual",
		meta=(ClampMin="0", ClampMax="32"))
	int32 MaximumRunoffEffects = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Runoff Visual")
	TSoftObjectPtr<UNiagaraSystem> RunoffNiagara;
};
