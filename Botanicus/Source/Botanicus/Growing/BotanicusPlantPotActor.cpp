// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantPotActor.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Building/BotanicusElementalGreenhouseActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "Visuals/BotanicusPotSoilVisualActor.h"
#include "UI/BotanicusPlantGrowthWidget.h"
#include "UI/BotanicusPlantInspectionWidget.h"
#include "Blueprint/UserWidget.h"

namespace
{
float ReadBlueprintFloatSetting(
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

FVector2D ReadBlueprintVector2DSetting(
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

bool ReadBlueprintBoolSetting(
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

ABotanicusPlantPotActor::ABotanicusPlantPotActor()
{
	// Plant pots are authored through BP_Item_PlantPot (and its variants).
	// Preserve their Blueprint meshes and transforms instead of replacing the
	// appearance with the native catalogue cylinder fallback.
	bUseBlueprintAppearance = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	InteractionName =
		NSLOCTEXT("BotanicusGrowing", "PlantPot", "Pot de culture");
	InteractionAction =
		NSLOCTEXT("BotanicusGrowing", "MovePot", "Deplacer");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CylinderFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderFinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.34f));
	}

	SoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Soil"));
	SoilMesh->SetupAttachment(SceneRoot);
	SoilMesh->SetStaticMesh(nullptr);
	SoilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoilMesh->SetVisibility(false);

	SoilShapeVisual = CreateDefaultSubobject<UChildActorComponent>(
		TEXT("Adaptive Pot Soil"));
	SoilShapeVisual->SetupAttachment(SceneRoot);
	SoilShapeVisual->SetChildActorClass(
		ABotanicusPotSoilVisualActor::StaticClass());

	StemMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Stem"));
	StemMesh->SetupAttachment(SceneRoot);
	StemMesh->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	StemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FoliageMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Foliage"));
	FoliageMesh->SetupAttachment(SceneRoot);
	FoliageMesh->SetStaticMesh(
		SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
	FoliageMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("GrowingStatus"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(15.0f);
	StatusText->SetTextRenderColor(FColor(110, 220, 255));
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusText->SetVisibility(false);

	ContextActionText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("ContextAction"));
	ContextActionText->SetupAttachment(SceneRoot);
	ContextActionText->SetRelativeLocation(FVector(0.0f, 0.0f, 225.0f));
	ContextActionText->SetHorizontalAlignment(EHTA_Center);
	ContextActionText->SetVerticalAlignment(EVRTA_TextCenter);
	ContextActionText->SetWorldSize(18.0f);
	ContextActionText->SetTextRenderColor(FColor(80, 255, 110));
	ContextActionText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContextActionText->SetVisibility(false);

	PlantGrowthWidget = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("PlantGrowthWidget"));
	PlantGrowthWidget->SetupAttachment(SceneRoot);
	PlantGrowthWidget->SetWidgetSpace(EWidgetSpace::World);
	PlantGrowthWidget->SetDrawSize(FVector2D(360.0f, 180.0f));
	PlantGrowthWidget->SetPivot(FVector2D(0.5f, 0.5f));
	PlantGrowthWidget->SetRelativeLocation(
		FVector(0.0f, 0.0f, PlantGrowthWidgetHeight));
	PlantGrowthWidget->SetRelativeScale3D(FVector(PlantGrowthWidgetScale));
	PlantGrowthWidget->SetTintColorAndOpacity(FLinearColor(
		PlantGrowthWidgetBrightness, PlantGrowthWidgetBrightness,
		PlantGrowthWidgetBrightness, 1.0f));
	PlantGrowthWidget->SetTwoSided(true);
	PlantGrowthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlantGrowthWidget->SetWidgetClass(
		UBotanicusPlantGrowthWidget::StaticClass());
	PlantGrowthWidget->SetVisibility(false);

	PlantInspectionWidget = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("PlantInspectionWidget"));
	PlantInspectionWidget->SetupAttachment(SceneRoot);
	PlantInspectionWidget->SetWidgetSpace(EWidgetSpace::World);
	PlantInspectionWidget->SetDrawSize(FVector2D(420.0f, 260.0f));
	PlantInspectionWidget->SetPivot(FVector2D(0.5f, 0.5f));
	PlantInspectionWidget->SetRelativeScale3D(FVector(0.20f));
	PlantInspectionWidget->SetTintColorAndOpacity(FLinearColor(1.5f, 1.5f, 1.5f, 1.0f));
	PlantInspectionWidget->SetTwoSided(true);
	PlantInspectionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlantInspectionWidget->SetWidgetClass(
		UBotanicusPlantInspectionWidget::StaticClass());
	PlantInspectionWidget->SetVisibility(false);

	RefreshVisuals();
}

float ABotanicusPlantPotActor::GetSoilMaximumHeight() const
{
	return GetConfiguredSoilMaximumHeight();
}

FVector2D ABotanicusPlantPotActor::GetConfiguredSoilHorizontalOffset() const
{
	return ReadBlueprintVector2DSetting(
		this, TEXT("SoilHorizontalPosition"), SoilHorizontalOffset);
}

float ABotanicusPlantPotActor::GetConfiguredSoilMaximumHeight() const
{
	return ReadBlueprintFloatSetting(
		this, TEXT("SoilMaximumHeightSetting"), SoilSurfaceHeight);
}

FVector2D ABotanicusPlantPotActor::GetConfiguredSoilBottomRadii() const
{
	return ReadBlueprintVector2DSetting(
		this, TEXT("SoilBottomRadiiSetting"), SoilBottomRadii);
}

FVector2D ABotanicusPlantPotActor::GetConfiguredSoilTopRadii() const
{
	return ReadBlueprintVector2DSetting(
		this, TEXT("SoilTopRadiiSetting"), SoilTopRadii);
}

float ABotanicusPlantPotActor::GetConfiguredSoilVolumeHeight() const
{
	return FMath::Max(
		1.0f,
		ReadBlueprintFloatSetting(
			this, TEXT("SoilVolumeHeightSetting"), SoilVolumeHeight));
}

bool ABotanicusPlantPotActor::GetConfiguredSquareSoilProfile() const
{
	return ReadBlueprintBoolSetting(
		this, TEXT("UseSquareSoilProfile"), bSquareSoilProfile);
}

