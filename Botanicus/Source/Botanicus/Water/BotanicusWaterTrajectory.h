// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Water/BotanicusWaterTypes.h"

class UWorld;

struct BOTANICUS_API FBotanicusWaterTrajectorySample
{
	float Time = 0.0f;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

namespace BotanicusWaterTrajectory
{
	BOTANICUS_API FVector PositionAtTime(
		const FBotanicusWaterStreamParams& Params,
		float Time);

	BOTANICUS_API FVector VelocityAtTime(
		const FBotanicusWaterStreamParams& Params,
		float Time);

	/** Builds an adaptive polyline used by collision, debug and future visuals. */
	BOTANICUS_API void BuildSamples(
		const FBotanicusWaterStreamParams& Params,
		TArray<FBotanicusWaterTrajectorySample>& OutSamples);

	/** Sweeps the adaptive polyline and returns only its first blocking hit. */
	BOTANICUS_API bool TraceFirstBlockingHit(
		UWorld* World,
		const FBotanicusWaterStreamParams& Params,
		AActor* SourceActor,
		const TArray<const AActor*>& IgnoredActors,
		float EmittedAmount,
		FBotanicusWaterHit& OutWaterHit,
		TArray<FBotanicusWaterTrajectorySample>* OutTrajectorySamples = nullptr);

	BOTANICUS_API bool IsDebugEnabled();
}
