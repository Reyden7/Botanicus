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

	FBotanicusPlantDiseaseDefinition MakeDisease(
		const TCHAR* Key,
		const TCHAR* Name,
		const TCHAR* Cause,
		const TCHAR* Treatment,
		const TCHAR* TreatmentItemKey,
		bool bElementSpecific = false,
		EBotanicusPlantElement Element = EBotanicusPlantElement::Normal)
	{
		FBotanicusPlantDiseaseDefinition Result;
		Result.DiseaseKey = FName(Key);
		Result.DisplayName = FText::FromString(Name);
		Result.CauseDescription = FText::FromString(Cause);
		Result.TreatmentDescription = FText::FromString(Treatment);
		Result.TreatmentItemKey = FName(TreatmentItemKey);
		Result.bElementSpecific = bElementSpecific;
		Result.Element = Element;
		return Result;
	}

	void AdvanceExposure(
		float& Exposure,
		bool bExposed,
		float DeltaSeconds)
	{
		Exposure = bExposed
			? Exposure + FMath::Max(0.0f, DeltaSeconds)
			: 0.0f;
	}
}

const TArray<FBotanicusPlantDiseaseDefinition>&
GetBotanicusPlantDiseaseDefinitions()
{
	static const TArray<FBotanicusPlantDiseaseDefinition> Definitions = {
		MakeDisease(TEXT("Disease_Element_Normal"),
			TEXT("Dépérissement chlorotique"),
			TEXT("Mauvais entretien prolongé d'une plante normale."),
			TEXT("Rétablir ses conditions idéales puis appliquer un fortifiant végétal."),
			TEXT("TreatmentElementNormal"), true, EBotanicusPlantElement::Normal),
		MakeDisease(TEXT("Disease_Element_Fire"),
			TEXT("Cendre froide"),
			TEXT("Une plante de feu reste trop longtemps hors de ses conditions idéales."),
			TEXT("Remonter progressivement la température et appliquer un élixir igné."),
			TEXT("TreatmentElementFire"), true, EBotanicusPlantElement::Fire),
		MakeDisease(TEXT("Disease_Element_Water"),
			TEXT("Nécrose saumâtre"),
			TEXT("Une plante d'eau subit un environnement inadapté pendant trop longtemps."),
			TEXT("Stabiliser humidité et température puis appliquer un sérum aquatique."),
			TEXT("TreatmentElementWater"), true, EBotanicusPlantElement::Water),
		MakeDisease(TEXT("Disease_Element_Ice"),
			TEXT("Dégel cristallin"),
			TEXT("Une plante de glace reste trop longtemps dans un climat défavorable."),
			TEXT("La refroidir progressivement puis appliquer un baume cryogénique."),
			TEXT("TreatmentElementIce"), true, EBotanicusPlantElement::Ice),
		MakeDisease(TEXT("Disease_Element_Shadow"),
			TEXT("Pâlissement d'ombre"),
			TEXT("Une plante de ténèbres est durablement privée de son climat adapté."),
			TEXT("Réduire la lumière et appliquer une essence d'ombre."),
			TEXT("TreatmentElementShadow"), true, EBotanicusPlantElement::Shadow),
		MakeDisease(TEXT("Disease_Overwatering"),
			TEXT("Pourriture des racines"),
			TEXT("Le terreau est resté saturé en eau trop longtemps."),
			TEXT("Laisser sécher le terreau puis appliquer une poudre drainante."),
			TEXT("TreatmentRootRot")),
		MakeDisease(TEXT("Disease_Humidity"),
			TEXT("Stress hygrométrique"),
			TEXT("L'humidité de l'air est restée hors de la plage idéale."),
			TEXT("Rétablir l'humidité idéale puis appliquer un sérum hygrométrique."),
			TEXT("TreatmentHumidity")),
		MakeDisease(TEXT("Disease_ExcessLight"),
			TEXT("Brûlure lumineuse"),
			TEXT("La plante a reçu trop de lumière pendant une longue durée."),
			TEXT("Créer de l'ombre puis appliquer un baume réparateur."),
			TEXT("TreatmentLightBurn"))};
	return Definitions;
}

const FBotanicusPlantDiseaseDefinition* FindBotanicusPlantDisease(
	FName DiseaseKey)
{
	return GetBotanicusPlantDiseaseDefinitions().FindByPredicate(
		[DiseaseKey](const FBotanicusPlantDiseaseDefinition& Definition)
		{
			return Definition.DiseaseKey == DiseaseKey;
		});
}

