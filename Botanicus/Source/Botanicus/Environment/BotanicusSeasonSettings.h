// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusSeasonSettings.generated.h"

UENUM(BlueprintType)
enum class EBotanicusSeason : uint8
{
	Spring UMETA(DisplayName="Printemps"),
	Summer UMETA(DisplayName="Ete"),
	Autumn UMETA(DisplayName="Automne"),
	Winter UMETA(DisplayName="Hiver")
};

/** Editable climate defaults for one season. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSeasonEnvironmentProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temperature",
		meta=(Units="Celsius"))
	float NightTemperatureCelsius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Temperature",
		meta=(Units="Celsius"))
	float DayTemperatureCelsius = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Air Humidity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float AirHumidityPercent = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MaximumLuminosityPercent = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="0.0", ClampMax="1439.0", Units="Minutes"))
	float SunriseTimeMinutes = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Luminosity",
		meta=(ClampMin="1.0", ClampMax="1440.0", Units="Minutes"))
	float SunsetTimeMinutes = 1200.0f;
};

/** Current replicated outdoor values consumed by plants and future equipment. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusOutdoorEnvironmentState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Season")
	EBotanicusSeason Season = EBotanicusSeason::Spring;

	UPROPERTY(BlueprintReadOnly, Category="Season")
	int32 DayInSeason = 1;

	UPROPERTY(BlueprintReadOnly, Category="Season")
	int32 Year = 1;

	UPROPERTY(BlueprintReadOnly, Category="Environment", meta=(Units="Celsius"))
	float TemperatureCelsius = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment", meta=(Units="Percent"))
	float AirHumidityPercent = 65.0f;

	UPROPERTY(BlueprintReadOnly, Category="Environment", meta=(Units="Percent"))
	float LuminosityPercent = 0.0f;

	bool IsNearlyEqual(
		const FBotanicusOutdoorEnvironmentState& Other,
		float Tolerance = 0.01f) const;
};

/**
 * Global season tuning, editable in Project Settings > Game > Botanicus Seasons.
 * The calendar is deterministic from the existing saved day number.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Seasons"))
class BOTANICUS_API UBotanicusSeasonSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBotanicusSeasonSettings();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Calendar",
		meta=(ClampMin="1", UIMin="1", UIMax="30"))
	int32 DaysPerSeason = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Seasons")
	FBotanicusSeasonEnvironmentProfile Spring;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Seasons")
	FBotanicusSeasonEnvironmentProfile Summer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Seasons")
	FBotanicusSeasonEnvironmentProfile Autumn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category="Seasons")
	FBotanicusSeasonEnvironmentProfile Winter;

	const FBotanicusSeasonEnvironmentProfile& GetProfile(
		EBotanicusSeason Season) const;

	FBotanicusOutdoorEnvironmentState EvaluateEnvironment(
		int32 DayNumber,
		float DayTimeMinutes) const;
};
