// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sales/BotanicusSalePotActor.h"

#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "Visuals/BotanicusPotSoilVisualActor.h"

namespace
{
FLinearColor SalePotPlantVisualColor(FName ColorTag)
{
	if (ColorTag == TEXT("Pink"))
	{
		return FLinearColor(0.95f, 0.20f, 0.55f);
	}
	if (ColorTag == TEXT("Purple"))
	{
		return FLinearColor(0.48f, 0.20f, 0.78f);
	}
	return FLinearColor(0.08f, 0.48f, 0.12f);
}

FString SalePotPlantQualityLabel(FName QualityTag)
{
	if (QualityTag == TEXT("Exceptional"))
	{
		return TEXT("EXCEPTIONNELLE");
	}
	if (QualityTag == TEXT("Beautiful"))
	{
		return TEXT("BELLE");
	}
	return TEXT("STANDARD");
}

void GetMatureSalePlantShape(
	FName PlantItemKey,
	float& OutStemHeight,
	FVector& OutFoliageShape)
{
	FString BaseKey = PlantItemKey.ToString();
	BaseKey.RemoveFromEnd(TEXT("_Beautiful"));
	BaseKey.RemoveFromEnd(TEXT("_Exceptional"));
	float HeightMultiplier = 1.0f;
	OutFoliageShape = FVector(1.0f, 1.0f, 0.7f);
	if (BaseKey == TEXT("Harvest_Orchid"))
	{
		HeightMultiplier = 1.15f;
		OutFoliageShape = FVector(0.7f, 0.7f, 1.3f);
	}
	else if (BaseKey == TEXT("Harvest_Monstera"))
	{
		HeightMultiplier = 0.9f;
		OutFoliageShape = FVector(1.55f, 1.3f, 0.65f);
	}
	else if (BaseKey == TEXT("Harvest_Lavender"))
	{
		HeightMultiplier = 1.3f;
		OutFoliageShape = FVector(0.62f, 0.62f, 1.5f);
	}
	else if (BaseKey == TEXT("Harvest_Violet"))
	{
		HeightMultiplier = 0.62f;
		OutFoliageShape = FVector(1.35f, 1.35f, 0.58f);
	}
	OutStemHeight = 80.0f * HeightMultiplier;
}

float ReadSalePotBlueprintFloatSetting(
	const UObject* Object,
	const FName PropertyName,
	const float Fallback)
{
	if (const FFloatProperty* Property =
		FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName))
	{
		return Property->GetPropertyValue_InContainer(Object);
	}
	return Fallback;
}

FVector2D ReadSalePotBlueprintVector2DSetting(
	const UObject* Object,
	const FName PropertyName,
	const FVector2D& Fallback)
{
	if (const FStructProperty* Property =
		FindFProperty<FStructProperty>(Object->GetClass(), PropertyName))
	{
		if (Property->Struct == TBaseStructure<FVector2D>::Get())
		{
			return *Property->ContainerPtrToValuePtr<FVector2D>(Object);
		}
	}
	return Fallback;
}

bool ReadSalePotBlueprintBoolSetting(
	const UObject* Object,
	const FName PropertyName,
	const bool bFallback)
{
	if (const FBoolProperty* Property =
		FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName))
	{
		return Property->GetPropertyValue_InContainer(Object);
	}
	return bFallback;
}
}

ABotanicusSalePotActor::ABotanicusSalePotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CylinderFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderFinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.22f));
	}

	SoilVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Sale Pot Soil"));
	SoilVisual->SetupAttachment(SceneRoot);
	SoilVisual->SetStaticMesh(nullptr);
	SoilVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoilVisual->SetVisibility(false);

	SoilShapeVisual = CreateDefaultSubobject<UChildActorComponent>(
		TEXT("Adaptive Pot Soil"));
	SoilShapeVisual->SetupAttachment(SceneRoot);
	SoilShapeVisual->SetChildActorClass(
		ABotanicusPotSoilVisualActor::StaticClass());

	PlantVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Sale Pot Plant"));
	PlantVisual->SetupAttachment(SceneRoot);
	PlantVisual->SetStaticMesh(
		SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
	PlantVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 58.0f));
	PlantVisual->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.38f));
	PlantVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StemVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Sale Pot Plant Stem"));
	StemVisual->SetupAttachment(SceneRoot);
	StemVisual->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	StemVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Sale Pot Status"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(15.0f);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshVisuals();
}

float ABotanicusSalePotActor::GetPreparationHeightAdjustment() const
{
	return ReadSalePotBlueprintFloatSetting(
		this,
		TEXT("WorkbenchHeightAdjustmentSetting"),
		PreparationHeightAdjustment);
}

