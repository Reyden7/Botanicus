// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusItemCatalogSubsystem.h"

#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"
#include "Preparation/BotanicusComputerActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"

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
	LargeTest.bPurchasable = false;
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
	SoloLargeTest.bPurchasable = false;
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
	PottingSoil.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Preparation) |
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	PottingSoil.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	PottingSoil.WorldScale = FVector(0.24f);
	PottingSoil.MaximumStack = 20;
	PottingSoil.bSaleSoil = true;
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
	BasilSeeds.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Seeds);
	BasilSeeds.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	BasilSeeds.WorldScale = FVector(0.18f, 0.08f, 0.24f);
	BasilSeeds.MaximumStack = 20;
	BasilSeeds.WeightClass = EBotanicusItemWeightClass::Hotbar;
	BasilSeeds.Price = 30;
	BasilSeeds.DeliveryQuantity = 5;
	BasilSeeds.DeliveryDelaySeconds = 2.0f;
	BasilSeeds.AllowedPlacementSurfaces = 0;

	const auto AddSeedDefinition =
		[this](
			FName ItemKey,
			const FText& DisplayName,
			int32 Price,
			int32 Quantity)
		{
			FBotanicusItemDefinition& Seeds =
				NativeFallbackItems.AddDefaulted_GetRef();
			Seeds.ItemKey = ItemKey;
			Seeds.DisplayName = DisplayName;
			Seeds.Category = EBotanicusItemCategory::Supply;
			Seeds.CatalogTabs =
				static_cast<int32>(EBotanicusCatalogTab::Seeds);
			Seeds.WorldMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
			Seeds.WorldScale = FVector(0.18f, 0.08f, 0.24f);
			Seeds.MaximumStack = 20;
			Seeds.WeightClass =
				EBotanicusItemWeightClass::Hotbar;
			Seeds.Price = Price;
			Seeds.DeliveryQuantity = Quantity;
			Seeds.DeliveryDelaySeconds = 2.5f;
			Seeds.AllowedPlacementSurfaces = 0;
		};
	AddSeedDefinition(
		TEXT("SeedPacket_Orchid"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"OrchidSeeds",
			"Graines d'orchidee rose"),
		45,
		3);
	AddSeedDefinition(
		TEXT("SeedPacket_Monstera"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"MonsteraSeeds",
			"Graines de monstera"),
		40,
		3);
	AddSeedDefinition(
		TEXT("SeedPacket_Lavender"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"LavenderSeeds",
			"Graines de lavande"),
		35,
		5);

	FBotanicusItemDefinition& WateringCan =
		NativeFallbackItems.AddDefaulted_GetRef();
	WateringCan.ItemKey = TEXT("WateringCan");
	WateringCan.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "WateringCan", "Arrosoir");
	WateringCan.Category = EBotanicusItemCategory::Tool;
	WateringCan.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::GardeningTools);
	WateringCan.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	WateringCan.WorldScale = FVector(0.38f, 0.2f, 0.28f);
	WateringCan.WorldActorClass =
		ABotanicusWateringCanActor::StaticClass();
	WateringCan.MaximumStack = 1;
	WateringCan.WeightClass = EBotanicusItemWeightClass::Handheld;
	WateringCan.Price = 120;
	WateringCan.DeliveryQuantity = 1;
	WateringCan.DeliveryDelaySeconds = 2.0f;
	WateringCan.AllowedPlacementSurfaces = 0;

	FBotanicusItemDefinition& WaterReserve =
		NativeFallbackItems.AddDefaulted_GetRef();
	WaterReserve.ItemKey = TEXT("WaterReserve");
	WaterReserve.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "WaterReserve", "Reserve d'eau");
	WaterReserve.Category = EBotanicusItemCategory::Equipment;
	WaterReserve.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::GardeningTools);
	WaterReserve.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	WaterReserve.WorldScale = FVector(0.55f, 0.55f, 0.8f);
	WaterReserve.WorldActorClass =
		ABotanicusWaterReserveActor::StaticClass();
	WaterReserve.MaximumStack = 2;
	WaterReserve.WeightClass = EBotanicusItemWeightClass::Hotbar;
	WaterReserve.Price = 250;
	WaterReserve.DeliveryQuantity = 1;
	WaterReserve.DeliveryDelaySeconds = 3.0f;

	FBotanicusItemDefinition& GardenTrowel =
		NativeFallbackItems.AddDefaulted_GetRef();
	GardenTrowel.ItemKey = TEXT("GardenTrowel");
	GardenTrowel.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "GardenTrowel", "Petite pelle");
	GardenTrowel.Category = EBotanicusItemCategory::Tool;
	GardenTrowel.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::GardeningTools);
	GardenTrowel.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	GardenTrowel.WorldScale = FVector(0.08f, 0.32f, 0.08f);
	GardenTrowel.MaximumStack = 1;
	GardenTrowel.WeightClass = EBotanicusItemWeightClass::Hotbar;
	GardenTrowel.Price = 90;
	GardenTrowel.DeliveryQuantity = 1;
	GardenTrowel.DeliveryDelaySeconds = 2.0f;
	GardenTrowel.AllowedPlacementSurfaces = 0;

	FBotanicusItemDefinition& BoxCutter =
		NativeFallbackItems.AddDefaulted_GetRef();
	BoxCutter.ItemKey = TEXT("BoxCutter");
	BoxCutter.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "BoxCutter", "Cutter");
	BoxCutter.Category = EBotanicusItemCategory::Tool;
	BoxCutter.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::GardeningTools);
	BoxCutter.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	BoxCutter.WorldScale = FVector(0.20f, 0.045f, 0.035f);
	BoxCutter.MaximumStack = 1;
	BoxCutter.WeightClass = EBotanicusItemWeightClass::Hotbar;
	BoxCutter.Price = 35;
	BoxCutter.DeliveryQuantity = 1;
	BoxCutter.DeliveryDelaySeconds = 1.5f;
	BoxCutter.AllowedPlacementSurfaces = 0;

	FBotanicusItemDefinition& BasilHarvest =
		NativeFallbackItems.AddDefaulted_GetRef();
	BasilHarvest.ItemKey = TEXT("Harvest_Basil");
	BasilHarvest.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "BasilHarvest", "Plant de basilic");
	BasilHarvest.Category = EBotanicusItemCategory::Supply;
	BasilHarvest.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	BasilHarvest.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	BasilHarvest.WorldScale = FVector(0.16f);
	BasilHarvest.MaximumStack = 1;
	BasilHarvest.bWholePlant = true;
	BasilHarvest.WeightClass = EBotanicusItemWeightClass::Hotbar;
	BasilHarvest.bPurchasable = false;
	BasilHarvest.SalePrice = 60;
	BasilHarvest.PlantColorTag = TEXT("Green");
	BasilHarvest.PlantTypeTag = TEXT("Aromatic");
	BasilHarvest.VisitorAppeal = 62;
	BasilHarvest.AllowedPlacementSurfaces = 0;

	const auto AddHarvestDefinition =
		[this](
			FName ItemKey,
			const FText& DisplayName,
			int32 SalePrice,
			FName ColorTag,
			FName TypeTag,
			int32 Appeal)
		{
			FBotanicusItemDefinition& Harvest =
				NativeFallbackItems.AddDefaulted_GetRef();
			Harvest.ItemKey = ItemKey;
			Harvest.DisplayName = DisplayName;
			Harvest.Category = EBotanicusItemCategory::Supply;
			Harvest.CatalogTabs =
				static_cast<int32>(EBotanicusCatalogTab::Sales);
			Harvest.WorldMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Harvest.WorldScale = FVector(0.16f);
			Harvest.MaximumStack = 1;
			Harvest.bWholePlant = true;
			Harvest.WeightClass =
				EBotanicusItemWeightClass::Hotbar;
			Harvest.bPurchasable = false;
			Harvest.SalePrice = SalePrice;
			Harvest.PlantColorTag = ColorTag;
			Harvest.PlantTypeTag = TypeTag;
			Harvest.VisitorAppeal = Appeal;
			Harvest.AllowedPlacementSurfaces = 0;
		};
	AddHarvestDefinition(
		TEXT("Harvest_Orchid"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"OrchidHarvest",
			"Orchidee rose"),
		110,
		TEXT("Pink"),
		TEXT("Flowering"),
		82);
	AddHarvestDefinition(
		TEXT("Harvest_Monstera"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"MonsteraHarvest",
			"Monstera"),
		95,
		TEXT("Green"),
		TEXT("Foliage"),
		72);
	AddHarvestDefinition(
		TEXT("Harvest_Lavender"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"LavenderHarvest",
			"Lavande violette"),
		75,
		TEXT("Purple"),
		TEXT("Aromatic"),
		74);

	const auto AddQualityVariants =
		[this](FName BaseItemKey)
		{
			const FBotanicusItemDefinition* FoundBase =
				NativeFallbackItems.FindByPredicate(
					[BaseItemKey](
						const FBotanicusItemDefinition& Candidate)
					{
						return Candidate.ItemKey == BaseItemKey;
					});
			if (!FoundBase)
			{
				return;
			}
			const FBotanicusItemDefinition Base = *FoundBase;
			const auto AddVariant =
				[this, &Base](
					const TCHAR* Suffix,
					const TCHAR* QualityLabel,
					FName QualityTag,
					float PriceMultiplier,
					int32 AppealBonus)
				{
					FBotanicusItemDefinition Variant = Base;
					Variant.ItemKey = FName(
						*(Base.ItemKey.ToString() + Suffix));
					Variant.DisplayName = FText::FromString(
						FString::Printf(
							TEXT("%s - %s"),
							*Base.DisplayName.ToString(),
							QualityLabel));
					Variant.PlantQualityTag = QualityTag;
					Variant.SalePrice = FMath::RoundToInt(
						Base.SalePrice * PriceMultiplier);
					Variant.VisitorAppeal = FMath::Min(
						100,
						Base.VisitorAppeal + AppealBonus);
					NativeFallbackItems.Add(MoveTemp(Variant));
				};
			AddVariant(
				TEXT("_Beautiful"),
				TEXT("Belle qualite"),
				TEXT("Beautiful"),
				1.35f,
				10);
			AddVariant(
				TEXT("_Exceptional"),
				TEXT("Qualite exceptionnelle"),
				TEXT("Exceptional"),
				1.75f,
				22);
		};
	AddQualityVariants(TEXT("Harvest_Basil"));
	AddQualityVariants(TEXT("Harvest_Orchid"));
	AddQualityVariants(TEXT("Harvest_Monstera"));
	AddQualityVariants(TEXT("Harvest_Lavender"));

	FBotanicusItemDefinition& SalesDisplay =
		NativeFallbackItems.AddDefaulted_GetRef();
	SalesDisplay.ItemKey = TEXT("SalesDisplay");
	SalesDisplay.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "SalesDisplay", "Presentoir de vente");
	SalesDisplay.Category = EBotanicusItemCategory::Decoration;
	SalesDisplay.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	SalesDisplay.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	SalesDisplay.WorldScale = FVector(1.15f, 0.45f, 0.65f);
	SalesDisplay.WorldActorClass =
		ABotanicusSalesDisplayActor::StaticClass();
	SalesDisplay.MaximumStack = 2;
	SalesDisplay.WeightClass = EBotanicusItemWeightClass::Hotbar;
	SalesDisplay.Price = 200;
	SalesDisplay.DeliveryQuantity = 1;
	SalesDisplay.DeliveryDelaySeconds = 3.0f;

	FBotanicusItemDefinition& SalePot =
		NativeFallbackItems.AddDefaulted_GetRef();
	SalePot.ItemKey = TEXT("SalePot");
	SalePot.DisplayName =
		NSLOCTEXT("BotanicusCatalog", "SalePot", "Pot de vente");
	SalePot.Category = EBotanicusItemCategory::Decoration;
	SalePot.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	SalePot.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	SalePot.WorldScale = FVector(0.28f, 0.28f, 0.22f);
	SalePot.WorldActorClass =
		ABotanicusSalePotActor::StaticClass();
	SalePot.MaximumStack = 10;
	SalePot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	SalePot.Price = 10;
	SalePot.DeliveryQuantity = 1;
	SalePot.DeliveryDelaySeconds = 2.0f;
	SalePot.CollisionHalfExtentOverride =
		FVector(28.0f, 28.0f, 22.0f);

	FBotanicusItemDefinition& PreparationWorkbench =
		NativeFallbackItems.AddDefaulted_GetRef();
	PreparationWorkbench.ItemKey = TEXT("PreparationWorkbench");
	PreparationWorkbench.DisplayName =
		NSLOCTEXT(
			"BotanicusCatalog",
			"PreparationWorkbench",
			"Etabli de preparation");
	PreparationWorkbench.Category =
		EBotanicusItemCategory::Equipment;
	PreparationWorkbench.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Preparation);
	PreparationWorkbench.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	PreparationWorkbench.WorldScale =
		FVector(1.2f, 0.6f, 0.12f);
	PreparationWorkbench.WorldActorClass =
		ABotanicusPreparationWorkbenchActor::StaticClass();
	PreparationWorkbench.MaximumStack = 1;
	PreparationWorkbench.WeightClass =
		EBotanicusItemWeightClass::OnePlayerCarry;
	PreparationWorkbench.CarryMovementSpeedMultiplier = 0.7f;
	PreparationWorkbench.Price = 350;
	PreparationWorkbench.DeliveryQuantity = 1;
	PreparationWorkbench.DeliveryDelaySeconds = 4.0f;
	PreparationWorkbench.bPurchasable = false;

	FBotanicusItemDefinition& Computer =
		NativeFallbackItems.AddDefaulted_GetRef();
	Computer.ItemKey = TEXT("CommandComputer");
	Computer.DisplayName =
		NSLOCTEXT(
			"BotanicusCatalog",
			"CommandComputer",
			"Ordinateur de commande");
	Computer.Category = EBotanicusItemCategory::Equipment;
	Computer.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Preparation);
	Computer.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Computer.WorldScale = FVector(0.5f, 0.12f, 0.34f);
	Computer.WorldActorClass =
		ABotanicusComputerActor::StaticClass();
	Computer.MaximumStack = 1;
	Computer.WeightClass = EBotanicusItemWeightClass::Handheld;
	Computer.Price = 250;
	Computer.DeliveryQuantity = 1;
	Computer.DeliveryDelaySeconds = 3.0f;
	Computer.CollisionHalfExtentOverride =
		FVector(55.0f, 32.0f, 42.0f);
	Computer.bPurchasable = false;

	FBotanicusItemDefinition& CashRegister =
		NativeFallbackItems.AddDefaulted_GetRef();
	CashRegister.ItemKey = TEXT("CashRegister");
	CashRegister.DisplayName =
		NSLOCTEXT(
			"BotanicusCatalog",
			"CashRegister",
			"Caisse");
	CashRegister.Category = EBotanicusItemCategory::Equipment;
	CashRegister.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	CashRegister.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	CashRegister.WorldScale = FVector(0.75f, 0.42f, 0.55f);
	CashRegister.WorldActorClass =
		ABotanicusCashRegisterActor::StaticClass();
	CashRegister.MaximumStack = 1;
	CashRegister.WeightClass =
		EBotanicusItemWeightClass::Handheld;
	CashRegister.Price = 0;
	CashRegister.bPurchasable = false;
	CashRegister.DeliveryQuantity = 1;
	CashRegister.DeliveryDelaySeconds = 3.0f;
	CashRegister.CollisionHalfExtentOverride =
		FVector(75.0f, 45.0f, 60.0f);

	FBotanicusItemDefinition& SelfCheckout =
		NativeFallbackItems.AddDefaulted_GetRef();
	SelfCheckout.ItemKey = TEXT("SelfCheckout");
	SelfCheckout.DisplayName =
		NSLOCTEXT(
			"BotanicusCatalog",
			"SelfCheckout",
			"Caisse automatique");
	SelfCheckout.Category = EBotanicusItemCategory::Equipment;
	SelfCheckout.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	SelfCheckout.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	// Engine cube = 100 cm: 60 cm deep, 40 cm wide, 200 cm high.
	SelfCheckout.WorldScale = FVector(0.6f, 0.4f, 2.0f);
	SelfCheckout.WorldActorClass =
		ABotanicusSelfCheckoutActor::StaticClass();
	SelfCheckout.MaximumStack = 1;
	SelfCheckout.WeightClass =
		EBotanicusItemWeightClass::Handheld;
	SelfCheckout.Price = 1500;
	SelfCheckout.DeliveryQuantity = 1;
	SelfCheckout.DeliveryDelaySeconds = 5.0f;
	SelfCheckout.CollisionHalfExtentOverride =
		FVector(30.0f, 20.0f, 100.0f);
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
