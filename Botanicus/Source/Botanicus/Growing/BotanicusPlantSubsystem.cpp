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
	Basil.DisplayName =
		NSLOCTEXT("BotanicusGrowing", "BasilName", "Basilic");
	Basil.GrowthDurationSeconds = 120.0f;
	Basil.MinimumHealthyWater = 0.25f;
	Basil.MaximumHealthyWater = 0.9f;
	Basil.WaterAddedPerUse = 0.35f;
	Basil.WaterConsumptionPerSecond = 0.003f;
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