FName GetBotanicusElementDiseaseKey(EBotanicusPlantElement Element)
{
	switch (Element)
	{
	case EBotanicusPlantElement::Fire: return TEXT("Disease_Element_Fire");
	case EBotanicusPlantElement::Water: return TEXT("Disease_Element_Water");
	case EBotanicusPlantElement::Ice: return TEXT("Disease_Element_Ice");
	case EBotanicusPlantElement::Shadow: return TEXT("Disease_Element_Shadow");
	default: return TEXT("Disease_Element_Normal");
	}
}

bool UpdateBotanicusPlantDiseaseState(
	FBotanicusPlantDiseaseState& State,
	const FBotanicusPlantDiseaseSusceptibility& Susceptibility,
	EBotanicusPlantElement Element,
	float WaterLevel,
	float MaximumHealthyWater,
	const FBotanicusPlantEnvironmentState& EnvironmentState,
	float DeltaSeconds,
	TArray<FName>& OutNewDiseaseKeys)
{
	OutNewDiseaseKeys.Reset();
	AdvanceExposure(State.ElementNeglectSeconds,
		EnvironmentState.bEnvironmentAvailable &&
			EnvironmentState.OverallComfort < 0.40f, DeltaSeconds);
	AdvanceExposure(State.OverwateringSeconds,
		WaterLevel > MaximumHealthyWater, DeltaSeconds);
	AdvanceExposure(State.IncorrectHumiditySeconds,
		EnvironmentState.bEnvironmentAvailable &&
			EnvironmentState.AirHumidityCondition !=
				EBotanicusPlantEnvironmentCondition::Ideal, DeltaSeconds);
	AdvanceExposure(State.ExcessLightSeconds,
		EnvironmentState.bEnvironmentAvailable &&
			EnvironmentState.LuminosityCondition ==
				EBotanicusPlantEnvironmentCondition::TooHigh, DeltaSeconds);

	const auto AddIfTriggered = [&State, &OutNewDiseaseKeys](
		FName DiseaseKey, float Exposure, float Delay)
	{
		if (Exposure >= FMath::Max(1.0f, Delay) &&
			!State.ActiveDiseaseKeys.Contains(DiseaseKey))
		{
			State.ActiveDiseaseKeys.Add(DiseaseKey);
			OutNewDiseaseKeys.Add(DiseaseKey);
		}
	};
	AddIfTriggered(GetBotanicusElementDiseaseKey(Element),
		State.ElementNeglectSeconds,
		Susceptibility.ElementNeglectDelaySeconds);
	AddIfTriggered(TEXT("Disease_Overwatering"), State.OverwateringSeconds,
		Susceptibility.OverwateringDelaySeconds);
	AddIfTriggered(TEXT("Disease_Humidity"), State.IncorrectHumiditySeconds,
		Susceptibility.IncorrectHumidityDelaySeconds);
	AddIfTriggered(TEXT("Disease_ExcessLight"), State.ExcessLightSeconds,
		Susceptibility.ExcessLightDelaySeconds);
	return !OutNewDiseaseKeys.IsEmpty();
}

bool ApplyBotanicusPlantDiseaseTreatment(
	FBotanicusPlantDiseaseState& State,
	FName TreatmentItemKey,
	FName& OutCuredDiseaseKey)
{
	OutCuredDiseaseKey = NAME_None;
	if (TreatmentItemKey.IsNone())
	{
		return false;
	}
	for (int32 Index = State.ActiveDiseaseKeys.Num() - 1;
		 Index >= 0;
		 --Index)
	{
		const FName DiseaseKey = State.ActiveDiseaseKeys[Index];
		const FBotanicusPlantDiseaseDefinition* Disease =
			FindBotanicusPlantDisease(DiseaseKey);
		if (!Disease || Disease->TreatmentItemKey != TreatmentItemKey)
		{
			continue;
		}
		State.ActiveDiseaseKeys.RemoveAt(Index);
		OutCuredDiseaseKey = DiseaseKey;
		if (DiseaseKey == TEXT("Disease_Overwatering"))
		{
			State.OverwateringSeconds = 0.0f;
		}
		else if (DiseaseKey == TEXT("Disease_Humidity"))
		{
			State.IncorrectHumiditySeconds = 0.0f;
		}
		else if (DiseaseKey == TEXT("Disease_ExcessLight"))
		{
			State.ExcessLightSeconds = 0.0f;
		}
		else
		{
			State.ElementNeglectSeconds = 0.0f;
		}
		return true;
	}
	return false;
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