float ABotanicusPlantPotActor::GetConfiguredPlantBaseHeight() const
{
	return ReadBlueprintFloatSetting(
		this, TEXT("PlantingHeightSetting"), PlantBaseHeight);
}

void ABotanicusPlantPotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshSoilVisual();
	if (PlantGrowthWidget)
	{
		PlantGrowthWidget->SetRelativeLocation(
			FVector(0.0f, 0.0f, PlantGrowthWidgetHeight));
		PlantGrowthWidget->SetRelativeScale3D(
			FVector(PlantGrowthWidgetScale));
		PlantGrowthWidget->SetTintColorAndOpacity(FLinearColor(
			PlantGrowthWidgetBrightness, PlantGrowthWidgetBrightness,
			PlantGrowthWidgetBrightness, 1.0f));
	}
}

void ABotanicusPlantPotActor::BeginPlay()
{
	Super::BeginPlay();
	if (PlantGrowthWidget)
	{
		if (UClass* WidgetClass = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/Botanicus/UI/Plant/WBP_PlantGrowthInfo.WBP_PlantGrowthInfo_C")))
		{
			PlantGrowthWidget->SetWidgetClass(WidgetClass);
		}
		PlantGrowthWidget->InitWidget();
		RefreshVisuals();
	}
	if (PlantInspectionWidget)
	{
		if (UClass* WidgetClass = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/Botanicus/UI/Plant/Inspection/WBP_PlantInspection.WBP_PlantInspection_C")))
		{
			PlantInspectionWidget->SetWidgetClass(WidgetClass);
		}
		PlantInspectionWidget->InitWidget();
		RefreshInspectionWidget();
	}
}

void ABotanicusPlantPotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StatusText)
	{
		const UWorld* World = GetWorld();
		const APlayerController* LocalPlayerController =
			World ? World->GetFirstPlayerController() : nullptr;
		const APlayerCameraManager* CameraManager =
			LocalPlayerController
				? LocalPlayerController->PlayerCameraManager
				: nullptr;
		if (CameraManager)
		{
			const FVector CameraLocation =
				CameraManager->GetCameraLocation();
			StatusText->SetWorldRotation(
				(CameraLocation -
				 StatusText->GetComponentLocation()).Rotation());
			if (ContextActionText)
			{
				ContextActionText->SetWorldRotation(
					(CameraLocation -
					 ContextActionText->GetComponentLocation()).Rotation());
			}
			if (PlantGrowthWidget)
			{
				PlantGrowthWidget->SetWorldRotation(
					(CameraLocation -
					 PlantGrowthWidget->GetComponentLocation()).Rotation());
			}
			if (PlantInspectionWidget)
			{
				FVector TowardCamera = CameraLocation - GetActorLocation();
				TowardCamera.Z = 0.0f;
				TowardCamera = TowardCamera.GetSafeNormal();
				if (TowardCamera.IsNearlyZero())
				{
					TowardCamera = GetActorForwardVector();
				}
				FVector CameraRight = CameraManager->GetCameraRotation().
					RotateVector(FVector::RightVector);
				CameraRight.Z = 0.0f;
				CameraRight = CameraRight.GetSafeNormal();
				PlantInspectionWidget->SetWorldLocation(
					GetActorLocation() +
					FVector::UpVector * PlantInspectionWidgetHeight +
					TowardCamera * PlantInspectionWidgetSideOffset +
					CameraRight * PlantInspectionWidgetHorizontalOffset);
				FRotator InspectionRotation =
					(CameraLocation -
					 PlantInspectionWidget->GetComponentLocation()).Rotation();
				InspectionRotation.Pitch = 0.0f;
				InspectionRotation.Roll = 0.0f;
				PlantInspectionWidget->SetWorldRotation(InspectionRotation);
			}
		}
		if (PlantGrowthWidget)
		{
			PlantGrowthWidget->SetVisibility(
				!PlantKey.IsNone() &&
				!ActorHasTag(TEXT("BotanicusPlacementPreview")));
		}
		if (PlantInspectionWidget)
		{
			PlantInspectionWidget->SetVisibility(
				bInspectionVisible && !PlantKey.IsNone() &&
				!ActorHasTag(TEXT("BotanicusPlacementPreview")));
			if (bInspectionVisible)
			{
				RefreshInspectionWidget();
			}
		}
	}
	RefreshLocalContextAction();
	if (!HasAuthority())
	{
		return;
	}
	const bool bPrimaryUseChanged = UpdatePrimaryUse(DeltaSeconds);
	if (PlantKey.IsNone())
	{
		if (bPrimaryUseChanged)
		{
			RefreshVisuals();
			ForceNetUpdate();
		}
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* Definition =
		Plants ? Plants->FindPlant(PlantKey) : nullptr;
	if (!Definition)
	{
		return;
	}

	const bool bElementalChanged =
		ProcessElementalInteractions(*Definition);
	if (bElementalDead)
	{
		if (bElementalChanged || bPrimaryUseChanged)
		{
			RefreshVisuals();
			ForceNetUpdate();
		}
		return;
	}

	const float PreviousWater = WaterLevel;
	const float PreviousGrowth = GrowthProgress;
	const float PreviousCareScore = CareScore;
	WaterLevel = FMath::Clamp(
		WaterLevel -
			FMath::Max(0.0f, Definition->WaterConsumptionPerSecond) *
				DeltaSeconds,
		0.0f,
		1.0f);
	if (IsInCompatibleGreenhouse(*Definition) &&
		WaterLevel >= Definition->MinimumHealthyWater &&
		WaterLevel <= Definition->MaximumHealthyWater &&
		GrowthProgress < 1.0f)
	{
		const float PreviousGrowthForCare = GrowthProgress;
		GrowthProgress = FMath::Clamp(
			GrowthProgress +
				DeltaSeconds /
					FMath::Max(
						1.0f,
						Definition->GrowthDurationSeconds),
			0.0f,
			1.0f);
		const float WaterMidpoint =
			(Definition->MinimumHealthyWater +
			 Definition->MaximumHealthyWater) *
			0.5f;
		const float WaterHalfRange =
			FMath::Max(
				0.01f,
				(Definition->MaximumHealthyWater -
				 Definition->MinimumHealthyWater) *
					0.5f);
		const float WaterCare =
			1.0f -
			FMath::Clamp(
				FMath::Abs(WaterLevel - WaterMidpoint) /
					WaterHalfRange,
				0.0f,
				1.0f);
		CareScore = FMath::Clamp(
			CareScore +
				(GrowthProgress - PreviousGrowthForCare) * WaterCare,
			0.0f,
			1.0f);
	}

	if (!FMath::IsNearlyEqual(PreviousWater, WaterLevel, 0.0001f) ||
		!FMath::IsNearlyEqual(PreviousGrowth, GrowthProgress, 0.0001f) ||
		!FMath::IsNearlyEqual(PreviousCareScore, CareScore, 0.0001f) ||
		bPrimaryUseChanged ||
		bElementalChanged)
	{
		RefreshVisuals();
		ForceNetUpdate();
	}
}

void ABotanicusPlantPotActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusPlantPotActor, bHasSoil);
	DOREPLIFETIME(ABotanicusPlantPotActor, PlantKey);
	DOREPLIFETIME(ABotanicusPlantPotActor, WaterLevel);
	DOREPLIFETIME(ABotanicusPlantPotActor, GrowthProgress);
	DOREPLIFETIME(ABotanicusPlantPotActor, CareScore);
	DOREPLIFETIME(ABotanicusPlantPotActor, WateringCount);
	DOREPLIFETIME(ABotanicusPlantPotActor, PlantingStartServerTime);
	DOREPLIFETIME(ABotanicusPlantPotActor, bElementalDead);
	DOREPLIFETIME(ABotanicusPlantPotActor, SoilFillProgress);
	DOREPLIFETIME(ABotanicusPlantPotActor, bWateringActive);
	DOREPLIFETIME(ABotanicusPlantPotActor, HarvestProgress);
}

