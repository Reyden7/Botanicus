// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BotanicusWaterSourceProfile.generated.h"

/** Editable gameplay profile shared by a kind of water source. */
UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusWaterSourceProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Normalized reservoir units emitted per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Emission", meta=(ClampMin="0.0"))
	float FlowRate = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trajectory", meta=(ClampMin="1.0", Units="cm/s"))
	float InitialSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trajectory", meta=(ClampMin="0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collision", meta=(ClampMin="0.1", Units="cm"))
	float TraceRadius = 7.0f;

	/** Diameter of the continuous visible core. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.5", ClampMax="30.0", Units="cm"))
	float StreamWidth = 5.0f;

	/** Angular spread of the cosmetic droplets around the ballistic direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.0", ClampMax="30.0", Units="deg"))
	float DropletSpread = 4.0f;

	/** Fractional variation around InitialSpeed for cosmetic droplets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visuals", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropletSpeedVariation = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trajectory", meta=(ClampMin="0.01", Units="s"))
	float MaxSimulationTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trajectory", meta=(ClampMin="1.0", Units="cm"))
	float MaxDistance = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="1.0", ClampMax="60.0", Units="Hz"))
	float SimulationFrequency = 10.0f;

	/** Preferred length of a trace segment before curvature is considered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="5.0", Units="cm"))
	float TargetSegmentLength = 70.0f;

	/** Maximum sag between the ballistic curve and one straight trace segment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="0.1", Units="cm"))
	float MaxCurveDeviation = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="1", ClampMax="64"))
	int32 MaxTraceSegments = 24;
};
