// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusItemCatalogSubsystem.h"

#include "Decoration/BotanicusBrokenFlowerPotActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"
#include "Preparation/BotanicusWorkSurfaceActor.h"
#include "Preparation/BotanicusComputerActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "Storage/BotanicusStorageShelfActor.h"

namespace
{
bool IsDeprecatedStorageShelfType(FName ItemKey)
{
	return ItemKey == TEXT("StorageShelfFloorLarge") ||
		ItemKey == TEXT("StorageShelfWallSmall");
}

struct FNativePlantItemEntry
{
	const TCHAR* Key;
	const TCHAR* DisplayName;
	const TCHAR* SeedDisplayName;
	int32 SeedPrice;
	int32 SalePrice;
	const TCHAR* ColorTag;
	const TCHAR* TypeTag;
	int32 Appeal;
};

const FNativePlantItemEntry NativePlantItems[] =
{
	{TEXT("AureliaSweet"), TEXT("Aurélia Douce"), TEXT("Graines d'Aurélia Douce"), 45, 105, TEXT("Green"), TEXT("Normal"), 80},
	{TEXT("CoraliaPerchee"), TEXT("Coralia Perchée"), TEXT("Graines de Coralia Perchée"), 45, 110, TEXT("Green"), TEXT("Normal"), 80},
	{TEXT("CoraliaPompon"), TEXT("Coralia Pompon"), TEXT("Graines de Coralia Pompon"), 45, 112, TEXT("Green"), TEXT("Normal"), 81},
	{TEXT("Iralia"), TEXT("Iralia"), TEXT("Graines d'Iralia"), 45, 108, TEXT("Green"), TEXT("Normal"), 79},
	{TEXT("LumineaFlorae"), TEXT("Luminéa Floraë"), TEXT("Graines de Luminéa Floraë"), 50, 120, TEXT("Green"), TEXT("Normal"), 84},
	{TEXT("VerdelaCommune"), TEXT("Verdéla Commune"), TEXT("Graines de Verdéla Commune"), 40, 100, TEXT("Green"), TEXT("Normal"), 76},
	{TEXT("CoralyneBrumes"), TEXT("Coralyne des brumes"), TEXT("Graines de Coralyne des brumes"), 80, 180, TEXT("Blue"), TEXT("ElementalWater"), 88},
	{TEXT("HydreaLagunaire"), TEXT("Hydréa lagunaire"), TEXT("Graines d'Hydréa lagunaire"), 80, 185, TEXT("Blue"), TEXT("ElementalWater"), 89},
	{TEXT("NerelisEventail"), TEXT("Nérélis éventail"), TEXT("Graines de Nérélis éventail"), 85, 195, TEXT("Blue"), TEXT("ElementalWater"), 91},
	{TEXT("OndeliaRuban"), TEXT("Ondélia ruban"), TEXT("Graines d'Ondélia ruban"), 80, 182, TEXT("Blue"), TEXT("ElementalWater"), 88},
	{TEXT("BonzaiaGivre"), TEXT("Bonzaïa Givré"), TEXT("Graines de Bonzaïa Givré"), 90, 210, TEXT("White"), TEXT("ElementalIce"), 90},
	{TEXT("CristalliaLumifleur"), TEXT("Cristallia lumifleur"), TEXT("Graines de Cristallia lumifleur"), 95, 225, TEXT("White"), TEXT("ElementalIce"), 93},
	{TEXT("GivrelanceAzure"), TEXT("Givrelance azure"), TEXT("Graines de Givrelance azure"), 90, 215, TEXT("White"), TEXT("ElementalIce"), 91},
	{TEXT("GlaceoraPerlee"), TEXT("Glacéora perlée"), TEXT("Graines de Glacéora perlée"), 95, 220, TEXT("White"), TEXT("ElementalIce"), 92},
	{TEXT("BraiseliaFlamme"), TEXT("Braiselia flamme"), TEXT("Graines de Braiselia flamme"), 80, 180, TEXT("Red"), TEXT("ElementalFire"), 88},
	{TEXT("MagmoraSpiral"), TEXT("Magmora spiral"), TEXT("Graines de Magmora spiral"), 85, 190, TEXT("Red"), TEXT("ElementalFire"), 90},
	{TEXT("PyrosteleRoyal"), TEXT("Pyrostèle royal"), TEXT("Graines de Pyrostèle royal"), 90, 200, TEXT("Red"), TEXT("ElementalFire"), 93},
	{TEXT("VolcaniaBasiera"), TEXT("Volcania basiera"), TEXT("Graines de Volcania basiera"), 85, 188, TEXT("Red"), TEXT("ElementalFire"), 89},
	{TEXT("Noctepine"), TEXT("Noctépine"), TEXT("Graines de Noctépine"), 100, 240, TEXT("Black"), TEXT("ElementalShadow"), 92},
	{TEXT("NoctivoraVentouse"), TEXT("Noctivora ventouse"), TEXT("Graines de Noctivora ventouse"), 105, 255, TEXT("Black"), TEXT("ElementalShadow"), 95},
	{TEXT("OmbraeLuridia"), TEXT("Ombrae luridia"), TEXT("Graines d'Ombrae luridia"), 105, 250, TEXT("Black"), TEXT("ElementalShadow"), 94},
};
}

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
	if (LoadedCatalog)
	{
		auto UseAuthoredShelfBlueprint =
			[this](FName ItemKey, const TCHAR* BlueprintClassPath)
			{
				if (FBotanicusItemDefinition* Shelf =
						LoadedCatalog->Items.FindByPredicate(
							[ItemKey](
								const FBotanicusItemDefinition& Definition)
							{
								return Definition.ItemKey == ItemKey;
							}))
				{
					Shelf->WorldActorClass = TSoftClassPtr<AActor>(
						FSoftObjectPath(BlueprintClassPath));
				}
			};
		UseAuthoredShelfBlueprint(
			TEXT("StorageShelfWallLarge"),
			TEXT("/Game/Botanicus/blueprints/BP_Item_StorageShelfWallLarge.BP_Item_StorageShelfWallLarge_C"));
		for (FBotanicusItemDefinition& Definition : LoadedCatalog->Items)
		{
			if (IsDeprecatedStorageShelfType(Definition.ItemKey))
			{
				Definition.bPurchasable = false;
			}
			else if (Definition.ItemKey == TEXT("StorageShelfFloorSmall"))
			{
				Definition.DisplayName = NSLOCTEXT(
					"BotanicusCatalog",
					"StorageShelfFloor",
					"Etagere au sol");
			}
			else if (Definition.ItemKey == TEXT("StorageShelfWallLarge"))
			{
				Definition.DisplayName = NSLOCTEXT(
					"BotanicusCatalog",
					"StorageShelfWall",
					"Etagere murale");
			}
		}

		if (FBotanicusItemDefinition* FloorSmallShelf =
				LoadedCatalog->Items.FindByPredicate(
					[](const FBotanicusItemDefinition& Definition)
					{
						return Definition.ItemKey ==
							TEXT("StorageShelfFloorSmall");
					}))
		{
			// Runtime migration for catalog assets made before the authored shelf
			// was imported. Its Blueprint StorageSlot components are authoritative;
			// spawning the native C++ class would ignore their number/transforms.
			FloorSmallShelf->WorldActorClass = TSoftClassPtr<AActor>(
				FSoftObjectPath(TEXT(
					"/Game/Botanicus/blueprints/BP_Item_StorageShelfFloorSmall.BP_Item_StorageShelfFloorSmall_C")));
			FloorSmallShelf->WorldMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(TEXT(
					"/Game/Botanicus/Items/furnituresMesh/etagère4Slot/etagère4Slot.etagère4Slot")));
			FloorSmallShelf->WorldScale = FVector::OneVector;
			FloorSmallShelf->CollisionHalfExtentOverride =
				FVector(45.0f, 103.0f, 100.0f);
		}
		if (FBotanicusItemDefinition* WateringCan =
				LoadedCatalog->Items.FindByPredicate(
					[](const FBotanicusItemDefinition& Definition)
					{
						return Definition.ItemKey == TEXT("WateringCan");
					}))
		{
			WateringCan->WeightClass =
				EBotanicusItemWeightClass::Hotbar;
			WateringCan->AllowedPlacementSurfaces =
				static_cast<int32>(EBotanicusPlacementSurface::Floor);
			WateringCan->Icon = TSoftObjectPtr<UTexture2D>(
				FSoftObjectPath(TEXT(
					"/Game/Botanicus/UI/Tools/T_WateringCan_Hotbar.T_WateringCan_Hotbar")));
		}
		if (FBotanicusItemDefinition* PottingSoil =
				LoadedCatalog->Items.FindByPredicate(
					[](const FBotanicusItemDefinition& Definition)
					{
						return Definition.ItemKey ==
							TEXT("PottingSoil");
					}))
		{
			// Runtime migration for catalogues generated before soil was
			// allowed on the floor or limited to bags of five. The asset
			// generator writes these values permanently the next time the
			// catalogue is regenerated.
			PottingSoil->AllowedPlacementSurfaces =
				static_cast<int32>(
					EBotanicusPlacementSurface::Floor);
			PottingSoil->MaximumStack = 5;
		}
	}

	FBotanicusItemDefinition& BrokenFlowerPot =
		NativeFallbackItems.AddDefaulted_GetRef();
	BrokenFlowerPot.ItemKey = TEXT("BrokenFlowerPot");
	BrokenFlowerPot.DisplayName = NSLOCTEXT(
		"BotanicusCatalog",
		"BrokenFlowerPot",
		"Pot de fleurs casse");
	BrokenFlowerPot.Category = EBotanicusItemCategory::Decoration;
	BrokenFlowerPot.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::None);
	BrokenFlowerPot.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/Items/itemsMesh/propsDeco/pot1/pot.pot")));
	BrokenFlowerPot.WorldScale = FVector::OneVector;
	BrokenFlowerPot.WorldActorClass = TSoftClassPtr<AActor>(
		ABotanicusBrokenFlowerPotActor::StaticClass());
	BrokenFlowerPot.MaximumStack = 1;
	BrokenFlowerPot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	BrokenFlowerPot.Price = 0;
	BrokenFlowerPot.bPurchasable = false;
	BrokenFlowerPot.AllowedPlacementSurfaces =
		static_cast<int32>(EBotanicusPlacementSurface::Floor);
	BrokenFlowerPot.CollisionHalfExtentOverride =
		FVector(28.0f, 28.0f, 24.0f);

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
	PlantPot.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_Item_PlantPot.BP_Item_PlantPot_C")));
	PlantPot.MaximumStack = 10;
	PlantPot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	PlantPot.Price = 75;
	PlantPot.DeliveryQuantity = 1;
	PlantPot.DeliveryDelaySeconds = 2.0f;

	FBotanicusItemDefinition& SquarePlantPot =
		NativeFallbackItems.AddDefaulted_GetRef();
	SquarePlantPot.ItemKey = TEXT("PlantPotSquare");
	SquarePlantPot.DisplayName = NSLOCTEXT(
		"BotanicusCatalog",
		"PlantPotSquare",
		"Pot de culture carre");
	SquarePlantPot.Category = EBotanicusItemCategory::Decoration;
	SquarePlantPot.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Preparation);
	SquarePlantPot.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/Items/itemsMesh/potPreparation/carré/pot_preparation-carré.pot_preparation-carré")));
	SquarePlantPot.WorldScale = FVector::OneVector;
	SquarePlantPot.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_Item_PlantPotSquare.BP_Item_PlantPotSquare_C")));
	SquarePlantPot.MaximumStack = 10;
	SquarePlantPot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	SquarePlantPot.Price = 85;
	SquarePlantPot.DeliveryQuantity = 1;
	SquarePlantPot.DeliveryDelaySeconds = 2.0f;
	SquarePlantPot.CollisionHalfExtentOverride =
		FVector(50.0f, 47.0f, 21.0f);

	const auto AddPreparationPlanter =
		[this](
			FName ItemKey,
			const FText& DisplayName,
			int32 Capacity,
			int32 Price)
		{
			FBotanicusItemDefinition& Planter =
				NativeFallbackItems.AddDefaulted_GetRef();
			Planter.ItemKey = ItemKey;
			Planter.DisplayName = DisplayName;
			Planter.Category =
				EBotanicusItemCategory::Decoration;
			Planter.CatalogTabs =
				static_cast<int32>(
					EBotanicusCatalogTab::Preparation);
			Planter.WorldMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(
					TEXT("/Engine/BasicShapes/Cube.Cube")));
			Planter.WorldScale = FVector(
				0.45f + static_cast<float>(Capacity) * 0.3f,
				0.42f,
				0.3f);
			Planter.CollisionHalfExtentOverride =
				Planter.WorldScale * 50.0f;
			Planter.WorldActorClass =
				ABotanicusMultiPlantPotActor::StaticClass();
			Planter.MaximumStack = 1;
			Planter.WeightClass =
				EBotanicusItemWeightClass::Hotbar;
			Planter.Price = Price;
			Planter.DeliveryQuantity = 1;
			Planter.DeliveryDelaySeconds = 3.0f;
		};
	AddPreparationPlanter(
		TEXT("PreparationPlanter2"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"PreparationPlanter2",
			"Jardiniere de preparation - 2 fleurs"),
		2,
		130);
	AddPreparationPlanter(
		TEXT("PreparationPlanter3"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"PreparationPlanter3",
			"Jardiniere de preparation - 3 fleurs"),
		3,
		180);
	AddPreparationPlanter(
		TEXT("PreparationPlanter4"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"PreparationPlanter4",
			"Jardiniere de preparation - 4 fleurs"),
		4,
		230);

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
	PottingSoil.MaximumStack = 5;
	PottingSoil.bSaleSoil = true;
	PottingSoil.WeightClass = EBotanicusItemWeightClass::Hotbar;
	PottingSoil.Price = 25;
	PottingSoil.DeliveryQuantity = 5;
	PottingSoil.DeliveryDelaySeconds = 2.0f;
	PottingSoil.AllowedPlacementSurfaces =
		static_cast<int32>(
			EBotanicusPlacementSurface::Floor);

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
	for (const FNativePlantItemEntry& Entry : NativePlantItems)
	{
		AddSeedDefinition(
			FName(*(FString(TEXT("SeedPacket_")) + Entry.Key)),
			FText::FromString(Entry.SeedDisplayName),
			Entry.SeedPrice,
			5);
	}

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
	WateringCan.Icon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/UI/Tools/T_WateringCan_Hotbar.T_WateringCan_Hotbar")));
	WateringCan.MaximumStack = 1;
	WateringCan.WeightClass = EBotanicusItemWeightClass::Hotbar;
	WateringCan.Price = 120;
	WateringCan.DeliveryQuantity = 1;
	WateringCan.DeliveryDelaySeconds = 2.0f;
	WateringCan.AllowedPlacementSurfaces =
		static_cast<int32>(EBotanicusPlacementSurface::Floor);

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
				FSoftObjectPath(TEXT(
					"/Game/Botanicus/Items/itemsMesh/plantes/normal/"
					"AuréliaDouce/niv4/niv4.niv4")));
			Harvest.WorldScale = FVector::OneVector;
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
	for (const FNativePlantItemEntry& Entry : NativePlantItems)
	{
		AddHarvestDefinition(
			FName(*(FString(TEXT("Harvest_")) + Entry.Key)),
			FText::FromString(Entry.DisplayName),
			Entry.SalePrice,
			FName(Entry.ColorTag),
			FName(Entry.TypeTag),
			Entry.Appeal);
	}

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
	for (const FNativePlantItemEntry& Entry : NativePlantItems)
	{
		AddQualityVariants(
			FName(*(FString(TEXT("Harvest_")) + Entry.Key)));
	}

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
	// Always spawn the editable Blueprint version.  Using the native class here
	// silently discarded the slot position/height configured in
	// BP_Item_SalesDisplay and made the displayed pot appear offset from the
	// furniture in game.
	SalesDisplay.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_Item_SalesDisplay.BP_Item_SalesDisplay_C")));
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
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/Items/itemsMesh/potDeVente/rond/"
			"pot_de_vente.pot_de_vente")));
	SalePot.WorldScale = FVector::OneVector;
	SalePot.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_Item_SalePot.BP_Item_SalePot_C")));
	SalePot.MaximumStack = 10;
	SalePot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	SalePot.Price = 10;
	SalePot.DeliveryQuantity = 1;
	SalePot.DeliveryDelaySeconds = 2.0f;
	SalePot.CollisionHalfExtentOverride =
		FVector(28.0f, 28.0f, 22.0f);

	FBotanicusItemDefinition& SquareSalePot =
		NativeFallbackItems.AddDefaulted_GetRef();
	SquareSalePot.ItemKey = TEXT("SalePotSquare");
	SquareSalePot.DisplayName = NSLOCTEXT(
		"BotanicusCatalog",
		"SalePotSquare",
		"Pot de vente carre");
	SquareSalePot.Category = EBotanicusItemCategory::Decoration;
	SquareSalePot.CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Sales);
	SquareSalePot.WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/Items/itemsMesh/potDeVente/carré/pot_vente-carré.pot_vente-carré")));
	SquareSalePot.WorldScale = FVector::OneVector;
	SquareSalePot.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_Item_SalePotSquare.BP_Item_SalePotSquare_C")));
	SquareSalePot.MaximumStack = 10;
	SquareSalePot.WeightClass = EBotanicusItemWeightClass::Hotbar;
	SquareSalePot.Price = 12;
	SquareSalePot.DeliveryQuantity = 1;
	SquareSalePot.DeliveryDelaySeconds = 2.0f;
	SquareSalePot.CollisionHalfExtentOverride =
		FVector(28.0f, 28.0f, 24.0f);

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
	PreparationWorkbench.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_WorkBench.BP_WorkBench_C")));
	PreparationWorkbench.MaximumStack = 1;
	PreparationWorkbench.WeightClass =
		EBotanicusItemWeightClass::OnePlayerCarry;
	PreparationWorkbench.CarryMovementSpeedMultiplier = 0.7f;
	PreparationWorkbench.Price = 350;
	PreparationWorkbench.DeliveryQuantity = 1;
	PreparationWorkbench.DeliveryDelaySeconds = 4.0f;
	// The preparation workbench is supplied automatically at the start of a
	// game and must not be listed in the purchasing catalogue.
	PreparationWorkbench.bPurchasable = false;

	const auto AddWorkSurface =
		[this](
			FName ItemKey,
			const FText& DisplayName,
			const FVector& WorldScale,
			const FVector& CollisionExtent,
			int32 Price)
		{
			FBotanicusItemDefinition& Surface =
				NativeFallbackItems.AddDefaulted_GetRef();
			Surface.ItemKey = ItemKey;
			Surface.DisplayName = DisplayName;
			Surface.Category = EBotanicusItemCategory::Equipment;
			Surface.CatalogTabs =
				static_cast<int32>(
					EBotanicusCatalogTab::Preparation);
			Surface.WorldMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(
					TEXT("/Engine/BasicShapes/Cube.Cube")));
			Surface.WorldScale = WorldScale;
			Surface.WorldActorClass =
				ABotanicusWorkSurfaceActor::StaticClass();
			Surface.MaximumStack = 1;
			Surface.WeightClass =
				EBotanicusItemWeightClass::OnePlayerCarry;
			Surface.CarryMovementSpeedMultiplier = 0.70f;
			Surface.Price = Price;
			Surface.DeliveryQuantity = 1;
			Surface.DeliveryDelaySeconds = 3.0f;
			Surface.AllowedPlacementSurfaces =
				static_cast<int32>(
					EBotanicusPlacementSurface::Floor);
			Surface.CollisionHalfExtentOverride =
				CollisionExtent;
		};
	AddWorkSurface(
		TEXT("WorkSurfaceSmall"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"WorkSurfaceSmall",
			"Petit plan de travail"),
		FVector(1.2f, 0.6f, 0.9f),
		FVector(60.0f, 30.0f, 45.0f),
		180);
	AddWorkSurface(
		TEXT("WorkSurfaceMedium"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"WorkSurfaceMedium",
			"Plan de travail moyen"),
		FVector(2.0f, 0.7f, 0.9f),
		FVector(100.0f, 35.0f, 45.0f),
		280);
	AddWorkSurface(
		TEXT("WorkSurfaceLarge"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"WorkSurfaceLarge",
			"Grand plan de travail"),
		FVector(3.0f, 0.8f, 0.9f),
		FVector(150.0f, 40.0f, 45.0f),
		420);

	auto AddStorageShelf =
		[this](
			FName ItemKey,
			const FText& DisplayName,
			const FVector& WorldScale,
			const FVector& CollisionExtent,
			int32 Price,
			bool bWallMounted)
		{
			FBotanicusItemDefinition& Shelf =
				NativeFallbackItems.AddDefaulted_GetRef();
			Shelf.ItemKey = ItemKey;
			Shelf.DisplayName = DisplayName;
			Shelf.Category = EBotanicusItemCategory::Equipment;
			Shelf.CatalogTabs =
				static_cast<int32>(EBotanicusCatalogTab::Preparation);
			Shelf.WorldMesh = TSoftObjectPtr<UStaticMesh>(
				FSoftObjectPath(
					ItemKey == TEXT("StorageShelfFloorSmall")
						? TEXT("/Game/Botanicus/Items/furnituresMesh/etagère4Slot/etagère4Slot.etagère4Slot")
						: TEXT("/Engine/BasicShapes/Cube.Cube")));
			Shelf.WorldScale =
				ItemKey == TEXT("StorageShelfFloorSmall")
					? FVector::OneVector
					: WorldScale;
			const TCHAR* ShelfBlueprintClassPath = nullptr;
			if (ItemKey == TEXT("StorageShelfFloorSmall"))
			{
				ShelfBlueprintClassPath =
					TEXT("/Game/Botanicus/blueprints/BP_Item_StorageShelfFloorSmall.BP_Item_StorageShelfFloorSmall_C");
			}
			else if (ItemKey == TEXT("StorageShelfWallLarge"))
			{
				ShelfBlueprintClassPath =
					TEXT("/Game/Botanicus/blueprints/BP_Item_StorageShelfWallLarge.BP_Item_StorageShelfWallLarge_C");
			}
			Shelf.WorldActorClass = ShelfBlueprintClassPath
				? TSoftClassPtr<AActor>(
					FSoftObjectPath(ShelfBlueprintClassPath))
				: TSoftClassPtr<AActor>(
					ABotanicusStorageShelfActor::StaticClass());
			Shelf.MaximumStack = 1;
			Shelf.WeightClass =
				EBotanicusItemWeightClass::OnePlayerCarry;
			Shelf.CarryMovementSpeedMultiplier = 0.75f;
			Shelf.Price = Price;
			Shelf.DeliveryQuantity = 1;
			Shelf.DeliveryDelaySeconds = 3.0f;
			Shelf.AllowedPlacementSurfaces =
				static_cast<int32>(
					bWallMounted
						? EBotanicusPlacementSurface::Wall
						: EBotanicusPlacementSurface::Floor);
			Shelf.CollisionHalfExtentOverride =
				ItemKey == TEXT("StorageShelfFloorSmall")
					? FVector(45.0f, 103.0f, 100.0f)
					: CollisionExtent;
		};
	AddStorageShelf(
		TEXT("StorageShelfFloorSmall"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"StorageShelfFloor",
			"Etagere au sol"),
		FVector(0.6f, 1.7f, 1.5f),
		FVector(30.0f, 85.0f, 75.0f),
		180,
		false);
	AddStorageShelf(
		TEXT("StorageShelfWallLarge"),
		NSLOCTEXT(
			"BotanicusCatalog",
			"StorageShelfWall",
			"Etagere murale"),
		FVector(0.36f, 1.9f, 0.96f),
		FVector(18.0f, 95.0f, 48.0f),
		220,
		true);

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
	Computer.WorldActorClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT(
			"/Game/Botanicus/blueprints/BP_Item_CommandComputer.BP_Item_CommandComputer_C")));
	Computer.MaximumStack = 1;
	// The unique starter computer is delivered in player one's quickbar and
	// placed with the regular A-key placement flow.
	Computer.WeightClass = EBotanicusItemWeightClass::Hotbar;
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
	if (const FBotanicusItemDefinition* NativeDefinition =
		NativeFallbackItems.FindByPredicate(
			[ItemKey](const FBotanicusItemDefinition& Definition)
			{
				return Definition.ItemKey == ItemKey;
			}))
	{
		return NativeDefinition;
	}

	// Seeds are deliberately authoritative in code. Do not allow a seed
	// removed from the catalogue to survive through a stale Data Asset.
	if (ItemKey.ToString().StartsWith(TEXT("SeedPacket_")))
	{
		return nullptr;
	}

	if (LoadedCatalog)
	{
		if (const FBotanicusItemDefinition* Definition =
			LoadedCatalog->FindItem(ItemKey))
		{
			return Definition;
		}
	}

	return nullptr;
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
		for (const FBotanicusItemDefinition& Definition :
			LoadedCatalog->Items)
		{
			if (!IsDeprecatedStorageShelfType(Definition.ItemKey) &&
				!Definition.ItemKey.ToString().StartsWith(
					TEXT("SeedPacket_")))
			{
				Result.Add(Definition);
			}
		}
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
