// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantSubsystem.h"

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

	FBotanicusPlantDefinition& Basil =
		NativeFallbackPlants.AddDefaulted_GetRef();
	Basil.PlantKey = TEXT("Basil");
	Basil.SeedItemKey = TEXT("SeedPacket_Basil");
	Basil.HarvestToolItemKey = TEXT("GardenTrowel");
	Basil.HarvestItemKey = TEXT("Harvest_Basil");
	Basil.CompatibleSaleSoilItemKey = TEXT("PottingSoil");
	Basil.HarvestQuantity = 1;
	Basil.HarvestDurationSeconds = 1.0f;
	Basil.DisplayName =
		NSLOCTEXT("BotanicusGrowing", "BasilName", "Basilic");
	Basil.GrowthDurationSeconds = 120.0f;
	Basil.MinimumHealthyWater = 0.25f;
	Basil.MaximumHealthyWater = 0.9f;
	Basil.WaterAddedPerUse = 0.35f;
	Basil.WaterConsumptionPerSecond = 0.003f;

	const auto AddPlant =
		[this](
			FName PlantKey,
			FName SeedKey,
			const FText& DisplayName,
			FName HarvestKey,
			float GrowthSeconds,
			float MinimumWater,
			float MaximumWater,
			float WaterPerUse,
			float WaterConsumption,
			const FLinearColor& MatureColor)
		{
			FBotanicusPlantDefinition& Plant =
				NativeFallbackPlants.AddDefaulted_GetRef();
			Plant.PlantKey = PlantKey;
			Plant.SeedItemKey = SeedKey;
			Plant.DisplayName = DisplayName;
			Plant.HarvestToolItemKey = TEXT("GardenTrowel");
			Plant.HarvestItemKey = HarvestKey;
			Plant.CompatibleSaleSoilItemKey = TEXT("PottingSoil");
			Plant.HarvestQuantity = 1;
			Plant.HarvestDurationSeconds = 1.0f;
			Plant.GrowthDurationSeconds = GrowthSeconds;
			Plant.MinimumHealthyWater = MinimumWater;
			Plant.MaximumHealthyWater = MaximumWater;
			Plant.WaterAddedPerUse = WaterPerUse;
			Plant.WaterConsumptionPerSecond = WaterConsumption;
			Plant.MatureColor = MatureColor;
		};
	AddPlant(
		TEXT("Orchid"),
		TEXT("SeedPacket_Orchid"),
		NSLOCTEXT("BotanicusGrowing", "OrchidName", "Orchidee rose"),
		TEXT("Harvest_Orchid"),
		240.0f,
		0.45f,
		0.75f,
		0.28f,
		0.004f,
		FLinearColor(0.95f, 0.22f, 0.58f, 1.0f));
	AddPlant(
		TEXT("Monstera"),
		TEXT("SeedPacket_Monstera"),
		NSLOCTEXT("BotanicusGrowing", "MonsteraName", "Monstera"),
		TEXT("Harvest_Monstera"),
		300.0f,
		0.30f,
		0.82f,
		0.32f,
		0.002f,
		FLinearColor(0.05f, 0.42f, 0.16f, 1.0f));
	AddPlant(
		TEXT("Lavender"),
		TEXT("SeedPacket_Lavender"),
		NSLOCTEXT("BotanicusGrowing", "LavenderName", "Lavande violette"),
		TEXT("Harvest_Lavender"),
		180.0f,
		0.18f,
		0.62f,
		0.25f,
		0.0015f,
		FLinearColor(0.48f, 0.20f, 0.78f, 1.0f));
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlant(FName PlantKey) const
{
	if (LoadedCatalog)
	{
		if (const FBotanicusPlantDefinition* Definition =
			LoadedCatalog->FindPlant(PlantKey))
		{
			return Definition;
		}
	}
	return NativeFallbackPlants.FindByPredicate(
		[PlantKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.PlantKey == PlantKey;
		});
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlantBySeed(FName SeedItemKey) const
{
	if (LoadedCatalog)
	{
		if (const FBotanicusPlantDefinition* Definition =
			LoadedCatalog->FindPlantBySeed(SeedItemKey))
		{
			return Definition;
		}
	}
	return NativeFallbackPlants.FindByPredicate(
		[SeedItemKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.SeedItemKey == SeedItemKey;
		});
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlantByHarvestItem(
	FName HarvestItemKey) const
{
	FString NormalizedKey = HarvestItemKey.ToString();
	NormalizedKey.RemoveFromEnd(TEXT("_Beautiful"));
	NormalizedKey.RemoveFromEnd(TEXT("_Exceptional"));
	const FName BaseHarvestItemKey(*NormalizedKey);
	if (LoadedCatalog)
	{
		if (const FBotanicusPlantDefinition* Definition =
			LoadedCatalog->FindPlantByHarvestItem(BaseHarvestItemKey))
		{
			return Definition;
		}
	}
	return NativeFallbackPlants.FindByPredicate(
		[BaseHarvestItemKey](
			const FBotanicusPlantDefinition& Definition)
		{
			return Definition.HarvestItemKey == BaseHarvestItemKey;
		});
}

TArray<FBotanicusPlantDefinition>
UBotanicusPlantSubsystem::GetAllPlants() const
{
	TArray<FBotanicusPlantDefinition> Result;
	if (LoadedCatalog)
	{
		Result = LoadedCatalog->Plants;
	}
	for (const FBotanicusPlantDefinition& Fallback : NativeFallbackPlants)
	{
		if (!Result.ContainsByPredicate(
				[&Fallback](const FBotanicusPlantDefinition& Existing)
				{
					return Existing.PlantKey == Fallback.PlantKey;
				}))
		{
			Result.Add(Fallback);
		}
	}
	return Result;
}
