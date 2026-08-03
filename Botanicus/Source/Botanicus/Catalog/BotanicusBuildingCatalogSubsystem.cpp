// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusBuildingCatalogSubsystem.h"

#include "Building/BotanicusCatalogBuildingActor.h"
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

	FBotanicusBuildingDefinition& Compact =
		NativeFallbackBuildings.AddDefaulted_GetRef();
	Compact.BuildingKey = TEXT("GreenhouseCompact");
	Compact.DisplayName =
		NSLOCTEXT("BotanicusBuildings", "CompactName", "Serre compacte");
	Compact.Description =
		NSLOCTEXT(
			"BotanicusBuildings",
			"CompactDescription",
			"Une serre fonctionnelle pour démarrer une petite production.");
	Compact.TemplateTag = TEXT("BotanicusTemplate_GreenhouseCompact");
	Compact.LegacyTemplateGroupIndex = 0;
	Compact.FallbackPrefabClass =
		ABotanicusCompactGreenhouseActor::StaticClass();
	Compact.Price = 900;
	Compact.RequiredDevelopmentLevel = 1;

	FBotanicusBuildingDefinition& Workshop =
		NativeFallbackBuildings.AddDefaulted_GetRef();
	Workshop.BuildingKey = TEXT("GreenhouseWorkshop");
	Workshop.DisplayName =
		NSLOCTEXT("BotanicusBuildings", "WorkshopName", "Serre atelier");
	Workshop.Description =
		NSLOCTEXT(
			"BotanicusBuildings",
			"WorkshopDescription",
			"Un bâtiment plus vaste adapté aux installations avancées.");
	Workshop.TemplateTag = TEXT("BotanicusTemplate_GreenhouseWorkshop");
	Workshop.LegacyTemplateGroupIndex = 1;
	Workshop.FallbackPrefabClass =
		ABotanicusWorkshopGreenhouseActor::StaticClass();
	Workshop.Price = 1400;
	Workshop.RequiredDevelopmentLevel = 2;

	const auto AddElementalGreenhouse =
		[this](
			FName BuildingKey,
			const FText& DisplayName,
			const FText& Description,
			FName TemplateTag,
			int32 LegacyIndex,
			TSubclassOf<ABotanicusCatalogBuildingActor> ActorClass,
			int32 Price,
			int32 RequiredLevel)
		{
			FBotanicusBuildingDefinition& Building =
				NativeFallbackBuildings.AddDefaulted_GetRef();
			Building.BuildingKey = BuildingKey;
			Building.DisplayName = DisplayName;
			Building.Description = Description;
			Building.TemplateTag = TemplateTag;
			Building.LegacyTemplateGroupIndex = LegacyIndex;
			Building.FallbackPrefabClass = ActorClass;
			Building.Price = Price;
			Building.RequiredDevelopmentLevel = RequiredLevel;
		};
	AddElementalGreenhouse(
		TEXT("GreenhouseFire"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"FireGreenhouseName",
			"Serre de feu"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"FireGreenhouseDescription",
			"Milieu securise indispensable aux fleurs de feu. Ameliorable jusqu'au niveau 3."),
		TEXT("BotanicusTemplate_GreenhouseFire"),
		2,
		ABotanicusFireGreenhouseActor::StaticClass(),
		1800,
		2);
	AddElementalGreenhouse(
		TEXT("GreenhouseWater"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"WaterGreenhouseName",
			"Serre d'eau"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"WaterGreenhouseDescription",
			"Milieu humide indispensable aux fleurs d'eau. Ameliorable jusqu'au niveau 3."),
		TEXT("BotanicusTemplate_GreenhouseWater"),
		3,
		ABotanicusWaterGreenhouseActor::StaticClass(),
		1800,
		2);
	AddElementalGreenhouse(
		TEXT("GreenhouseIce"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"IceGreenhouseName",
			"Serre de glace"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"IceGreenhouseDescription",
			"Milieu froid indispensable aux fleurs de glace. Ameliorable jusqu'au niveau 3."),
		TEXT("BotanicusTemplate_GreenhouseIce"),
		4,
		ABotanicusIceGreenhouseActor::StaticClass(),
		2000,
		3);
	AddElementalGreenhouse(
		TEXT("GreenhouseShadow"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"ShadowGreenhouseName",
			"Serre des tenebres"),
		NSLOCTEXT(
			"BotanicusBuildings",
			"ShadowGreenhouseDescription",
			"Milieu obscur indispensable aux fleurs des tenebres. Ameliorable jusqu'au niveau 3."),
		TEXT("BotanicusTemplate_GreenhouseShadow"),
		5,
		ABotanicusShadowGreenhouseActor::StaticClass(),
		2200,
		3);
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
