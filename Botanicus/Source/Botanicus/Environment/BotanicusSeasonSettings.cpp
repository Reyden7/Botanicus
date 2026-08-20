// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/BotanicusSeasonSettings.h"

bool FBotanicusOutdoorEnvironmentState::IsNearlyEqual(
	const FBotanicusOutdoorEnvironmentState& Other,
	float Tolerance) const
{
	return Season == Other.Season &&
		DayInSeason == Other.DayInSeason &&
		Year == Other.Year &&
		FMath::IsNearlyEqual(TemperatureCelsius, Other.TemperatureCelsius, Tolerance) &&
		FMath::IsNearlyEqual(AirHumidityPercent, Other.AirHumidityPercent, Tolerance) &&
		FMath::IsNearlyEqual(LuminosityPercent, Other.LuminosityPercent, Tolerance);
}

UBotanicusSeasonSettings::UBotanicusSeasonSettings()
{
	Spring.NightTemperatureCelsius = 10.0f;
	Spring.DayTemperatureCelsius = 22.0f;
	Spring.AirHumidityPercent = 65.0f;
	Spring.MaximumLuminosityPercent = 70.0f;
	Spring.SunriseTimeMinutes = 360.0f;
	Spring.SunsetTimeMinutes = 1200.0f;

	Summer.NightTemperatureCelsius = 18.0f;
	Summer.DayTemperatureCelsius = 32.0f;
	Summer.AirHumidityPercent = 45.0f;
	Summer.MaximumLuminosityPercent = 95.0f;
	Summer.SunriseTimeMinutes = 300.0f;
	Summer.SunsetTimeMinutes = 1260.0f;

	Autumn.NightTemperatureCelsius = 7.0f;
	Autumn.DayTemperatureCelsius = 19.0f;
	Autumn.AirHumidityPercent = 75.0f;
	Autumn.MaximumLuminosityPercent = 55.0f;
	Autumn.SunriseTimeMinutes = 420.0f;
	Autumn.SunsetTimeMinutes = 1080.0f;

	Winter.NightTemperatureCelsius = -2.0f;
	Winter.DayTemperatureCelsius = 8.0f;
	Winter.AirHumidityPercent = 60.0f;
	Winter.MaximumLuminosityPercent = 35.0f;
	Winter.SunriseTimeMinutes = 480.0f;
	Winter.SunsetTimeMinutes = 1020.0f;
}

const FBotanicusSeasonEnvironmentProfile&
UBotanicusSeasonSettings::GetProfile(EBotanicusSeason Season) const
{
	switch (Season)
	{
	case EBotanicusSeason::Summer:
		return Summer;
	case EBotanicusSeason::Autumn:
		return Autumn;
	case EBotanicusSeason::Winter:
		return Winter;
	default:
		return Spring;
	}
}

FBotanicusOutdoorEnvironmentState
UBotanicusSeasonSettings::EvaluateEnvironment(
	int32 DayNumber,
	float DayTimeMinutes) const
{
	FBotanicusOutdoorEnvironmentState State;
	const int32 SafeDaysPerSeason = FMath::Max(1, DaysPerSeason);
	const int32 SafeDayIndex = FMath::Max(0, DayNumber - 1);
	const int32 SeasonIndex = (SafeDayIndex / SafeDaysPerSeason) % 4;
	State.Season = static_cast<EBotanicusSeason>(SeasonIndex);
	State.DayInSeason = (SafeDayIndex % SafeDaysPerSeason) + 1;
	State.Year = (SafeDayIndex / (SafeDaysPerSeason * 4)) + 1;

	const FBotanicusSeasonEnvironmentProfile& Profile =
		GetProfile(State.Season);
	const float SafeMinute = FMath::Fmod(
		FMath::Max(0.0f, DayTimeMinutes), 1440.0f);
	const float TemperatureCycle = 0.5f + 0.5f * FMath::Cos(
		(SafeMinute - 840.0f) / 1440.0f * 2.0f * PI);
	State.TemperatureCelsius = FMath::Lerp(
		Profile.NightTemperatureCelsius,
		Profile.DayTemperatureCelsius,
		TemperatureCycle);
	State.AirHumidityPercent = FMath::Clamp(
		Profile.AirHumidityPercent, 0.0f, 100.0f);

	const float Sunrise = FMath::Clamp(
		Profile.SunriseTimeMinutes, 0.0f, 1439.0f);
	const float Sunset = FMath::Clamp(
		Profile.SunsetTimeMinutes, Sunrise + 1.0f, 1440.0f);
	float DaylightFactor = 0.0f;
	if (SafeMinute >= Sunrise && SafeMinute <= Sunset)
	{
		const float DayProgress =
			(SafeMinute - Sunrise) / (Sunset - Sunrise);
		DaylightFactor = FMath::Sin(DayProgress * PI);
	}
	State.LuminosityPercent = FMath::Clamp(
		Profile.MaximumLuminosityPercent * DaylightFactor,
		0.0f,
		100.0f);

	// Stable tenths avoid needless network updates for imperceptible changes.
	State.TemperatureCelsius =
		FMath::RoundToFloat(State.TemperatureCelsius * 10.0f) / 10.0f;
	State.AirHumidityPercent =
		FMath::RoundToFloat(State.AirHumidityPercent * 10.0f) / 10.0f;
	State.LuminosityPercent =
		FMath::RoundToFloat(State.LuminosityPercent * 10.0f) / 10.0f;
	return State;
}