FBotanicusInteractionPrompt
ABotanicusPlantPotActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	const FName SelectedItemKey = GetSelectedItemKey(Interactor);

	if (SelectedItemKey == TEXT("GardenTrowel") &&
		PlantKey.IsNone() &&
		GetSoilLevel() > KINDA_SMALL_NUMBER)
	{
		Prompt.ActionText = NSLOCTEXT(
			"BotanicusGrowing",
			"RemovePotSoil",
			"Retirer le terreau");
	}
	else if (!IsSoilFull())
	{
		Prompt.ActionText =
			SelectedItemKey == TEXT("PottingSoil")
				? NSLOCTEXT(
					"BotanicusGrowing",
					"FillPot",
					"Remplir de terreau")
				: NSLOCTEXT(
					"BotanicusGrowing",
					"NeedsSoil",
					"Sélectionner du terreau");
	}
	else if (PlantKey.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		Prompt.ActionText =
			Plants && Plants->FindPlantBySeed(SelectedItemKey)
				? NSLOCTEXT(
					"BotanicusGrowing",
					"PlantSeed",
					"Planter la graine")
				: NSLOCTEXT(
					"BotanicusGrowing",
					"NeedsSeed",
					"Sélectionner des graines");
	}
	else
	{
		const ABotanicusCharacter* Character =
			Cast<ABotanicusCharacter>(Interactor);
		const UBotanicusQuickBarComponent* QuickBar =
			Character ? Character->GetQuickBarComponent() : nullptr;
		Prompt.ActionText =
			QuickBar && QuickBar->HasSelectedWateringCan()
				? NSLOCTEXT(
					"BotanicusGrowing",
					"WaterPlant",
					"Arroser")
				: NSLOCTEXT(
					"BotanicusGrowing",
					"InspectPlant",
					"Observer");
	}
	if (IsMature())
	{
		const FBotanicusPlantDefinition* Definition =
			GetPlantDefinition();
		const FText PlantName =
			Definition && !Definition->DisplayName.IsEmpty()
				? Definition->DisplayName
				: FText::FromName(PlantKey);
		Prompt.TargetName = PlantName;
		if (CanHarvestWithInteractor(Interactor))
		{
			Prompt.ActionText = FText::Format(
				NSLOCTEXT(
					"BotanicusGrowing",
					"HarvestReady",
					"Maintenir clic gauche 1 s : utiliser la petite pelle pour recolter {0}"),
				PlantName);
			Prompt.bCanInteract = true;
		}
		else
		{
			Prompt.ActionText = FText::Format(
				NSLOCTEXT(
					"BotanicusGrowing",
					"HarvestNeedsTrowel",
					"Petite pelle requise pour recolter {0}"),
				PlantName);
			Prompt.bCanInteract = false;
		}
	}
	else
	{
		Prompt.ActionText = InteractionAction;
		Prompt.bCanInteract = false;
	}
	return Prompt;
}

bool ABotanicusPlantPotActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return CanHarvestWithInteractor(Interactor);
}

void ABotanicusPlantPotActor::Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"E sert a deplacer le pot. Utilisez le clic gauche pour agir."));
	}
}

void ABotanicusPlantPotActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (StatusText)
	{
		StatusText->SetVisibility(false);
	}
	if (ContextActionText)
	{
		ContextActionText->SetVisibility(false);
	}
	if (PlantGrowthWidget)
	{
		PlantGrowthWidget->SetVisibility(false);
	}
	if (PlantInspectionWidget)
	{
		PlantInspectionWidget->SetVisibility(false);
	}
}

