// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantSubsystem.h"

namespace
{
	FBotanicusPlantEnvironmentRequirements MakeEnvironmentProfile(
		EBotanicusPlantElement Element)
	{
		FBotanicusPlantEnvironmentRequirements Profile;
		switch (Element)
		{
		case EBotanicusPlantElement::Fire:
			Profile.MinimumToleratedTemperatureCelsius = 18.0f;
			Profile.MinimumIdealTemperatureCelsius = 28.0f;
			Profile.MaximumIdealTemperatureCelsius = 38.0f;
			Profile.MaximumToleratedTemperatureCelsius = 48.0f;
			Profile.MinimumToleratedAirHumidityPercent = 5.0f;
			Profile.MinimumIdealAirHumidityPercent = 20.0f;
			Profile.MaximumIdealAirHumidityPercent = 40.0f;
			Profile.MaximumToleratedAirHumidityPercent = 65.0f;
			Profile.MinimumToleratedLuminosityPercent = 40.0f;
			Profile.MinimumIdealLuminosityPercent = 70.0f;
			Profile.MaximumIdealLuminosityPercent = 100.0f;
			Profile.MaximumToleratedLuminosityPercent = 100.0f;
			break;
		case EBotanicusPlantElement::Water:
			Profile.MinimumToleratedTemperatureCelsius = 12.0f;
			Profile.MinimumIdealTemperatureCelsius = 20.0f;
			Profile.MaximumIdealTemperatureCelsius = 27.0f;
			Profile.MaximumToleratedTemperatureCelsius = 35.0f;
			Profile.MinimumToleratedAirHumidityPercent = 40.0f;
			Profile.MinimumIdealAirHumidityPercent = 65.0f;
			Profile.MaximumIdealAirHumidityPercent = 90.0f;
			Profile.MaximumToleratedAirHumidityPercent = 100.0f;
			Profile.MinimumToleratedLuminosityPercent = 20.0f;
			Profile.MinimumIdealLuminosityPercent = 45.0f;
			Profile.MaximumIdealLuminosityPercent = 75.0f;
			Profile.MaximumToleratedLuminosityPercent = 95.0f;
			break;
		case EBotanicusPlantElement::Ice:
			Profile.MinimumToleratedTemperatureCelsius = -5.0f;
			Profile.MinimumIdealTemperatureCelsius = 4.0f;
			Profile.MaximumIdealTemperatureCelsius = 12.0f;
			Profile.MaximumToleratedTemperatureCelsius = 22.0f;
			Profile.MinimumToleratedAirHumidityPercent = 25.0f;
			Profile.MinimumIdealAirHumidityPercent = 45.0f;
			Profile.MaximumIdealAirHumidityPercent = 70.0f;
			Profile.MaximumToleratedAirHumidityPercent = 90.0f;
			Profile.MinimumToleratedLuminosityPercent = 10.0f;
			Profile.MinimumIdealLuminosityPercent = 25.0f;
			Profile.MaximumIdealLuminosityPercent = 55.0f;
			Profile.MaximumToleratedLuminosityPercent = 80.0f;
			break;
		case EBotanicusPlantElement::Shadow:
			Profile.MinimumToleratedTemperatureCelsius = 8.0f;
			Profile.MinimumIdealTemperatureCelsius = 16.0f;
			Profile.MaximumIdealTemperatureCelsius = 24.0f;
			Profile.MaximumToleratedTemperatureCelsius = 32.0f;
			Profile.MinimumToleratedAirHumidityPercent = 35.0f;
			Profile.MinimumIdealAirHumidityPercent = 60.0f;
			Profile.MaximumIdealAirHumidityPercent = 85.0f;
			Profile.MaximumToleratedAirHumidityPercent = 100.0f;
			Profile.MinimumToleratedLuminosityPercent = 0.0f;
			Profile.MinimumIdealLuminosityPercent = 5.0f;
			Profile.MaximumIdealLuminosityPercent = 30.0f;
			Profile.MaximumToleratedLuminosityPercent = 55.0f;
			break;
		default:
			break;
		}
		return Profile;
	}
}

void UBotanicusPlantSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UBotanicusPlantCatalogSettings* Settings =
		GetDefault<UBotanicusPlantCatalogSettings>();
	if (Settings && !Settings->Catalog.IsNull())
	{
		LoadedCatalog = Settings->Catalog.LoadSynchronous();
	}

	const auto AddPlant =
		[this](const TCHAR* Key,
			const TCHAR* Name,
			EBotanicusPlantElement Element)
		{
			FBotanicusPlantDefinition& Plant =
				NativeFallbackPlants.AddDefaulted_GetRef();
			Plant.PlantKey = FName(Key);
			Plant.SeedItemKey =
				FName(*(FString(TEXT("SeedPacket_")) + Key));
			Plant.DisplayName = FText::FromString(Name);
			Plant.HarvestToolItemKey = TEXT("GardenTrowel");
			Plant.HarvestItemKey =
				FName(*(FString(TEXT("Harvest_")) + Key));
			Plant.CompatibleSaleSoilItemKey = TEXT("PottingSoil");
			Plant.HarvestQuantity = 1;
			Plant.HarvestDurationSeconds = 1.0f;
			Plant.GrowthDurationSeconds = 240.0f;
			Plant.MinimumHealthyWater = 0.30f;
			Plant.MaximumHealthyWater = 0.80f;
			Plant.WaterAddedPerUse = 0.30f;
			Plant.WaterConsumptionPerSecond = 0.002f;
			Plant.Element = Element;
			Plant.Environment = MakeEnvironmentProfile(Element);
			Plant.MatureColor =
				Element == EBotanicusPlantElement::Fire
					? FLinearColor(1.0f, 0.12f, 0.01f, 1.0f)
					: Element == EBotanicusPlantElement::Water
						? FLinearColor(0.02f, 0.45f, 1.0f, 1.0f)
						: Element == EBotanicusPlantElement::Ice
							? FLinearColor(0.45f, 0.9f, 1.0f, 1.0f)
							: Element == EBotanicusPlantElement::Shadow
								? FLinearColor(0.16f, 0.01f, 0.28f, 1.0f)
								: FLinearColor(0.42f, 0.72f, 0.20f, 1.0f);

			// Until each variety receives authored meshes, reuse Aurélia's
			// complete four-stage visual instead of reverting to primitives.
			const FString MeshRoot =
				TEXT("/Game/Botanicus/Items/itemsMesh/plantes/normal/")
				TEXT("AuréliaDouce/");
			Plant.SmallGrowthMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(MeshRoot + TEXT("niv1/niv1.niv1")));
			Plant.MediumGrowthMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(MeshRoot + TEXT("niv2/niv2.niv2")));
			Plant.LargeGrowthMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(MeshRoot + TEXT("niv3/niv3.niv3")));
			Plant.MatureGrowthMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(MeshRoot + TEXT("niv4/niv4.niv4")));
		};
	AddPlant(TEXT("AureliaSweet"), TEXT("Aurélia Douce"), EBotanicusPlantElement::Normal);
	AddPlant(TEXT("CoraliaPerchee"), TEXT("Coralia Perchée"), EBotanicusPlantElement::Normal);
	AddPlant(TEXT("CoraliaPompon"), TEXT("Coralia Pompon"), EBotanicusPlantElement::Normal);
	AddPlant(TEXT("Iralia"), TEXT("Iralia"), EBotanicusPlantElement::Normal);
	AddPlant(TEXT("LumineaFlorae"), TEXT("Luminéa Floraë"), EBotanicusPlantElement::Normal);
	AddPlant(TEXT("VerdelaCommune"), TEXT("Verdéla Commune"), EBotanicusPlantElement::Normal);
	AddPlant(TEXT("CoralyneBrumes"), TEXT("Coralyne des brumes"), EBotanicusPlantElement::Water);
	AddPlant(TEXT("HydreaLagunaire"), TEXT("Hydréa lagunaire"), EBotanicusPlantElement::Water);
	AddPlant(TEXT("NerelisEventail"), TEXT("Nérélis éventail"), EBotanicusPlantElement::Water);
	AddPlant(TEXT("OndeliaRuban"), TEXT("Ondélia ruban"), EBotanicusPlantElement::Water);
	AddPlant(TEXT("BonzaiaGivre"), TEXT("Bonzaïa Givré"), EBotanicusPlantElement::Ice);
	AddPlant(TEXT("CristalliaLumifleur"), TEXT("Cristallia lumifleur"), EBotanicusPlantElement::Ice);
	AddPlant(TEXT("GivrelanceAzure"), TEXT("Givrelance azure"), EBotanicusPlantElement::Ice);
	AddPlant(TEXT("GlaceoraPerlee"), TEXT("Glacéora perlée"), EBotanicusPlantElement::Ice);
	AddPlant(TEXT("BraiseliaFlamme"), TEXT("Braiselia flamme"), EBotanicusPlantElement::Fire);
	AddPlant(TEXT("MagmoraSpiral"), TEXT("Magmora spiral"), EBotanicusPlantElement::Fire);
	AddPlant(TEXT("PyrosteleRoyal"), TEXT("Pyrostèle royal"), EBotanicusPlantElement::Fire);
	AddPlant(TEXT("VolcaniaBasiera"), TEXT("Volcania basiera"), EBotanicusPlantElement::Fire);
	AddPlant(TEXT("Noctepine"), TEXT("Noctépine"), EBotanicusPlantElement::Shadow);
	AddPlant(TEXT("NoctivoraVentouse"), TEXT("Noctivora ventouse"), EBotanicusPlantElement::Shadow);
	AddPlant(TEXT("OmbraeLuridia"), TEXT("Ombrae luridia"), EBotanicusPlantElement::Shadow);
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlant(FName PlantKey) const
{
	// Keep the native list as the authoritative whitelist, but let the
	// designer-authored Data Asset provide the editable gameplay values.
	const FBotanicusPlantDefinition* NativeDefinition =
		NativeFallbackPlants.FindByPredicate(
		[PlantKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.PlantKey == PlantKey;
		});
	if (!NativeDefinition)
	{
		return nullptr;
	}
	if (LoadedCatalog)
	{
		if (const FBotanicusPlantDefinition* AuthoredDefinition =
				LoadedCatalog->FindPlant(PlantKey))
		{
			return AuthoredDefinition;
		}
	}
	return NativeDefinition;
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlantBySeed(FName SeedItemKey) const
{
	const FBotanicusPlantDefinition* NativeDefinition =
		NativeFallbackPlants.FindByPredicate(
		[SeedItemKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.SeedItemKey == SeedItemKey;
		});
	return NativeDefinition
		? FindPlant(NativeDefinition->PlantKey)
		: nullptr;
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlantByHarvestItem(
	FName HarvestItemKey) const
{
	FString NormalizedKey = HarvestItemKey.ToString();
	NormalizedKey.RemoveFromEnd(TEXT("_Beautiful"));
	NormalizedKey.RemoveFromEnd(TEXT("_Exceptional"));
	const FName BaseHarvestItemKey(*NormalizedKey);
	const FBotanicusPlantDefinition* NativeDefinition =
		NativeFallbackPlants.FindByPredicate(
		[BaseHarvestItemKey](
			const FBotanicusPlantDefinition& Definition)
		{
			return Definition.HarvestItemKey == BaseHarvestItemKey;
		});
	return NativeDefinition
		? FindPlant(NativeDefinition->PlantKey)
		: nullptr;
}

TArray<FBotanicusPlantDefinition>
UBotanicusPlantSubsystem::GetAllPlants() const
{
	TArray<FBotanicusPlantDefinition> Result;
	Result.Reserve(NativeFallbackPlants.Num());
	for (const FBotanicusPlantDefinition& NativeDefinition :
		 NativeFallbackPlants)
	{
		if (const FBotanicusPlantDefinition* Definition =
				FindPlant(NativeDefinition.PlantKey))
		{
			Result.Add(*Definition);
		}
	}
	return Result;
}