FVector2D ABotanicusSalePotActor::GetConfiguredSoilHorizontalOffset() const
{
	return ReadSalePotBlueprintVector2DSetting(
		this, TEXT("SoilHorizontalPosition"), SoilHorizontalOffset);
}

float ABotanicusSalePotActor::GetConfiguredSoilMaximumHeight() const
{
	return ReadSalePotBlueprintFloatSetting(
		this, TEXT("SoilMaximumHeightSetting"), SoilSurfaceHeight);
}

FVector2D ABotanicusSalePotActor::GetConfiguredSoilBottomRadii() const
{
	return ReadSalePotBlueprintVector2DSetting(
		this, TEXT("SoilBottomRadiiSetting"), SoilBottomRadii);
}

FVector2D ABotanicusSalePotActor::GetConfiguredSoilTopRadii() const
{
	return ReadSalePotBlueprintVector2DSetting(
		this, TEXT("SoilTopRadiiSetting"), SoilTopRadii);
}

float ABotanicusSalePotActor::GetConfiguredSoilVolumeHeight() const
{
	return FMath::Max(
		1.0f,
		ReadSalePotBlueprintFloatSetting(
			this, TEXT("SoilVolumeHeightSetting"), SoilVolumeHeight));
}

bool ABotanicusSalePotActor::GetConfiguredSquareSoilProfile() const
{
	return ReadSalePotBlueprintBoolSetting(
		this, TEXT("UseSquareSoilProfile"), bSquareSoilProfile);
}

void ABotanicusSalePotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshSoilVisual();
}

void ABotanicusSalePotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	if (StatusText && Controller && Controller->PlayerCameraManager)
	{
		StatusText->SetWorldRotation(
			(Controller->PlayerCameraManager->GetCameraLocation() -
			 StatusText->GetComponentLocation()).Rotation());
	}
	if (HasAuthority() && ActiveUser.IsValid())
	{
		UpdatePrimaryUse(DeltaSeconds);
		RefreshVisuals();
	}
}

void ABotanicusSalePotActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusSalePotActor, SoilItemKey);
	DOREPLIFETIME(ABotanicusSalePotActor, PlantItemKey);
	DOREPLIFETIME(ABotanicusSalePotActor, SoilFillProgress);
}

void ABotanicusSalePotActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (StatusText)
	{
		StatusText->SetVisibility(false);
	}
}

void ABotanicusSalePotActor::BeginPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() || ActiveUser.IsValid() || IsReadyForSale())
	{
		return;
	}
	if (!IsOnPreparationWorkbench())
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"Placez d'abord le pot de vente sur un établi de préparation."));
		return;
	}
	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!QuickBar)
	{
		return;
	}
	const FName SelectedKey = QuickBar->GetSelectedSlot().ItemKey;
	if (SelectedKey == TEXT("GardenTrowel") &&
		GetSoilLevel() > KINDA_SMALL_NUMBER)
	{
		SoilFillProgress = GetSoilLevel();
		ActiveUser = Character;
		PrimaryUseMode = EPrimaryUseMode::RemoveSoil;
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Maintenez le clic gauche pour retirer le terreau (%d%% restant)."),
				FMath::RoundToInt(SoilFillProgress * 100.0f)));
		return;
	}
	if (!HasSoil())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusItemCatalogSubsystem* Catalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusItemCatalogSubsystem>()
				: nullptr;
		const FBotanicusItemDefinition* SoilDefinition =
			Catalog ? Catalog->FindItem(SelectedKey) : nullptr;
		if (!SoilDefinition || !SoilDefinition->bSaleSoil)
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Selectionnez un terreau compatible."));
			return;
		}
		ActiveUser = Character;
		PrimaryUseMode = EPrimaryUseMode::FillSoil;
		PendingSoilItemKey = SelectedKey;
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Maintenez le clic gauche pour remplir le pot de vente (%d%%)."),
				FMath::RoundToInt(SoilFillProgress * 100.0f)));
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* Plant =
		Plants ? Plants->FindPlantByHarvestItem(SelectedKey) : nullptr;
	if (!Plant)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Selectionnez une plante entiere recoltee."));
		return;
	}
	if (Plant->CompatibleSaleSoilItemKey != SoilItemKey)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Ce terreau n'est pas compatible avec cette plante."));
		return;
	}
	if (!QuickBar->ConsumeSelectedItem(1))
	{
		return;
	}
	PlantItemKey = SelectedKey;
	RefreshVisuals();
	ForceNetUpdate();
	SendInteractorMessage(
		Interactor,
		FString::Printf(
			TEXT(
				"%s rempote. Maintenez E pour placer le pot sur un presentoir."),
			*Plant->DisplayName.ToString()));
}

