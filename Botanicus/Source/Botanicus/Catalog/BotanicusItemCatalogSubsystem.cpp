// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusItemCatalogSubsystem.h"

#include "Growing/BotanicusPlantPotActor.h"

void UBotanicusItemCatalogSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UBotanicusItemCatalogSettings* Settings =
		GetDefault<UBotanicusItemCatalogSettings>();
	if (Settings && !Settings->Catalog.IsNull())
	{
		LoadedCatalog = Settings->Catalog.LoadSynchronous();
	}

	FBotanicusItemDefinition& SmallTest =
		NativeFallbackItems.AddDefaulted_GetRef();
	SmallTest.ItemKey = TEXT("SeedPacket_Test");
	SmallTest.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "SeedPacketTest", "Paquet de graines test");
	SmallTest.Category = EBotanicusItemCategory::Decoration;
	SmallTest.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	SmallTest.WorldScale = FVector(0.4f);
	SmallTest.MaximumStack = 99;
	SmallTest.WeightClass = EBotanicusItemWeightClass::Hotbar;
	SmallTest.Price = 25;
	SmallTest.DeliveryQuantity = 10;
	SmallTest.DeliveryDelaySeconds = 3.0f;

	FBotanicusItemDefinition& LargeTest =
		NativeFallbackItems.AddDefaulted_GetRef();
	LargeTest.ItemKey = TEXT("LargeEquipment_Test");
	LargeTest.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "LargeEquipmentTest", "Gros equipement test");
	LargeTest.Category = EBotanicusItemCategory::Equipment;
	LargeTest.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	LargeTest.WorldScale = FVector(1.0f, 0.65f, 0.75f);
	LargeTest.MaximumStack = 1;
	LargeTest.WeightClass =
		EBotanicusItemWeightClass::TwoPlayerCarry;
	LargeTest.CarryMovementSpeedMultiplier = 0.55f;
	LargeTest.Price = 600;
	LargeTest.DeliveryDelaySeconds = 10.0f;

	FBotanicusItemDefinition& SoloLargeTest =
		NativeFallbackItems.AddDefaulted_GetRef();
	SoloLargeTest.ItemKey = TEXT("LargeEquipmentSolo_Test");
	SoloLargeTest.DisplayName =
		NSLOCTEXT(
			"BotanicusCatalog",
			"LargeEquipmentSoloTest",
			"Gros equipement solo test");
	SoloLargeTest.Category = EBotanicusItemCategory::Equipment;
	SoloLargeTest.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	SoloLargeTest.WorldScale = FVector(0.8f, 0.55f, 0.65f);
	SoloLargeTest.MaximumStack = 1;
	SoloLargeTest.WeightClass =
		EBotanicusItemWeightClass::OnePlayerCarry;
	SoloLargeTest.CarryMovementSpeedMultiplier = 0.7f;
	SoloLargeTest.Price = 350;
	SoloLargeTest.DeliveryDelaySeconds = 6.0f;

	FBotanicusItemDefinition& PlantPot =
		NativeFallbackItems.AddDefaulted_GetRef();
	PlantPot.ItemKey = TEXT("PlantPot");
	PlantPot.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "PlantPot", "Pot de culture");
	PlantPot.Category = EBotanicusItemCategory::Decoration;
	PlantPot.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	PlantPot.WorldScale = FVector(0.42f, 0.42f, 0.34f);
	PlantPot.WorldActorClass = ABotanicusPlantPotActor::StaticClass();
	PlantPot.MaximumStack = 10;
	PlantPot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	PlantPot.Price = 75;
	PlantPot.DeliveryQuantity = 1;
	PlantPot.DeliveryDelaySeconds = 2.0f;

	FBotanicusItemDefinition& PottingSoil =
		NativeFallbackItems.AddDefaulted_GetRef();
	PottingSoil.ItemKey = TEXT("PottingSoil");
	PottingSoil.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "PottingSoil", "Dose de terreau");
	PottingSoil.Category = EBotanicusItemCategory::Supply;
	PottingSoil.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	PottingSoil.WorldScale = FVector(0.24f);
	PottingSoil.MaximumStack = 20;
	PottingSoil.WeightClass = EBotanicusItemWeightClass::Hotbar;
	PottingSoil.Price = 25;
	PottingSoil.DeliveryQuantity = 5;
	PottingSoil.DeliveryDelaySeconds = 2.0f;
	PottingSoil.AllowedPlacementSurfaces = 0;

	FBotanicusItemDefinition& BasilSeeds =
		NativeFallbackItems.AddDefaulted_GetRef();
	BasilSeeds.ItemKey = TEXT("SeedPacket_Basil");
	BasilSeeds.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "BasilSeeds", "Graines de basilic");
	BasilSeeds.Category = EBotanicusItemCategory::Supply;
	BasilSeeds.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	BasilSeeds.WorldScale = FVector(0.18f, 0.08f, 0.24f);
	BasilSeeds.MaximumStack = 20;
	BasilSeeds.WeightClass = EBotanicusItemWeightClass::Hotbar;
	BasilSeeds.Price = 30;
	BasilSeeds.DeliveryQuantity = 5;
	BasilSeeds.DeliveryDelaySeconds = 2.0f;
	BasilSeeds.AllowedPlacementSurfaces = 0;

	FBotanicusItemDefinition& WateringCan =
		NativeFallbackItems.AddDefaulted_GetRef();
	WateringCan.ItemKey = TEXT("WateringCan");
	WateringCan.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "WateringCan", "Arrosoir");
	WateringCan.Category = EBotanicusItemCategory::Tool;
	WateringCan.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	WateringCan.WorldScale = FVector(0.38f, 0.2f, 0.28f);
	WateringCan.MaximumStack = 1;
	WateringCan.WeightClass = EBotanicusItemWeightClass::Hotbar;
	WateringCan.Price = 120;
	WateringCan.DeliveryQuantity = 1;
	WateringCan.DeliveryDelaySeconds = 2.0f;
	WateringCan.AllowedPlacementSurfaces = 0;
}

const FBotanicusItemDefinition*
UBotanicusItemCatalogSubsystem::FindItem(FName ItemKey) const
{
	if (LoadedCatalog)
	{
		if (const FBotanicusItemDefinition* Definition =
			LoadedCatalog->FindItem(ItemKey))
		{
			return Definition;
		}
	}

	return NativeFallbackItems.FindByPredicate(
		[ItemKey](const FBotanicusItemDefinition& Definition)
		{
			return Definition.ItemKey == ItemKey;
		});
}

bool UBotanicusItemCatalogSubsystem::GetItemDefinition(
	FName ItemKey,
	FBotanicusItemDefinition& OutDefinition) const
{
	if (const FBotanicusItemDefinition* Definition =
		FindItem(ItemKey))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

TArray<FBotanicusItemDefinition>
UBotanicusItemCatalogSubsystem::GetAllItems() const
{
	TArray<FBotanicusItemDefinition> Result;
	if (LoadedCatalog)
	{
		Result = LoadedCatalog->Items;
	}

	for (const FBotanicusItemDefinition& Fallback : NativeFallbackItems)
	{
		if (!Result.ContainsByPredicate(
				[&Fallback](const FBotanicusItemDefinition& Existing)
				{
					return Existing.ItemKey == Fallback.ItemKey;
				}))
		{
			Result.Add(Fallback);
		}
	}
	return Result;
}
