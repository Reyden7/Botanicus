// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/BotanicusEnvironmentSubsystem.h"

#include "BotanicusGameState.h"
#include "Building/BotanicusElementalGreenhouseActor.h"
#include "Environment/BotanicusClimateDeviceActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "EngineUtils.h"

namespace
{
	void ResolvePlantEnvironmentDeltas(
		const FBotanicusPlantDefinition& Definition,
		float& OutTemperatureDelta,
		float& OutHumidityDelta,
		float& OutLuminosityDelta)
	{
		OutTemperatureDelta = Definition.EnvironmentTemperatureDelta;
		OutHumidityDelta = Definition.EnvironmentAirHumidityDelta;
		OutLuminosityDelta = Definition.EnvironmentLuminosityDelta;
		if (!FMath::IsNearlyZero(OutTemperatureDelta) ||
			!FMath::IsNearlyZero(OutHumidityDelta) ||
			!FMath::IsNearlyZero(OutLuminosityDelta))
		{
			return;
		}

		// Immediately useful defaults. Setting any delta in the plant catalog
		// replaces these family defaults for that species.
		switch (Definition.Element)
		{
		case EBotanicusPlantElement::Fire:
			OutTemperatureDelta = 2.0f;
			break;
		case EBotanicusPlantElement::Ice:
			OutTemperatureDelta = -2.0f;
			break;
		case EBotanicusPlantElement::Water:
			OutHumidityDelta = 6.0f;
			break;
		case EBotanicusPlantElement::Shadow:
			OutLuminosityDelta = -8.0f;
			break;
		default:
			break;
		}
	}

	void AccumulatePlantEnvironmentInfluence(
		const FBotanicusPlantDefinition& Definition,
		const FVector& SourceLocation,
		const FVector& TargetLocation,
		float GrowthProgress,
		float& InOutTemperatureDelta,
		float& InOutHumidityDelta,
		float& InOutLuminosityDelta)
	{
		const float Radius = FMath::Max(
			0.0f,
			Definition.EnvironmentInfluenceRadius);
		if (!Definition.bInfluencesEnvironment ||
			Radius <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float Distance = FVector::Distance(
			SourceLocation,
			TargetLocation);
		if (Distance >= Radius)
		{
			return;
		}

		float TemperatureDelta = 0.0f;
		float HumidityDelta = 0.0f;
		float LuminosityDelta = 0.0f;
		ResolvePlantEnvironmentDeltas(
			Definition,
			TemperatureDelta,
			HumidityDelta,
			LuminosityDelta);
		const float DistanceStrength = 1.0f - Distance / Radius;
		const float GrowthStrength = FMath::Lerp(
			0.25f,
			1.0f,
			FMath::Clamp(GrowthProgress, 0.0f, 1.0f));
		const float Strength = DistanceStrength * GrowthStrength;
		InOutTemperatureDelta += TemperatureDelta * Strength;
		InOutHumidityDelta += HumidityDelta * Strength;
		InOutLuminosityDelta += LuminosityDelta * Strength;
	}
}

bool UBotanicusEnvironmentSubsystem::GetEnvironmentAtLocation(
	const FVector& WorldLocation,
	float& OutTemperatureCelsius,
	float& OutAirHumidityPercent,
	float& OutLuminosityPercent,
	bool& bOutInsideGreenhouse) const
{
	OutTemperatureCelsius = 0.0f;
	OutAirHumidityPercent = 0.0f;
	OutLuminosityPercent = 0.0f;
	bOutInsideGreenhouse = false;
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	OutTemperatureCelsius = GameState->GetOutdoorTemperatureCelsius();
	OutAirHumidityPercent = GameState->GetOutdoorAirHumidityPercent();
	OutLuminosityPercent = GameState->GetOutdoorLuminosityPercent();
	const ABotanicusGreenhouseActor* Greenhouse =
		ABotanicusGreenhouseActor::FindGreenhouseAtLocation(
			World, WorldLocation);
	bOutInsideGreenhouse = Greenhouse != nullptr;
	if (Greenhouse)
	{
		Greenhouse->GetEnvironmentAtLocation(
			WorldLocation,
			OutTemperatureCelsius,
			OutAirHumidityPercent,
			OutLuminosityPercent);
	}

	float TemperatureDelta = 0.0f;
	float HumidityDelta = 0.0f;
	float LuminosityDelta = 0.0f;
	for (TActorIterator<ABotanicusClimateDeviceActor> DeviceIt(
			 const_cast<UWorld*>(World));
		 DeviceIt;
		 ++DeviceIt)
	{
		float DeviceTemperatureDelta = 0.0f;
		float DeviceHumidityDelta = 0.0f;
		float DeviceLuminosityDelta = 0.0f;
		DeviceIt->GetEnvironmentDeltasAtLocation(
			WorldLocation,
			DeviceTemperatureDelta,
			DeviceHumidityDelta,
			DeviceLuminosityDelta);
		TemperatureDelta += DeviceTemperatureDelta;
		HumidityDelta += DeviceHumidityDelta;
		LuminosityDelta += DeviceLuminosityDelta;
	}

	const UGameInstance* GameInstance = World->GetGameInstance();
	const UBotanicusPlantSubsystem* PlantSubsystem =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	if (PlantSubsystem)
	{
		for (TActorIterator<ABotanicusPlantPotActor> PotIt(
				 const_cast<UWorld*>(World));
			 PotIt;
			 ++PotIt)
		{
			if (PotIt->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
				PotIt->ActorHasTag(TEXT("BotanicusInspectionPreview")))
			{
				continue;
			}

			if (const ABotanicusMultiPlantPotActor* MultiPot =
					Cast<ABotanicusMultiPlantPotActor>(*PotIt))
			{
				for (const FBotanicusMultiPlantSlotState& Slot :
					 MultiPot->GetPlantSlots())
				{
					const FBotanicusPlantDefinition* Definition =
						!Slot.PlantKey.IsNone() && !Slot.bElementalDead
							? PlantSubsystem->FindPlant(Slot.PlantKey)
							: nullptr;
					if (Definition)
					{
						AccumulatePlantEnvironmentInfluence(
							*Definition,
							MultiPot->GetActorLocation(),
							WorldLocation,
							Slot.GrowthProgress,
							TemperatureDelta,
							HumidityDelta,
							LuminosityDelta);
					}
				}
				continue;
			}

			const FName PlantKey = PotIt->GetPlantKey();
			const FBotanicusPlantDefinition* Definition =
				!PlantKey.IsNone() && !PotIt->IsElementalDead()
					? PlantSubsystem->FindPlant(PlantKey)
					: nullptr;
			if (Definition)
			{
				AccumulatePlantEnvironmentInfluence(
					*Definition,
					PotIt->GetActorLocation(),
					WorldLocation,
					PotIt->GetGrowthProgress(),
					TemperatureDelta,
					HumidityDelta,
					LuminosityDelta);
			}
		}
	}
	OutTemperatureCelsius = FMath::Clamp(
		OutTemperatureCelsius + TemperatureDelta, -50.0f, 100.0f);
	OutAirHumidityPercent = FMath::Clamp(
		OutAirHumidityPercent + HumidityDelta, 0.0f, 100.0f);
	OutLuminosityPercent = FMath::Clamp(
		OutLuminosityPercent + LuminosityDelta, 0.0f, 100.0f);
	return true;
}