void ABotanicusPlantPotActor::BeginPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority())
	{
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
	if (ActivePrimaryUser.IsValid())
	{
		if (ActivePrimaryUser.Get() != Character)
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Ce pot est deja utilise par un autre joueur."));
		}
		return;
	}
	const FName SelectedItemKey =
		QuickBar->GetSelectedSlot().ItemKey;
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Plant pot interaction: selected=%s soil=%s plant=%s water=%.2f waterings=%d."),
		*SelectedItemKey.ToString(),
		bHasSoil ? TEXT("yes") : TEXT("no"),
		PlantKey.IsNone() ? TEXT("none") : *PlantKey.ToString(),
		WaterLevel,
		WateringCount);
	if (bElementalDead)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Cette plante est morte a cause d'une reaction elementaire."));
		return;
	}

	if (SelectedItemKey == TEXT("GardenTrowel") &&
		PlantKey.IsNone() &&
		GetSoilLevel() > KINDA_SMALL_NUMBER)
	{
		SoilFillProgress = GetSoilLevel();
		ActivePrimaryUser = Character;
		PrimaryUseMode = EPrimaryUseMode::RemoveSoil;
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Maintenez le clic gauche pour retirer le terreau (%d%% restant)."),
				FMath::RoundToInt(SoilFillProgress * 100.0f)));
	}
	else if (!IsSoilFull())
	{
		if (SelectedItemKey != TEXT("PottingSoil"))
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Sélectionnez une dose de terreau dans la hotbar."));
			return;
		}
		// A partially emptied pot can be filled again from its current level.
		bHasSoil = false;
		ActivePrimaryUser = Character;
		PrimaryUseMode = EPrimaryUseMode::FillSoil;
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Maintenez le clic gauche pour verser le terreau (%d%%)."),
				FMath::RoundToInt(SoilFillProgress * 100.0f)));
	}
	else if (PlantKey.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		const FBotanicusPlantDefinition* Definition =
			Plants ? Plants->FindPlantBySeed(SelectedItemKey) : nullptr;
		if (!Definition || !QuickBar->ConsumeSelectedItem(1))
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Sélectionnez un sachet de graines compatible."));
			return;
		}
		PlantKey = Definition->PlantKey;
		PlantingStartServerTime = GetWorld()
			? GetWorld()->GetTimeSeconds()
			: 0.0f;
		bElementalDead = false;
		GrowthProgress = 0.02f;
		CareScore = 0.01f;
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("%s planté. Il faut maintenant arroser."),
				*Definition->DisplayName.ToString()));
	}
	else if (CanHarvestWithInteractor(Interactor))
	{
		ActivePrimaryUser = Character;
		PrimaryUseMode = EPrimaryUseMode::Harvest;
		// A mature plant is harvested with one click. Immature plants use the
		// separate held trowel workflow managed by the player controller.
		HarvestProgress = 1.0f;
		UpdatePrimaryUse(0.0f);
	}
	else if (QuickBar->HasSelectedWateringCan())
	{
		if (QuickBar->GetSelectedWateringCanWaterLevel() <=
			KINDA_SMALL_NUMBER)
		{
			SendInteractorMessage(
				Interactor,
				TEXT(
					"L'arrosoir est vide. Remplissez-le a une reserve d'eau."));
			return;
		}
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		const FBotanicusPlantDefinition* Definition =
			Plants ? Plants->FindPlant(PlantKey) : nullptr;
		if (Definition)
		{
			ActivePrimaryUser = Character;
			PrimaryUseMode = EPrimaryUseMode::Water;
			bWateringActive = true;
			++WateringCount;
			SendInteractorMessage(
				Interactor,
				FString::Printf(
					TEXT("Pot arrosé : humidité %d%%."),
					FMath::RoundToInt(WaterLevel * 100.0f)));
		}
	}
	else
	{
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Croissance %d%% — humidité %d%%."),
				FMath::RoundToInt(GrowthProgress * 100.0f),
				FMath::RoundToInt(WaterLevel * 100.0f)));
		return;
	}

	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusPlantPotActor::EndPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	if (ActivePrimaryUser.IsValid() &&
		ActivePrimaryUser.Get() != Character)
	{
		return;
	}

	if (PrimaryUseMode == EPrimaryUseMode::FillSoil &&
		!IsSoilFull())
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
	else if (PrimaryUseMode == EPrimaryUseMode::Water)
	{
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Arrosage arrete : eau %d%%."),
				FMath::RoundToInt(WaterLevel * 100.0f)));
	}
	else if (PrimaryUseMode == EPrimaryUseMode::Harvest &&
		HarvestProgress > 0.0f)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Recolte annulee."));
	}

	ActivePrimaryUser.Reset();
	PrimaryUseMode = EPrimaryUseMode::None;
	HarvestProgress = 0.0f;
	bWateringActive = false;
	RefreshVisuals();
	ForceNetUpdate();
}

