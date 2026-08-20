// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusBuildingCatalogSubsystem.h"

#include "Building/BotanicusElementalGreenhouseActor.h"

void UBotanicusBuildingCatalogSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UBotanicusBuildingCatalogSettings* Settings =
		GetDefault<UBotanicusBuildingCatalogSettings>();
	if (Settings && !Settings->Catalog.IsNull())
	{
		LoadedCatalog = Settings->Catalog.LoadSynchronous();
	}

	FBotanicusBuildingDefinition& Greenhouse =
		NativeFallbackBuildings.AddDefaulted_GetRef();
	Greenhouse.BuildingKey = TEXT("Greenhouse");
	Greenhouse.DisplayName =
		NSLOCTEXT("BotanicusBuildings", "GreenhouseName", "Serre principale");
	Greenhouse.Description =
		NSLOCTEXT(
			"BotanicusBuildings",
			"GreenhouseDescription",
			"La serre evolutive de la pepiniere. Sa temperature, son humidite et sa luminosite peuvent etre amenagees." );
	Greenhouse.TemplateTag = TEXT("BotanicusTemplate_Greenhouse");
	Greenhouse.LegacyTemplateGroupIndex = 0;
	Greenhouse.FallbackPrefabClass =
		ABotanicusGreenhouseActor::StaticClass();
	Greenhouse.Price = 900;
	Greenhouse.RequiredDevelopmentLevel = 1;

}

const FBotanicusBuildingDefinition*
UBotanicusBuildingCatalogSubsystem::FindBuilding(FName BuildingKey) const
{
	if (LoadedCatalog)
	{
		if (const FBotanicusBuildingDefinition* Definition =
			LoadedCatalog->FindBuilding(BuildingKey))
		{
			return Definition;
		}
	}

	return NativeFallbackBuildings.FindByPredicate(
		[BuildingKey](const FBotanicusBuildingDefinition& Definition)
		{
			return Definition.BuildingKey == BuildingKey;
		});
}

bool UBotanicusBuildingCatalogSubsystem::GetBuildingDefinition(
	FName BuildingKey,
	FBotanicusBuildingDefinition& OutDefinition) const
{
	if (const FBotanicusBuildingDefinition* Definition =
		FindBuilding(BuildingKey))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

TArray<FBotanicusBuildingDefinition>
UBotanicusBuildingCatalogSubsystem::GetAllBuildings() const
{
	TArray<FBotanicusBuildingDefinition> Result;
	if (LoadedCatalog)
	{
		Result = LoadedCatalog->Buildings;
	}

	for (const FBotanicusBuildingDefinition& Fallback :
		 NativeFallbackBuildings)
	{
		if (!Result.ContainsByPredicate(
				[&Fallback](const FBotanicusBuildingDefinition& Existing)
				{
					return Existing.BuildingKey == Fallback.BuildingKey;
				}))
		{
			Result.Add(Fallback);
		}
	}
	return Result;
}
