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
	// The native list is the authoritative whitelist. This prevents seeds
	// removed from the game from being resurrected by an older Data Asset.
	return NativeFallbackPlants.FindByPredicate(
		[PlantKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.PlantKey == PlantKey;
		});
}

const FBotanicusPlantDefinition*
UBotanicusPlantSubsystem::FindPlantBySeed(FName SeedItemKey) const
{
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
	return NativeFallbackPlants;
}