bool ABotanicusPlantPotActor::UpdatePrimaryUse(float DeltaSeconds)
{
	if (PrimaryUseMode == EPrimaryUseMode::None)
	{
		return false;
	}

	ABotanicusCharacter* Character = ActivePrimaryUser.Get();
	const bool bCharacterValid =
		IsValid(Character) &&
		IsInteractorStillTargeting(Character);
	const FName SelectedItemKey =
		bCharacterValid
			? GetSelectedItemKey(Character)
			: NAME_None;

	if (PrimaryUseMode == EPrimaryUseMode::FillSoil)
	{
		if (!bCharacterValid ||
			SelectedItemKey != TEXT("PottingSoil") ||
			bHasSoil)
		{
			EndPrimaryUse(Character);
			return true;
		}

		SoilFillProgress = FMath::Clamp(
			SoilFillProgress +
				DeltaSeconds /
					FMath::Max(0.1f, SoilFillDuration),
			0.0f,
			1.0f);
		if (SoilFillProgress >= 1.0f)
		{
			UBotanicusQuickBarComponent* QuickBar =
				Character->GetQuickBarComponent();
			if (QuickBar &&
				QuickBar->ConsumeSelectedItem(1))
			{
				bHasSoil = true;
				SendInteractorMessage(
					Character,
					TEXT("Le pot est rempli de terreau."));
			}
			ActivePrimaryUser.Reset();
			PrimaryUseMode = EPrimaryUseMode::None;
			SoilFillProgress = 0.0f;
		}
		return true;
	}

	if (PrimaryUseMode == EPrimaryUseMode::RemoveSoil)
	{
		if (!bCharacterValid ||
			SelectedItemKey != TEXT("GardenTrowel") ||
			!PlantKey.IsNone() ||
			GetSoilLevel() <= KINDA_SMALL_NUMBER)
		{
			EndPrimaryUse(Character);
			return true;
		}

		SoilFillProgress = FMath::Clamp(
			GetSoilLevel() -
				DeltaSeconds / FMath::Max(0.1f, SoilRemovalDuration),
			0.0f,
			1.0f);
		if (SoilFillProgress <= KINDA_SMALL_NUMBER)
		{
			SoilFillProgress = 0.0f;
			bHasSoil = false;
			WaterLevel = 0.0f;
			WateringCount = 0;
			SendInteractorMessage(
				Character,
				TEXT("Le terreau a ete retire du pot."));
			ActivePrimaryUser.Reset();
			PrimaryUseMode = EPrimaryUseMode::None;
		}
		return true;
	}

	if (PrimaryUseMode == EPrimaryUseMode::Harvest)
	{
		const FBotanicusPlantDefinition* Definition =
			GetPlantDefinition();
		if (!bCharacterValid ||
			!Definition ||
			!CanHarvestWithInteractor(Character))
		{
			EndPrimaryUse(Character);
			return true;
		}

		HarvestProgress = FMath::Clamp(
			HarvestProgress +
				DeltaSeconds /
					FMath::Max(
						0.1f,
						Definition->HarvestDurationSeconds),
			0.0f,
			1.0f);
		if (HarvestProgress >= 1.0f)
		{
			UBotanicusQuickBarComponent* QuickBar =
				Character->GetQuickBarComponent();
			int32 AddedSlotIndex = INDEX_NONE;
			if (!QuickBar ||
				!QuickBar->AddItem(
					GetQualityHarvestItemKey(*Definition),
					FMath::Max(1, Definition->HarvestQuantity),
					AddedSlotIndex))
			{
				SendInteractorMessage(
					Character,
					TEXT(
						"Recolte impossible : liberez de la place dans la hotbar."));
				EndPrimaryUse(Character);
				return true;
			}

			const FString HarvestedPlantName =
				Definition->DisplayName.ToString();
			const FString HarvestedQuality =
				GetPlantQualityLabel();
			const int32 HarvestedQuantity =
				FMath::Max(1, Definition->HarvestQuantity);
			PlantKey = NAME_None;
			PlantingStartServerTime = 0.0f;
			bInspectionVisible = false;
			WaterLevel = 0.0f;
			GrowthProgress = 0.0f;
			CareScore = 0.0f;
			WateringCount = 0;
			bElementalDead = false;
			HarvestProgress = 0.0f;
			ActivePrimaryUser.Reset();
			PrimaryUseMode = EPrimaryUseMode::None;
			SendInteractorMessage(
				Character,
				FString::Printf(
					TEXT("%s recolte (%s) : x%d ajoute a la hotbar."),
					*HarvestedPlantName,
					*HarvestedQuality,
					HarvestedQuantity));
		}
		return true;
	}

	if (!bCharacterValid ||
		PlantKey.IsNone())
	{
		EndPrimaryUse(Character);
		return true;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* Definition =
		Plants ? Plants->FindPlant(PlantKey) : nullptr;
	if (!Definition)
	{
		EndPrimaryUse(Character);
		return true;
	}

	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!QuickBar || !QuickBar->HasSelectedWateringCan() ||
		QuickBar->GetSelectedWateringCanWaterLevel() <=
			KINDA_SMALL_NUMBER)
	{
		if (IsValid(Character))
		{
			SendInteractorMessage(
				Character,
				TEXT(
					"L'arrosoir est vide. L'arrosage s'arrete."));
		}
		EndPrimaryUse(Character);
		return true;
	}

	if (WaterLevel >= 1.0f - KINDA_SMALL_NUMBER)
	{
		SendInteractorMessage(
			Character,
			TEXT(
				"La plante est suffisamment arrosee : eau 100%."));
		EndPrimaryUse(Character);
		return true;
	}

	const float RequestedWater =
		FMath::Max(0.0f, Definition->WaterAddedPerUse) *
			DeltaSeconds;
	const float PreviousWaterLevel = WaterLevel;
	WaterLevel = FMath::Clamp(
		WaterLevel + RequestedWater,
		0.0f,
		1.0f);
	const float WaterActuallyAdded =
		WaterLevel - PreviousWaterLevel;
	if (WaterActuallyAdded > KINDA_SMALL_NUMBER &&
		RequestedWater > KINDA_SMALL_NUMBER)
	{
		QuickBar->ConsumeSelectedWateringCanWater(
			0.12f * DeltaSeconds *
				(WaterActuallyAdded / RequestedWater));
	}
	if (WaterLevel >= 1.0f - KINDA_SMALL_NUMBER)
	{
		SendInteractorMessage(
			Character,
			TEXT(
				"La plante est suffisamment arrosee : eau 100%."));
		EndPrimaryUse(Character);
	}
	return true;
}

