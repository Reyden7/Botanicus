// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantCatalog.h"

namespace
{
	constexpr float MinimumEnvironmentGrowthRateMultiplier = 0.35f;

	float CalculateRangeComfort(
		float Value,
		float MinimumTolerated,
		float MinimumIdeal,
		float MaximumIdeal,
		float MaximumTolerated)
	{
		const float SafeMinimumIdeal = FMath::Max(
			MinimumTolerated, MinimumIdeal);
		const float SafeMaximumIdeal = FMath::Max(
			SafeMinimumIdeal, MaximumIdeal);
		const float SafeMaximumTolerated = FMath::Max(
			SafeMaximumIdeal, MaximumTolerated);

		if (Value < MinimumTolerated || Value > SafeMaximumTolerated)
		{
			return 0.0f;
		}
		if (Value >= SafeMinimumIdeal && Value <= SafeMaximumIdeal)
		{
			return 1.0f;
		}
		if (Value < SafeMinimumIdeal)
		{
			return FMath::GetRangePct(
				MinimumTolerated, SafeMinimumIdeal, Value);
		}
		return 1.0f - FMath::GetRangePct(
			SafeMaximumIdeal, SafeMaximumTolerated, Value);
	}

	EBotanicusPlantEnvironmentCondition ClassifyCondition(
		float Value,
		float MinimumIdeal,
		float MaximumIdeal)
	{
		if (Value < MinimumIdeal)
		{
			return EBotanicusPlantEnvironmentCondition::TooLow;
		}
		if (Value > MaximumIdeal)
		{
			return EBotanicusPlantEnvironmentCondition::TooHigh;
		}
		return EBotanicusPlantEnvironmentCondition::Ideal;
	}
}

bool FBotanicusPlantEnvironmentState::IsNearlyEqual(
	const FBotanicusPlantEnvironmentState& Other,
	float Tolerance) const
{
	return bEnvironmentAvailable == Other.bEnvironmentAvailable &&
		bInsideGreenhouse == Other.bInsideGreenhouse &&
		FMath::IsNearlyEqual(TemperatureCelsius, Other.TemperatureCelsius, Tolerance) &&
		FMath::IsNearlyEqual(AirHumidityPercent, Other.AirHumidityPercent, Tolerance) &&
		FMath::IsNearlyEqual(LuminosityPercent, Other.LuminosityPercent, Tolerance) &&
		FMath::IsNearlyEqual(TemperatureComfort, Other.TemperatureComfort, Tolerance) &&
		TemperatureCondition == Other.TemperatureCondition &&
		FMath::IsNearlyEqual(AirHumidityComfort, Other.AirHumidityComfort, Tolerance) &&
		AirHumidityCondition == Other.AirHumidityCondition &&
		FMath::IsNearlyEqual(LuminosityComfort, Other.LuminosityComfort, Tolerance) &&
		LuminosityCondition == Other.LuminosityCondition &&
		FMath::IsNearlyEqual(OverallComfort, Other.OverallComfort, Tolerance) &&
		FMath::IsNearlyEqual(
			GrowthRateMultiplier, Other.GrowthRateMultiplier, Tolerance);
}

FBotanicusPlantEnvironmentState EvaluateBotanicusPlantEnvironment(
	const FBotanicusPlantEnvironmentRequirements& Requirements,
	bool bEnvironmentAvailable,
	bool bInsideGreenhouse,
	float TemperatureCelsius,
	float AirHumidityPercent,
	float LuminosityPercent)
{
	FBotanicusPlantEnvironmentState State;
	State.bEnvironmentAvailable = bEnvironmentAvailable;
	State.bInsideGreenhouse = bInsideGreenhouse;
	if (!bEnvironmentAvailable)
	{
		return State;
	}

	State.TemperatureCelsius = TemperatureCelsius;
	State.AirHumidityPercent = AirHumidityPercent;
	State.LuminosityPercent = LuminosityPercent;
	State.TemperatureComfort = CalculateRangeComfort(
		TemperatureCelsius,
		Requirements.MinimumToleratedTemperatureCelsius,
		Requirements.MinimumIdealTemperatureCelsius,
		Requirements.MaximumIdealTemperatureCelsius,
		Requirements.MaximumToleratedTemperatureCelsius);
	State.TemperatureCondition = ClassifyCondition(
		TemperatureCelsius,
		Requirements.MinimumIdealTemperatureCelsius,
		Requirements.MaximumIdealTemperatureCelsius);
	State.AirHumidityComfort = CalculateRangeComfort(
		AirHumidityPercent,
		Requirements.MinimumToleratedAirHumidityPercent,
		Requirements.MinimumIdealAirHumidityPercent,
		Requirements.MaximumIdealAirHumidityPercent,
		Requirements.MaximumToleratedAirHumidityPercent);
	State.AirHumidityCondition = ClassifyCondition(
		AirHumidityPercent,
		Requirements.MinimumIdealAirHumidityPercent,
		Requirements.MaximumIdealAirHumidityPercent);
	State.LuminosityComfort = CalculateRangeComfort(
		LuminosityPercent,
		Requirements.MinimumToleratedLuminosityPercent,
		Requirements.MinimumIdealLuminosityPercent,
		Requirements.MaximumIdealLuminosityPercent,
		Requirements.MaximumToleratedLuminosityPercent);
	State.LuminosityCondition = ClassifyCondition(
		LuminosityPercent,
		Requirements.MinimumIdealLuminosityPercent,
		Requirements.MaximumIdealLuminosityPercent);
	State.OverallComfort =
		(State.TemperatureComfort +
		 State.AirHumidityComfort +
		 State.LuminosityComfort) / 3.0f;
	State.GrowthRateMultiplier = FMath::Lerp(
		MinimumEnvironmentGrowthRateMultiplier,
		1.0f,
		State.OverallComfort);
	return State;
}

const FBotanicusPlantDefinition* UBotanicusPlantCatalog::FindPlant(
	FName PlantKey) const
{
	return Plants.FindByPredicate(
		[PlantKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.PlantKey == PlantKey;
		});
}

const FBotanicusPlantDefinition*
UBotanicusPlantCatalog::FindPlantBySeed(FName SeedItemKey) const
{
	return Plants.FindByPredicate(
		[SeedItemKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.SeedItemKey == SeedItemKey;
		});
}

const FBotanicusPlantDefinition*
UBotanicusPlantCatalog::FindPlantByHarvestItem(
	FName HarvestItemKey) const
{
	return Plants.FindByPredicate(
		[HarvestItemKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.HarvestItemKey == HarvestItemKey;
		});
}
