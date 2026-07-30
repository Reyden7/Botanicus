// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusItemCatalogSubsystem.h"

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
	SmallTest.DeliveryQuantity = 10;

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