void ABotanicusPlantPotActor::RestoreWateringCount(
	int32 InWateringCount)
{
	if (!HasAuthority())
	{
		return;
	}
	WateringCount = FMath::Max(0, InWateringCount);
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusPlantPotActor::RestoreGrowingState(
	bool bInHasSoil,
	FName InPlantKey,
	float InWaterLevel,
	float InGrowthProgress,
	float InCareScore,
	bool bInElementalDead)
{
	if (!HasAuthority())
	{
		return;
	}
	bHasSoil = bInHasSoil;
	SoilFillProgress = 0.0f;
	PlantKey = InPlantKey;
	WaterLevel = FMath::Clamp(InWaterLevel, 0.0f, 1.0f);
	GrowthProgress = FMath::Clamp(InGrowthProgress, 0.0f, 1.0f);
	CareScore = FMath::Clamp(InCareScore, 0.0f, 1.0f);
	bElementalDead = bInElementalDead && !PlantKey.IsNone();
	if (PlantKey.IsNone())
	{
		PlantingStartServerTime = 0.0f;
		bInspectionVisible = false;
	}
	else if (GetWorld())
	{
		const FBotanicusPlantDefinition* Definition = GetPlantDefinition();
		const float EstimatedAge = Definition
			? GrowthProgress * Definition->GrowthDurationSeconds
			: 0.0f;
		PlantingStartServerTime =
			GetWorld()->GetTimeSeconds() - EstimatedAge;
	}
	RefreshVisuals();
	ForceNetUpdate();
}

FName ABotanicusPlantPotActor::GetCurrentHarvestItemKey() const
{
	const FBotanicusPlantDefinition* Definition = GetPlantDefinition();
	return Definition
		? GetQualityHarvestItemKey(*Definition)
		: NAME_None;
}

void ABotanicusPlantPotActor::SetInspectionVisible(const bool bVisible)
{
	bInspectionVisible = bVisible && !PlantKey.IsNone();
	if (PlantInspectionWidget)
	{
		PlantInspectionWidget->SetVisibility(bInspectionVisible);
	}
	RefreshInspectionWidget();
}

void ABotanicusPlantPotActor::RefreshInspectionWidget()
{
	UBotanicusPlantInspectionWidget* Widget = PlantInspectionWidget
		? Cast<UBotanicusPlantInspectionWidget>(PlantInspectionWidget->GetWidget())
		: nullptr;
	const FBotanicusPlantDefinition* Definition = GetPlantDefinition();
	if (!Widget || !Definition || PlantKey.IsNone())
	{
		return;
	}

	const UBotanicusItemCatalogSubsystem* Items = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>()
		: nullptr;
	const FBotanicusItemDefinition* HarvestItem = Items
		? Items->FindItem(GetCurrentHarvestItemKey())
		: nullptr;
	const int32 Price = HarvestItem ? HarvestItem->SalePrice : 0;

	FText ElementName;
	const TCHAR* ElementTexturePath = nullptr;
	switch (Definition->Element)
	{
	case EBotanicusPlantElement::Fire:
		ElementName = NSLOCTEXT("BotanicusPlantUI", "Fire", "FEU");
		ElementTexturePath = TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Fire.T_PlantElement_Fire");
		break;
	case EBotanicusPlantElement::Water:
		ElementName = NSLOCTEXT("BotanicusPlantUI", "Water", "EAU");
		ElementTexturePath = TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Water.T_PlantElement_Water");
		break;
	case EBotanicusPlantElement::Ice:
		ElementName = NSLOCTEXT("BotanicusPlantUI", "Ice", "GLACE");
		ElementTexturePath = TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Ice.T_PlantElement_Ice");
		break;
	case EBotanicusPlantElement::Shadow:
		ElementName = NSLOCTEXT("BotanicusPlantUI", "Shadow", "TENEBRES");
		ElementTexturePath = TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Shadow.T_PlantElement_Shadow");
		break;
	default:
		ElementName = NSLOCTEXT("BotanicusPlantUI", "Normal", "NORMAL");
		ElementTexturePath = TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Normal.T_PlantElement_Normal");
		break;
	}
	const float AgeSeconds = GetWorld()
		? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - PlantingStartServerTime)
		: 0.0f;
	Widget->SetInspectionData(
		FText::FromString(GetPlantQualityLabel()), Price, ElementName,
		LoadObject<UTexture2D>(nullptr, ElementTexturePath),
		WateringCount, AgeSeconds);
}

void ABotanicusPlantPotActor::ApplyElementalInfluence(
	EBotanicusPlantElement SourceElement,
	const FVector& SourceLocation)
{
	if (!HasAuthority() ||
		bElementalDead ||
		PlantKey.IsNone())
	{
		return;
	}
	const FBotanicusPlantDefinition* Definition =
		GetPlantDefinition();
	if (!Definition ||
		FVector::DistSquared(
			GetActorLocation(),
			SourceLocation) >
			FMath::Square(500.0f))
	{
		return;
	}
	const bool bBurnedByFire =
		SourceElement == EBotanicusPlantElement::Fire &&
		Definition->Element == EBotanicusPlantElement::Normal;
	const bool bExtinguishedByWater =
		SourceElement == EBotanicusPlantElement::Water &&
		Definition->Element == EBotanicusPlantElement::Fire;
	if (bBurnedByFire || bExtinguishedByWater)
	{
		bElementalDead = true;
		EndPrimaryUse(ActivePrimaryUser.Get());
		RefreshVisuals();
		ForceNetUpdate();
	}
}

bool ABotanicusPlantPotActor::IsInCompatibleGreenhouse(
	const FBotanicusPlantDefinition& Definition) const
{
	if (Definition.Element == EBotanicusPlantElement::Normal)
	{
		return true;
	}
	return ABotanicusElementalGreenhouseActor::
		FindGreenhouseElementAtLocation(
			GetWorld(),
			GetActorLocation()) == Definition.Element;
}

bool ABotanicusPlantPotActor::ProcessElementalInteractions(
	const FBotanicusPlantDefinition& Definition)
{
	if (bElementalDead ||
		(Definition.Element != EBotanicusPlantElement::Fire &&
		 Definition.Element != EBotanicusPlantElement::Water))
	{
		return false;
	}

	const FVector SourceLocation = GetActorLocation();
	const float Radius = FMath::Max(
		0.0f,
		Definition.ElementalInteractionRadius);
	for (TActorIterator<ABotanicusPlantPotActor> It(GetWorld());
		 It;
		 ++It)
	{
		if (*It == this ||
			FVector::DistSquared(
				It->GetActorLocation(),
				SourceLocation) > FMath::Square(Radius))
		{
			continue;
		}
		if (ABotanicusMultiPlantPotActor* MultiPlanter =
			Cast<ABotanicusMultiPlantPotActor>(*It))
		{
			MultiPlanter->ApplyElementalInfluence(
				Definition.Element,
				SourceLocation);
		}
		else
		{
			It->ApplyElementalInfluence(
				Definition.Element,
				SourceLocation);
		}
	}
	return false;
}

const FBotanicusPlantDefinition*
ABotanicusPlantPotActor::GetPlantDefinition() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	return Plants ? Plants->FindPlant(PlantKey) : nullptr;
}

float ABotanicusPlantPotActor::GetCareRating() const
{
	return PlantKey.IsNone()
		? 0.0f
		: FMath::Clamp(
			CareScore / FMath::Max(0.02f, GrowthProgress),
			0.0f,
			1.0f);
}

FName ABotanicusPlantPotActor::GetPlantQualityTag() const
{
	const float Rating = GetCareRating();
	if (Rating >= 0.80f)
	{
		return TEXT("Exceptional");
	}
	if (Rating >= 0.50f)
	{
		return TEXT("Beautiful");
	}
	return TEXT("Standard");
}

FString ABotanicusPlantPotActor::GetPlantQualityLabel() const
{
	const FName Quality = GetPlantQualityTag();
	if (Quality == TEXT("Exceptional"))
	{
		return TEXT("EXCEPTIONNELLE");
	}
	if (Quality == TEXT("Beautiful"))
	{
		return TEXT("BELLE");
	}
	return TEXT("STANDARD");
}