void ABotanicusSalePotActor::EndPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() ||
		(ActiveUser.IsValid() && ActiveUser.Get() != Interactor))
	{
		return;
	}
	if (PrimaryUseMode == EPrimaryUseMode::FillSoil)
	{
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Remplissage interrompu : %d%% conserve."),
				FMath::RoundToInt(SoilFillProgress * 100.0f)));
	}
	else if (PrimaryUseMode == EPrimaryUseMode::RemoveSoil)
	{
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Retrait interrompu : %d%% de terreau restant."),
				FMath::RoundToInt(GetSoilLevel() * 100.0f)));
	}
	ActiveUser.Reset();
	PrimaryUseMode = EPrimaryUseMode::None;
	PendingSoilItemKey = NAME_None;
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusSalePotActor::RestoreSalePotState(
	FName InSoilItemKey,
	FName InPlantItemKey)
{
	if (!HasAuthority())
	{
		return;
	}
	SoilItemKey = InSoilItemKey;
	PlantItemKey = InPlantItemKey;
	SoilFillProgress = 0.0f;
	PrimaryUseMode = EPrimaryUseMode::None;
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusSalePotActor::UpdatePrimaryUse(float DeltaSeconds)
{
	ABotanicusCharacter* Character = ActiveUser.Get();
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (PrimaryUseMode == EPrimaryUseMode::RemoveSoil)
	{
		if (!IsValid(Character) || !QuickBar ||
			!IsOnPreparationWorkbench() ||
			!IsInteractorStillTargeting(Character) ||
			QuickBar->GetSelectedSlot().ItemKey != TEXT("GardenTrowel") ||
			IsReadyForSale() || SoilItemKey.IsNone() ||
			GetSoilLevel() <= KINDA_SMALL_NUMBER)
		{
			EndPrimaryUse(Character);
			return;
		}

		SoilFillProgress = FMath::Clamp(
			GetSoilLevel() -
				DeltaSeconds / FMath::Max(0.1f, SoilRemovalDuration),
			0.0f,
			1.0f);
		if (SoilFillProgress <= KINDA_SMALL_NUMBER)
		{
			SoilFillProgress = 0.0f;
			SoilItemKey = NAME_None;
			ActiveUser.Reset();
			PrimaryUseMode = EPrimaryUseMode::None;
			SendInteractorMessage(
				Character,
				TEXT("Le terreau a ete retire du pot de vente."));
			ForceNetUpdate();
		}
		return;
	}

	if (PrimaryUseMode != EPrimaryUseMode::FillSoil)
	{
		EndPrimaryUse(Character);
		return;
	}
	if (!IsValid(Character) || !QuickBar ||
		!IsOnPreparationWorkbench() ||
		!IsInteractorStillTargeting(Character) ||
		QuickBar->GetSelectedSlot().ItemKey != PendingSoilItemKey ||
		HasSoil())
	{
		EndPrimaryUse(Character);
		return;
	}
	SoilFillProgress = FMath::Clamp(
		SoilFillProgress +
			DeltaSeconds / FMath::Max(0.1f, SoilFillDuration),
		0.0f,
		1.0f);
	if (SoilFillProgress >= 1.0f)
	{
		if (QuickBar->ConsumeSelectedItem(1))
		{
			SoilItemKey = PendingSoilItemKey;
			SendInteractorMessage(
				Character,
				TEXT("Terreau ajoute au pot de vente."));
		}
		ActiveUser.Reset();
		SoilFillProgress = 0.0f;
		PrimaryUseMode = EPrimaryUseMode::None;
		PendingSoilItemKey = NAME_None;
		ForceNetUpdate();
	}
}

float ABotanicusSalePotActor::GetSoilLevel() const
{
	if (SoilItemKey.IsNone())
	{
		return FMath::Clamp(SoilFillProgress, 0.0f, 1.0f);
	}
	return SoilFillProgress > KINDA_SMALL_NUMBER
		? FMath::Clamp(SoilFillProgress, 0.0f, 1.0f)
		: 1.0f;
}

bool ABotanicusSalePotActor::IsSoilFull() const
{
	return !SoilItemKey.IsNone() &&
		GetSoilLevel() >= 1.0f - KINDA_SMALL_NUMBER;
}

bool ABotanicusSalePotActor::IsOnPreparationWorkbench() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABotanicusPreparationWorkbenchActor> WorkbenchIt(
			 World);
		 WorkbenchIt;
		 ++WorkbenchIt)
	{
		if (WorkbenchIt->IsLocationOnPreparationSlot(
				GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

bool ABotanicusSalePotActor::IsInteractorStillTargeting(
	AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Controller || !World ||
		FVector::DistSquared(
			Pawn->GetActorLocation(),
			GetActorLocation()) > FMath::Square(450.0f))
	{
		return false;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Query(
		SCENE_QUERY_STAT(BotanicusSalePotUse),
		false,
		Pawn);
	const bool bHit = World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			GetActorLocation(),
			ECC_Visibility,
			Query);
	if (bHit && Hit.GetActor() == this)
	{
		return true;
	}

	// The imported workbench mesh can cover a pot snapped into one of its
	// preparation slots.  Keep the continuous hold-to-fill validation strict,
	// but retry past that one supporting workbench so the action is not
	// cancelled on its first tick.
	ABotanicusPreparationWorkbenchActor* SupportingWorkbench = nullptr;
	for (TActorIterator<ABotanicusPreparationWorkbenchActor> WorkbenchIt(
			 World);
		 WorkbenchIt;
		 ++WorkbenchIt)
	{
		if (WorkbenchIt->IsLocationOnPreparationSlot(
				GetActorLocation(),
				55.0f))
		{
			SupportingWorkbench = *WorkbenchIt;
			break;
		}
	}

	if (!SupportingWorkbench ||
		!bHit ||
		Hit.GetActor() != SupportingWorkbench)
	{
		return false;
	}

	Query.AddIgnoredActor(SupportingWorkbench);
	FHitResult PotHit;
	return World->LineTraceSingleByChannel(
			PotHit,
			ViewLocation,
			GetActorLocation(),
			ECC_Visibility,
			Query) &&
		PotHit.GetActor() == this;
}

void ABotanicusSalePotActor::RefreshVisuals()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(PlantItemKey) : nullptr;
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* PlantDefinition =
		Plants ? Plants->FindPlantByHarvestItem(PlantItemKey) : nullptr;
	UStaticMesh* MaturePlantMesh =
		PlantDefinition && !PlantDefinition->MatureGrowthMesh.IsNull()
			? PlantDefinition->MatureGrowthMesh.LoadSynchronous()
			: nullptr;
	RefreshSoilVisual();
	if (PlantVisual)
	{
		PlantVisual->SetVisibility(IsReadyForSale());
		const float PlantBaseHeight =
			GetConfiguredSoilMaximumHeight();
		if (MaturePlantMesh)
		{
			if (PlantVisual->GetStaticMesh() != MaturePlantMesh)
			{
				PlantVisual->SetStaticMesh(MaturePlantMesh);
				PlantVisual->EmptyOverrideMaterials();
				PlantMaterial = nullptr;
			}
			const FBox MeshBounds = MaturePlantMesh->GetBoundingBox();
			const float DesiredHeight = FMath::Max(
				0.1f, PlantDefinition->MatureGrowthVisualHeight);
			const float UniformScale = DesiredHeight /
				FMath::Max(0.01f, MeshBounds.GetSize().Z);
			const FVector MeshCentre = MeshBounds.GetCenter();
			PlantVisual->SetRelativeScale3D(FVector(UniformScale));
			PlantVisual->SetRelativeLocation(FVector(
				-MeshCentre.X * UniformScale,
				-MeshCentre.Y * UniformScale,
				PlantBaseHeight - MeshBounds.Min.Z * UniformScale));
		}
		else
		{
			if (UStaticMesh* FallbackMesh = LoadObject<UStaticMesh>(
				nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
			{
				if (PlantVisual->GetStaticMesh() != FallbackMesh)
				{
					PlantVisual->SetStaticMesh(FallbackMesh);
					PlantVisual->EmptyOverrideMaterials();
					PlantMaterial = nullptr;
				}
			}
			float StemHeight = 80.0f;
			FVector FoliageShape;
			GetMatureSalePlantShape(
				PlantItemKey, StemHeight, FoliageShape);
			PlantVisual->SetRelativeLocation(FVector(
				0.0f, 0.0f, PlantBaseHeight + StemHeight));
			PlantVisual->SetRelativeScale3D(FoliageShape * 0.32f);
			if (Definition)
			{
				if (!PlantMaterial)
				{
					PlantMaterial =
						PlantVisual->CreateAndSetMaterialInstanceDynamic(0);
				}
				if (PlantMaterial)
				{
					PlantMaterial->SetVectorParameterValue(
						TEXT("Color"),
						SalePotPlantVisualColor(
							Definition->PlantColorTag));
				}
			}
		}
	}
	if (StemVisual)
	{
		float StemHeight = 80.0f;
		FVector FoliageShape;
		GetMatureSalePlantShape(
			PlantItemKey, StemHeight, FoliageShape);
		const float PlantBaseHeight =
			GetConfiguredSoilMaximumHeight();
		StemVisual->SetVisibility(
			IsReadyForSale() && MaturePlantMesh == nullptr);
		StemVisual->SetRelativeLocation(FVector(
			0.0f,
			0.0f,
			PlantBaseHeight + StemHeight * 0.5f));
		StemVisual->SetRelativeScale3D(FVector(
			0.035f, 0.035f, StemHeight / 100.0f));
	}
	if (!StatusText)
	{
		return;
	}
	const float SoilLevel = GetSoilLevel();
	const FString SoilStatus = SoilLevel >= 1.0f - KINDA_SMALL_NUMBER
		? TEXT("OUI")
		: SoilLevel > KINDA_SMALL_NUMBER
			? FString::Printf(
				TEXT("%d%%"),
				FMath::RoundToInt(SoilLevel * 100.0f))
			: TEXT("NON");
	const FString PlantStatus = IsReadyForSale()
		? Definition
			? Definition->DisplayName.ToString().ToUpper()
			: PlantItemKey.ToString().ToUpper()
		: TEXT("NON");
	const FString QualityStatus = Definition
		? SalePotPlantQualityLabel(Definition->PlantQualityTag)
		: TEXT("-");
	FString Progress;
	if (ActiveUser.IsValid())
	{
		const int32 SoilPercent =
			FMath::RoundToInt(GetSoilLevel() * 100.0f);
		Progress = PrimaryUseMode == EPrimaryUseMode::RemoveSoil
			? FString::Printf(
				TEXT("\nRETRAIT : %d%% RESTANT"),
				SoilPercent)
			: FString::Printf(
				TEXT("\nREMPLISSAGE : %d%%"),
				SoilPercent);
	}
	StatusText->SetText(
		FText::FromString(
			FString::Printf(
				TEXT(
					"POT DE VENTE\nETABLI : %s\nTERREAU COMPATIBLE : %s\nPLANTE : %s\nQUALITE : %s%s"),
				IsOnPreparationWorkbench() ? TEXT("OUI") : TEXT("NON"),
				*SoilStatus,
				*PlantStatus,
				*QualityStatus,
				*Progress)));
	StatusText->SetTextRenderColor(
		IsReadyForSale()
			? FColor(80, 255, 110)
			: FColor(245, 225, 160));
}

void ABotanicusSalePotActor::RefreshSoilVisual()
{
	if (SoilVisual)
	{
		SoilVisual->SetVisibility(false);
	}
	if (!SoilShapeVisual)
	{
		return;
	}

	const float FillAlpha = GetSoilLevel();
	const bool bIsFilling = ActiveUser.IsValid() &&
		PrimaryUseMode != EPrimaryUseMode::None;
	const FVector2D ConfiguredHorizontalOffset =
		GetConfiguredSoilHorizontalOffset();
	const float ConfiguredMaximumHeight =
		GetConfiguredSoilMaximumHeight();
	const float ConfiguredVolumeHeight =
		GetConfiguredSoilVolumeHeight();
	SoilShapeVisual->SetRelativeLocation(FVector(
		ConfiguredHorizontalOffset.X,
		ConfiguredHorizontalOffset.Y,
		ConfiguredMaximumHeight - ConfiguredVolumeHeight));
	if (ABotanicusPotSoilVisualActor* SoilActor =
			Cast<ABotanicusPotSoilVisualActor>(
				SoilShapeVisual->GetChildActor()))
	{
		SoilActor->ConfigureSoilShape(
			GetConfiguredSoilBottomRadii(),
			GetConfiguredSoilTopRadii(),
			ConfiguredVolumeHeight,
			FillAlpha,
			bIsFilling || FillAlpha > KINDA_SMALL_NUMBER,
			GetConfiguredSquareSoilProfile());
	}
}

void ABotanicusSalePotActor::SendInteractorMessage(
	AActor* Interactor,
	const FString& Message) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller)
	{
		Controller->ClientMessage(Message);
	}
}

void ABotanicusSalePotActor::OnRep_SalePotState()
{
	RefreshVisuals();
}
