// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
struct FBotanicusPlantDefinition;

/** Result of evaluating the living plants around one growing plant. */
struct BOTANICUS_API FBotanicusPlantCompatibilityResult
{
	float GrowthMultiplier = 1.0f;
	int32 CompatibleNeighbourCount = 0;
	int32 IncompatibleNeighbourCount = 0;
};

/**
 * Evaluates species-specific neighbour lists inside the definition's
 * interaction radius. IgnoredSlotIndex is used by multi-plant containers;
 * INDEX_NONE ignores the entire owning single-plant pot.
 */
BOTANICUS_API FBotanicusPlantCompatibilityResult
EvaluateBotanicusPlantCompatibility(
	const UWorld* World,
	const FVector& TargetLocation,
	const AActor* IgnoredOwner,
	int32 IgnoredSlotIndex,
	const FBotanicusPlantDefinition& Definition);