FName ABotanicusPlantPotActor::GetQualityHarvestItemKey(
	const FBotanicusPlantDefinition& Definition) const
{
	FString Key = Definition.HarvestItemKey.ToString();
	const FName Quality = GetPlantQualityTag();
	if (Quality == TEXT("Beautiful"))
	{
		Key += TEXT("_Beautiful");
	}
	else if (Quality == TEXT("Exceptional"))
	{
		Key += TEXT("_Exceptional");
	}
	return FName(*Key);
}

bool ABotanicusPlantPotActor::CanHarvestWithInteractor(
	AActor* Interactor) const
{
	if (!IsMature())
	{
		return false;
	}
	const FBotanicusPlantDefinition* Definition =
		GetPlantDefinition();
	return Definition &&
		!Definition->HarvestItemKey.IsNone() &&
		!Definition->HarvestToolItemKey.IsNone() &&
		GetSelectedItemKey(Interactor) ==
			Definition->HarvestToolItemKey;
}

bool ABotanicusPlantPotActor::IsInteractorStillTargeting(
	AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn)
	{
		return false;
	}

	const AController* Controller = Pawn->GetController();
	const FVector ViewLocation = Pawn->GetPawnViewLocation();
	const FVector ViewDirection =
		Controller
			? Controller->GetControlRotation().Vector()
			: Pawn->GetActorForwardVector();
	FVector TargetOrigin;
	FVector TargetExtent;
	GetActorBounds(true, TargetOrigin, TargetExtent);
	const FVector ToTarget = TargetOrigin - ViewLocation;
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER ||
		Distance > 450.0f ||
		FVector::DotProduct(
			ViewDirection,
			ToTarget / Distance) <
			FMath::Cos(FMath::DegreesToRadians(45.0f)))
	{
		return false;
	}

	return true;
}

void ABotanicusPlantPotActor::RefreshLocalContextAction()
{
	// All contextual actions are now presented by WBP_HUD_Interaction.
	// Keeping this world-space text hidden also prevents duplicated guidance.
	if (ContextActionText)
	{
		ContextActionText->SetVisibility(false);
	}
}

FName ABotanicusPlantPotActor::GetSelectedItemKey(
	AActor* Interactor) const
{
	const ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	return QuickBar
		? QuickBar->GetSelectedSlot().ItemKey
		: NAME_None;
}

float ABotanicusPlantPotActor::GetSoilLevel() const
{
	if (!bHasSoil)
	{
		return FMath::Clamp(SoilFillProgress, 0.0f, 1.0f);
	}
	return SoilFillProgress > KINDA_SMALL_NUMBER
		? FMath::Clamp(SoilFillProgress, 0.0f, 1.0f)
		: 1.0f;
}

bool ABotanicusPlantPotActor::IsSoilFull() const
{
	return bHasSoil && GetSoilLevel() >= 1.0f - KINDA_SMALL_NUMBER;
}

void ABotanicusPlantPotActor::RefreshVisuals()
{
	RefreshSoilVisual();
	const bool bHasPlant = !PlantKey.IsNone();
	const FBotanicusPlantDefinition* Definition =
		GetPlantDefinition();
	float HeightMultiplier = 1.0f;
	FVector FoliageShape(1.0f, 1.0f, 0.7f);
	const float VisualGrowth =
		FMath::Clamp(GrowthProgress, 0.02f, 1.0f);
	const float LegacyStemHeight =
		FMath::Lerp(8.0f, 80.0f, VisualGrowth) *
		HeightMultiplier;
	const float ConfiguredPlantBaseHeight =
		GetConfiguredPlantBaseHeight();
	const float LegacyFoliageScale =
		FMath::Lerp(0.06f, 0.32f, VisualGrowth);

	TSoftObjectPtr<UStaticMesh> GrowthStageMeshReference;
	if (Definition)
	{
		if (GrowthProgress >= 1.0f - KINDA_SMALL_NUMBER)
		{
			GrowthStageMeshReference = Definition->MatureGrowthMesh;
		}
		else if (GrowthProgress >= 0.70f)
		{
			GrowthStageMeshReference = Definition->LargeGrowthMesh;
		}
		else if (GrowthProgress >= 0.30f)
		{
			GrowthStageMeshReference = Definition->MediumGrowthMesh;
		}
		else
		{
			GrowthStageMeshReference = Definition->SmallGrowthMesh;
		}
	}
	UStaticMesh* GrowthStageMesh =
		GrowthStageMeshReference.IsNull()
			? nullptr
			: GrowthStageMeshReference.LoadSynchronous();
	const bool bUsesGrowthStageMesh = GrowthStageMesh != nullptr;
	float FoliageTop = 0.0f;

	if (StemMesh)
	{
		StemMesh->SetVisibility(bHasPlant && !bUsesGrowthStageMesh);
		if (!bUsesGrowthStageMesh)
		{
			StemMesh->SetRelativeLocation(FVector(
				0.0f,
				0.0f,
				ConfiguredPlantBaseHeight +
					LegacyStemHeight * 0.5f));
			StemMesh->SetRelativeScale3D(FVector(
				0.035f,
				0.035f,
				LegacyStemHeight / 100.0f));
		}
	}
	if (FoliageMesh)
	{
		FoliageMesh->SetVisibility(bHasPlant);
		if (bUsesGrowthStageMesh)
		{
			if (FoliageMesh->GetStaticMesh() != GrowthStageMesh)
			{
				FoliageMesh->SetStaticMesh(GrowthStageMesh);
				FoliageMesh->EmptyOverrideMaterials();
				FoliageMaterial = nullptr;
			}

			const FBox MeshBounds = GrowthStageMesh->GetBoundingBox();
			const float MeshHeight =
				FMath::Max(0.01f, MeshBounds.GetSize().Z);
			const float MinimumHeight = FMath::Max(
				0.1f, Definition->MinimumGrowthVisualHeight);
			const float MatureHeight = FMath::Max(
				MinimumHeight, Definition->MatureGrowthVisualHeight);
			// The target height depends only on the global growth percentage,
			// not on the selected mesh. Therefore both meshes have exactly the
			// same height on either side of the 30% and 70% transitions.
			const float DesiredHeight = FMath::Lerp(
				MinimumHeight, MatureHeight, VisualGrowth);
			const float UniformScale = DesiredHeight / MeshHeight;
			const FVector MeshCentre = MeshBounds.GetCenter();
			FoliageMesh->SetRelativeScale3D(FVector(UniformScale));
			FoliageMesh->SetRelativeLocation(FVector(
				-MeshCentre.X * UniformScale,
				-MeshCentre.Y * UniformScale,
				ConfiguredPlantBaseHeight -
					MeshBounds.Min.Z * UniformScale));
			FoliageTop = ConfiguredPlantBaseHeight + DesiredHeight;
			if (bElementalDead)
			{
				FoliageMesh->SetVectorParameterValueOnMaterials(
					TEXT("Color"),
					FVector(0.03f, 0.03f, 0.03f));
			}
		}
		else
		{
			static UStaticMesh* FallbackFoliageMesh =
				LoadObject<UStaticMesh>(
					nullptr,
					TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			if (FallbackFoliageMesh &&
				FoliageMesh->GetStaticMesh() != FallbackFoliageMesh)
			{
				FoliageMesh->SetStaticMesh(FallbackFoliageMesh);
				FoliageMesh->EmptyOverrideMaterials();
				FoliageMaterial = nullptr;
			}
			FoliageMesh->SetRelativeLocation(FVector(
				0.0f,
				0.0f,
				ConfiguredPlantBaseHeight + LegacyStemHeight));
			FoliageMesh->SetRelativeScale3D(
				FoliageShape * LegacyFoliageScale);
			FoliageTop = bHasPlant
				? ConfiguredPlantBaseHeight + LegacyStemHeight +
					50.0f * FoliageShape.Z * LegacyFoliageScale
				: 0.0f;
			if (Definition)
			{
				if (!FoliageMaterial)
				{
					FoliageMaterial = FoliageMesh->
						CreateAndSetMaterialInstanceDynamic(0);
				}
				if (FoliageMaterial)
				{
					FoliageMaterial->SetVectorParameterValue(
						TEXT("Color"),
						bElementalDead
							? FLinearColor(
								0.03f, 0.03f, 0.03f, 1.0f)
							: Definition->MatureColor);
				}
			}
		}
	}
	if (PlantGrowthWidget)
	{
		const float WidgetHalfHeight =
			PlantGrowthWidget->GetDrawSize().Y *
			PlantGrowthWidgetScale * 0.5f;
		const float AdaptiveHeight = FoliageTop +
			PlantGrowthWidgetClearance + WidgetHalfHeight;
		PlantGrowthWidget->SetRelativeLocation(FVector(
			0.0f, 0.0f,
			FMath::Max(PlantGrowthWidgetHeight, AdaptiveHeight)));
	}
	if (StatusText)
	{
		FString EnvironmentStatus = TEXT("-");
		if (Definition &&
			Definition->Element != EBotanicusPlantElement::Normal)
		{
			EnvironmentStatus =
				IsInCompatibleGreenhouse(*Definition)
					? TEXT("SERRE COMPATIBLE")
					: TEXT("MAUVAISE SERRE - CROISSANCE BLOQUEE");
		}
		if (bElementalDead)
		{
			EnvironmentStatus = TEXT("PLANTE MORTE");
		}
		FString ActionStatus = TEXT("AUCUNE");
		if (PrimaryUseMode == EPrimaryUseMode::FillSoil &&
			SoilFillProgress > 0.0f)
		{
			ActionStatus = FString::Printf(
				TEXT("TERREAU %d%%"),
				FMath::RoundToInt(SoilFillProgress * 100.0f));
		}
		else if (PrimaryUseMode == EPrimaryUseMode::RemoveSoil)
		{
			ActionStatus = FString::Printf(
				TEXT("RETRAIT TERREAU %d%% RESTANT"),
				FMath::RoundToInt(GetSoilLevel() * 100.0f));
		}
		else if (bWateringActive)
		{
			ActionStatus = TEXT("ARROSAGE EN COURS");
		}
		else if (HarvestProgress > 0.0f)
		{
			ActionStatus = FString::Printf(
				TEXT("RECOLTE %d%%"),
				FMath::RoundToInt(HarvestProgress * 100.0f));
		}
		StatusText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"TERREAU : %s\nPLANTE : %s\nARROSAGES : %d\nEAU : %d%%\nCROISSANCE : %d%%\nENVIRONNEMENT : %s\nQUALITE ESTIMEE : %s\nACTION : %s"),
					bHasSoil ? TEXT("OUI") : TEXT("NON"),
					Definition
						? *Definition->DisplayName.ToString().ToUpper()
						: TEXT("NON"),
					WateringCount,
					FMath::RoundToInt(WaterLevel * 100.0f),
					FMath::RoundToInt(GrowthProgress * 100.0f),
					*EnvironmentStatus,
					PlantKey.IsNone()
						? TEXT("-")
						: *GetPlantQualityLabel(),
					*ActionStatus)));
		StatusText->SetTextRenderColor(
			bElementalDead
				? FColor(255, 70, 40)
				: bHasSoil
				? FColor(120, 255, 150)
				: FColor(255, 190, 80));
	}
	if (PlantGrowthWidget)
	{
		if (UBotanicusPlantGrowthWidget* Widget =
			Cast<UBotanicusPlantGrowthWidget>(PlantGrowthWidget->GetWidget()))
		{
			const FText PlantName = Definition && !Definition->DisplayName.IsEmpty()
				? Definition->DisplayName
				: FText::FromName(PlantKey);
			Widget->SetPlantState(PlantName, WaterLevel, GrowthProgress);
		}
	}
}

void ABotanicusPlantPotActor::RefreshSoilVisual()
{
	if (SoilMesh)
	{
		SoilMesh->SetVisibility(false);
	}
	if (!SoilShapeVisual)
	{
		return;
	}

	const float FillAlpha = GetSoilLevel();
	const bool bIsFilling =
		PrimaryUseMode == EPrimaryUseMode::FillSoil ||
		PrimaryUseMode == EPrimaryUseMode::RemoveSoil;
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
			FillAlpha > KINDA_SMALL_NUMBER || bIsFilling,
			GetConfiguredSquareSoilProfile());
	}
}

void ABotanicusPlantPotActor::SendInteractorMessage(
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

void ABotanicusPlantPotActor::OnRep_GrowingState()
{
	RefreshVisuals();
}
