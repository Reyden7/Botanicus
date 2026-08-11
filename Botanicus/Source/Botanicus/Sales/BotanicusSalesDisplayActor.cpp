// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sales/BotanicusSalesDisplayActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "BotanicusPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/BotanicusSalesDisplayEmptyWidget.h"
#include "UI/BotanicusSalesDisplayOccupiedWidget.h"

namespace
{
FLinearColor SalesDisplayPlantVisualColor(FName ColorTag)
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

FString SalesDisplayPlantQualityLabel(FName QualityTag)
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

void GetMatureDisplayedPlantShape(
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
}

ABotanicusSalesDisplayActor::ABotanicusSalesDisplayActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	InteractionName =
		NSLOCTEXT("BotanicusSales", "SalesDisplay", "Presentoir de vente");
	InteractionAction =
		NSLOCTEXT("BotanicusSales", "MoveDisplay", "Deplacer");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
		Mesh->SetRelativeScale3D(FVector(1.15f, 0.45f, 0.65f));
	}

	SalePotSlot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SalePotSlot"));
	SalePotSlot->SetupAttachment(SceneRoot);

	DisplayedContentRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("DisplayedPotAndPlantRoot"));
	DisplayedContentRoot->SetupAttachment(SalePotSlot);

	PlantVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("DisplayedPlant"));
	PlantVisual->SetupAttachment(DisplayedContentRoot);
	PlantVisual->SetStaticMesh(
		SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
	PlantVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
	PlantVisual->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.45f));
	PlantVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StemVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("DisplayedPlantStem"));
	StemVisual->SetupAttachment(DisplayedContentRoot);
	StemVisual->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	StemVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PotVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("DisplayedSalePot"));
	PotVisual->SetupAttachment(DisplayedContentRoot);
	PotVisual->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	PotVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 76.0f));
	PotVisual->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.2f));
	PotVisual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PotVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	PotVisual->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("SalesStatus"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(16.0f);
	StatusText->SetTextRenderColor(FColor(245, 225, 160));
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ContextActionText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("SalesContextAction"));
	ContextActionText->SetupAttachment(SceneRoot);
	ContextActionText->SetRelativeLocation(FVector(0.0f, 0.0f, 205.0f));
	ContextActionText->SetHorizontalAlignment(EHTA_Center);
	ContextActionText->SetVerticalAlignment(EVRTA_TextCenter);
	ContextActionText->SetWorldSize(18.0f);
	ContextActionText->SetTextRenderColor(FColor(80, 255, 110));
	ContextActionText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContextActionText->SetVisibility(false);

	EmptyDisplayWidget = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("EmptySalesDisplayWidget"));
	EmptyDisplayWidget->SetupAttachment(SceneRoot);
	EmptyDisplayWidget->SetWidgetSpace(EWidgetSpace::World);
	EmptyDisplayWidget->SetDrawSize(FVector2D(720.0f, 420.0f));
	EmptyDisplayWidget->SetPivot(FVector2D(0.5f, 0.5f));
	EmptyDisplayWidget->SetRelativeLocation(
		FVector(0.0f, 0.0f, EmptyDisplayWidgetHeight));
	EmptyDisplayWidget->SetRelativeScale3D(FVector(0.18f));
	EmptyDisplayWidget->SetTintColorAndOpacity(
		FLinearColor(
			EmptyDisplayWidgetBrightness,
			EmptyDisplayWidgetBrightness,
			EmptyDisplayWidgetBrightness,
			1.0f));
	EmptyDisplayWidget->SetTwoSided(true);
	EmptyDisplayWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EmptyDisplayWidget->SetWidgetClass(
		UBotanicusSalesDisplayEmptyWidget::StaticClass());
	EmptyDisplayWidget->SetVisibility(false);

	OccupiedDisplayWidget = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("OccupiedSalesDisplayWidget"));
	OccupiedDisplayWidget->SetupAttachment(SceneRoot);
	OccupiedDisplayWidget->SetWidgetSpace(EWidgetSpace::World);
	OccupiedDisplayWidget->SetDrawSize(FVector2D(620.0f, 220.0f));
	OccupiedDisplayWidget->SetPivot(FVector2D(0.5f, 0.5f));
	OccupiedDisplayWidget->SetRelativeLocation(
		FVector(0.0f, 0.0f, OccupiedDisplayWidgetHeight));
	OccupiedDisplayWidget->SetRelativeScale3D(
		FVector(OccupiedDisplayWidgetScale));
	OccupiedDisplayWidget->SetTintColorAndOpacity(FLinearColor(
		OccupiedDisplayWidgetBrightness, OccupiedDisplayWidgetBrightness,
		OccupiedDisplayWidgetBrightness, 1.0f));
	OccupiedDisplayWidget->SetTwoSided(true);
	OccupiedDisplayWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OccupiedDisplayWidget->SetWidgetClass(
		UBotanicusSalesDisplayOccupiedWidget::StaticClass());
	OccupiedDisplayWidget->SetVisibility(false);

	RefreshVisuals();
}

void ABotanicusSalesDisplayActor::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshSalePotSlotTransform();
	if (EmptyDisplayWidget)
	{
		EmptyDisplayWidget->SetRelativeLocation(
			FVector(0.0f, 0.0f, EmptyDisplayWidgetHeight));
		EmptyDisplayWidget->SetTintColorAndOpacity(
			FLinearColor(
				EmptyDisplayWidgetBrightness,
				EmptyDisplayWidgetBrightness,
				EmptyDisplayWidgetBrightness,
				1.0f));
	}
	if (OccupiedDisplayWidget)
	{
		OccupiedDisplayWidget->SetRelativeLocation(
			FVector(0.0f, 0.0f, OccupiedDisplayWidgetHeight));
		OccupiedDisplayWidget->SetRelativeScale3D(
			FVector(OccupiedDisplayWidgetScale));
		OccupiedDisplayWidget->SetTintColorAndOpacity(FLinearColor(
			OccupiedDisplayWidgetBrightness, OccupiedDisplayWidgetBrightness,
			OccupiedDisplayWidgetBrightness, 1.0f));
	}
	RefreshVisuals();
}

#if WITH_EDITOR
void ABotanicusSalesDisplayActor::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshSalePotSlotTransform();
	RefreshVisuals();
}
#endif

void ABotanicusSalesDisplayActor::BeginPlay()
{
	Super::BeginPlay();
	if (EmptyDisplayWidget)
	{
		if (UClass* WidgetClass = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/Botanicus/UI/Sales/WBP_SalesDisplayEmpty.WBP_SalesDisplayEmpty_C")))
		{
			EmptyDisplayWidget->SetWidgetClass(WidgetClass);
		}
		EmptyDisplayWidget->InitWidget();
	}
	if (OccupiedDisplayWidget)
	{
		if (UClass* WidgetClass = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/Botanicus/UI/Sales/WBP_SalesDisplayOccupied.WBP_SalesDisplayOccupied_C")))
		{
			OccupiedDisplayWidget->SetWidgetClass(WidgetClass);
		}
		OccupiedDisplayWidget->InitWidget();
	}
}

void ABotanicusSalesDisplayActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	const APlayerCameraManager* CameraManager =
		Controller ? Controller->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector CameraLocation =
			CameraManager->GetCameraLocation();
		if (StatusText)
		{
			StatusText->SetWorldRotation(
				(CameraLocation -
				 StatusText->GetComponentLocation()).Rotation());
		}
		if (ContextActionText)
		{
			ContextActionText->SetWorldRotation(
				(CameraLocation -
				 ContextActionText->GetComponentLocation()).Rotation());
		}
		if (EmptyDisplayWidget)
		{
			EmptyDisplayWidget->SetWorldRotation(
				(CameraLocation -
				 EmptyDisplayWidget->GetComponentLocation()).Rotation());
		}
		if (OccupiedDisplayWidget)
		{
			OccupiedDisplayWidget->SetWorldRotation(
				(CameraLocation - OccupiedDisplayWidget->GetComponentLocation()).Rotation());
		}
	}
	RefreshLocalAction();
	if (EmptyDisplayWidget)
	{
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		EmptyDisplayWidget->SetVisibility(
			IsEmpty() && Pawn &&
			!ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
			IsLocalPlayerTargetingDisplay(Pawn));
	}
	if (!DisplayedPlantItemKey.IsNone())
	{
		RefreshVisuals();
	}

	if (HasAuthority() &&
		!DisplayedPlantItemKey.IsNone() &&
		!IsValid(SellerController) &&
		World)
	{
		// A pending sale restored from disk resumes with the first active
		// nursery player instead of leaving the display permanently blocked.
		for (TActorIterator<ABotanicusPlayerController> ControllerIt(
				 World);
			 ControllerIt;
			 ++ControllerIt)
		{
			SellerController = *ControllerIt;
			SaleEndServerTime = 0.0f;
			ForceNetUpdate();
			break;
		}
	}

}

void ABotanicusSalesDisplayActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusSalesDisplayActor,
		DisplayedPlantItemKey);
	DOREPLIFETIME(
		ABotanicusSalesDisplayActor,
		DisplayedSoilItemKey);
	DOREPLIFETIME(
		ABotanicusSalesDisplayActor,
		DisplayedPotItemKey);
	DOREPLIFETIME(
		ABotanicusSalesDisplayActor,
		SaleEndServerTime);
	DOREPLIFETIME(
		ABotanicusSalesDisplayActor,
		bVisitorEnRoute);
	DOREPLIFETIME(
		ABotanicusSalesDisplayActor,
		bPlantTakenByVisitor);
}

FBotanicusInteractionPrompt
ABotanicusSalesDisplayActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	Prompt.ActionText = DisplayedPlantItemKey.IsNone()
		? NSLOCTEXT(
			"BotanicusSales",
			"NeedsPreparedSalePot",
			"Clic gauche : placer le pot de vente prepare")
		: NSLOCTEXT(
			"BotanicusSales",
			"DisplayOccupied",
			"Presentoir occupe");
	Prompt.bCanInteract = false;
	return Prompt;
}

bool ABotanicusSalesDisplayActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return false;
}

void ABotanicusSalesDisplayActor::Interact_Implementation(
	AActor* Interactor)
{
	if (HasAuthority())
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"E deplace le presentoir. Le clic gauche expose la plante."));
	}
}

void ABotanicusSalesDisplayActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (PlantVisual)
	{
		PlantVisual->SetVisibility(false);
	}
	if (PotVisual)
	{
		PotVisual->SetVisibility(false);
	}
	if (StatusText)
	{
		StatusText->SetVisibility(false);
	}
	if (ContextActionText)
	{
		ContextActionText->SetVisibility(false);
	}
	if (EmptyDisplayWidget)
	{
		EmptyDisplayWidget->SetVisibility(false);
	}
}

void ABotanicusSalesDisplayActor::TryPlaceSelectedPlant(
	AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}
	SendInteractorMessage(
		Interactor,
		TEXT(
			"Preparez la plante dans un pot de vente, puis maintenez E pour poser ce pot sur le presentoir."));
}

bool ABotanicusSalesDisplayActor::TryMountSalePot(
	ABotanicusSalePotActor* SalePot,
	ABotanicusPlayerController* Seller)
{
	if (!HasAuthority() || !IsEmpty() || !IsValid(SalePot) ||
		!SalePot->IsReadyForSale() || !IsValid(Seller))
	{
		return false;
	}
	if (!TryMountSalePotState(
			SalePot->GetSoilItemKey(),
			SalePot->GetPlantItemKey(),
			Seller,
			SalePot->GetItemKey()))
	{
		return false;
	}
	SalePot->Destroy();
	return true;
}

bool ABotanicusSalesDisplayActor::TryMountSalePotState(
	FName SoilItemKey,
	FName PlantItemKey,
	ABotanicusPlayerController* Seller,
	FName SalePotItemKey)
{
	if (!HasAuthority() || !IsEmpty() || SoilItemKey.IsNone() ||
		PlantItemKey.IsNone() || !IsValid(Seller))
	{
		return false;
	}
	DisplayedPlantItemKey = PlantItemKey;
	DisplayedSoilItemKey = SoilItemKey;
	DisplayedPotItemKey = SalePotItemKey.IsNone()
		? FName(TEXT("SalePot"))
		: SalePotItemKey;
	SellerController = Seller;
	SaleEndServerTime = 0.0f;
	bVisitorEnRoute = false;
	bPlantTakenByVisitor = false;
	RefreshVisuals();
	ForceNetUpdate();
	Seller->ClientMessage(
		TEXT(
			"Pot installé : un visiteur va venir voir cette plante."));
	return true;
}

bool ABotanicusSalesDisplayActor::CompleteVisitorPurchase(
	ABotanicusVisitorCharacter* Visitor)
{
	if (!HasAuthority() ||
		Visitor != ActiveVisitor ||
		DisplayedPlantItemKey.IsNone() ||
		!bPlantTakenByVisitor ||
		!IsValid(SellerController))
	{
		return false;
	}

	const FBotanicusItemDefinition* Definition =
		GetDisplayedPlantDefinition();
	const FName SoldPlant = DisplayedPlantItemKey;
	const ABotanicusGameState* BotanicusGameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const int32 SalePrice =
		Definition
			? BotanicusGameState
				? BotanicusGameState->GetTrendAdjustedSalePrice(
					*Definition)
				: FMath::Max(0, Definition->SalePrice)
			: 0;
	SellerController->CreditPlantSale(SoldPlant, SalePrice);
	DisplayedPlantItemKey = NAME_None;
	DisplayedSoilItemKey = NAME_None;
	DisplayedPotItemKey = TEXT("SalePot");
	SaleEndServerTime = 0.0f;
	bVisitorEnRoute = false;
	bPlantTakenByVisitor = false;
	SellerController = nullptr;
	RefreshVisuals();
	ForceNetUpdate();
	Visitor->BeginDeparture(true);
	return true;
}

bool ABotanicusSalesDisplayActor::TryReserveForVisitor(
	ABotanicusVisitorCharacter* Visitor)
{
	if (!HasAuthority() ||
		!IsValid(Visitor) ||
		DisplayedPlantItemKey.IsNone() ||
		bPlantTakenByVisitor ||
		!IsValid(SellerController) ||
		IsValid(ActiveVisitor))
	{
		return false;
	}

	ActiveVisitor = Visitor;
	bVisitorEnRoute = true;
	RefreshVisuals();
	ForceNetUpdate();
	return true;
}

bool ABotanicusSalesDisplayActor::IsAvailableForVisitorBrowsing(
	const ABotanicusVisitorCharacter* Visitor) const
{
	return !DisplayedPlantItemKey.IsNone() &&
		!bPlantTakenByVisitor &&
		(!IsValid(ActiveVisitor.Get()) ||
			ActiveVisitor.Get() == Visitor);
}

bool ABotanicusSalesDisplayActor::TakeReservedPlantForVisitor(
	ABotanicusVisitorCharacter* Visitor)
{
	if (!HasAuthority() ||
		Visitor != ActiveVisitor ||
		DisplayedPlantItemKey.IsNone() ||
		bPlantTakenByVisitor)
	{
		return false;
	}
	bPlantTakenByVisitor = true;
	RefreshVisuals();
	ForceNetUpdate();
	return true;
}

void ABotanicusSalesDisplayActor::NotifyVisitorEnded(
	ABotanicusVisitorCharacter* Visitor)
{
	if (!HasAuthority() || Visitor != ActiveVisitor)
	{
		return;
	}
	ActiveVisitor = nullptr;
	bVisitorEnRoute = false;
	bPlantTakenByVisitor = false;
	RefreshVisuals();
	ForceNetUpdate();
}

FTransform ABotanicusSalesDisplayActor::
	GetSalePotPlacementTransform() const
{
	if (SalePotSlot)
	{
		return SalePotSlot->GetComponentTransform();
	}
	const FVector SlotOffset = GetConfiguredDisplayedContentOffset();
	return FTransform(
		GetActorRotation(),
		GetActorTransform().TransformPosition(
			FVector(
				SlotOffset.X,
				SlotOffset.Y,
				GetDisplaySurfaceHeight() +
					SlotOffset.Z)));
}

FVector ABotanicusSalesDisplayActor::
	GetConfiguredDisplayedContentOffset() const
{
	return FVector(
		SalePotSlotPosition.X,
		SalePotSlotPosition.Y,
		SalePotSlotHeight);
}

float ABotanicusSalesDisplayActor::GetDisplaySurfaceHeight() const
{
	if (Mesh && Mesh->GetStaticMesh())
	{
		const FBoxSphereBounds FurnitureBounds =
			Mesh->CalcBounds(Mesh->GetRelativeTransform());
		return FurnitureBounds.Origin.Z +
			FurnitureBounds.BoxExtent.Z + DisplayedPotSurfaceOffset;
	}
	return 32.5f + DisplayedPotSurfaceOffset;
}

void ABotanicusSalesDisplayActor::RefreshSalePotSlotTransform()
{
	const FVector SlotOffset = GetConfiguredDisplayedContentOffset();
	if (SalePotSlot)
	{
		SalePotSlot->SetRelativeLocation(FVector(
			SlotOffset.X,
			SlotOffset.Y,
			GetDisplaySurfaceHeight() + SlotOffset.Z));
	}
	if (DisplayedContentRoot)
	{
		if (SalePotSlot &&
			DisplayedContentRoot->GetAttachParent() != SalePotSlot)
		{
			DisplayedContentRoot->AttachToComponent(
				SalePotSlot,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		DisplayedContentRoot->SetRelativeLocation(FVector::ZeroVector);
	}
}

bool ABotanicusSalesDisplayActor::
	CanRetrieveDisplayedSalePot() const
{
	return !DisplayedPlantItemKey.IsNone() &&
		!bPlantTakenByVisitor;
}

bool ABotanicusSalesDisplayActor::
	IsDisplayedSalePotTargeted(const AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Controller || !World || !PotVisual ||
		!CanRetrieveDisplayedSalePot() ||
		FVector::DistSquared(
			Pawn->GetActorLocation(),
			PotVisual->GetComponentLocation()) > FMath::Square(450.0f))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ToPot =
		PotVisual->Bounds.Origin - ViewLocation;
	const float Distance = ToPot.Size();
	if (Distance <= KINDA_SMALL_NUMBER ||
		FVector::DotProduct(
			ViewRotation.Vector(),
			ToPot / Distance) <
			FMath::Cos(FMath::DegreesToRadians(22.0f)))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusDisplayedSalePotTarget),
		false,
		Pawn);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			PotVisual->Bounds.Origin,
			ECC_Visibility,
			QueryParams) &&
		Hit.GetActor() == this &&
		Hit.GetComponent() == PotVisual;
}

void ABotanicusSalesDisplayActor::
	SetDisplayedSalePotHighlighted(
		bool bHighlighted,
		UMaterialInterface* HighlightMaterial)
{
	for (UStaticMeshComponent* Visual :
		 {PotVisual.Get(), StemVisual.Get(), PlantVisual.Get()})
	{
		if (!Visual)
		{
			continue;
		}
		Visual->SetRenderCustomDepth(bHighlighted);
		Visual->SetCustomDepthStencilValue(bHighlighted ? 2 : 0);
		Visual->SetOverlayMaterial(
			bHighlighted ? HighlightMaterial : nullptr);
	}
}

bool ABotanicusSalesDisplayActor::TryRetrieveDisplayedSalePot(
	AActor* Interactor)
{
	if (!HasAuthority() ||
		!CanRetrieveDisplayedSalePot() ||
		!IsDisplayedSalePotTargeted(Interactor))
	{
		return false;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!QuickBar)
	{
		return false;
	}

	FBotanicusCarriedItemState State;
	State.bHasSalePotState = true;
	State.SaleSoilItemKey = DisplayedSoilItemKey.IsNone()
		? FName(TEXT("PottingSoil"))
		: DisplayedSoilItemKey;
	State.SalePlantItemKey = DisplayedPlantItemKey;
	int32 AddedSlotIndex = INDEX_NONE;
	if (!QuickBar->AddUniqueItem(
			DisplayedPotItemKey.IsNone()
				? FName(TEXT("SalePot"))
				: DisplayedPotItemKey,
			State,
			AddedSlotIndex))
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Hotbar pleine : liberez un emplacement pour reprendre le pot."));
		return false;
	}
	if (ABotanicusVisitorCharacter* ReservedVisitor =
			ActiveVisitor.Get())
	{
		// The pot is still physically on the display, so the owner may take
		// it back.  Release the visitor cleanly before clearing the display.
		ReservedVisitor->BeginDeparture(false);
	}

	DisplayedPlantItemKey = NAME_None;
	DisplayedSoilItemKey = NAME_None;
	DisplayedPotItemKey = TEXT("SalePot");
	SellerController = nullptr;
	SaleEndServerTime = 0.0f;
	bVisitorEnRoute = false;
	bPlantTakenByVisitor = false;
	QuickBar->SelectSlotAuthoritative(AddedSlotIndex);
	RefreshVisuals();
	ForceNetUpdate();
	SendInteractorMessage(
		Interactor,
		TEXT("Pot de vente repris et place dans la hotbar."));
	return true;
}

void ABotanicusSalesDisplayActor::RestoreDisplayedPlant(
	FName InDisplayedPlantItemKey,
	FName InDisplayedSoilItemKey,
	FName InDisplayedPotItemKey)
{
	if (!HasAuthority())
	{
		return;
	}
	DisplayedPlantItemKey = InDisplayedPlantItemKey;
	DisplayedSoilItemKey = DisplayedPlantItemKey.IsNone()
		? NAME_None
		: InDisplayedSoilItemKey.IsNone()
			? FName(TEXT("PottingSoil"))
			: InDisplayedSoilItemKey;
	DisplayedPotItemKey = InDisplayedPotItemKey.IsNone()
		? FName(TEXT("SalePot"))
		: InDisplayedPotItemKey;
	SaleEndServerTime = 0.0f;
	bVisitorEnRoute = false;
	bPlantTakenByVisitor = false;
	SellerController = nullptr;
	RefreshVisuals();
	ForceNetUpdate();
}

FName ABotanicusSalesDisplayActor::GetSelectedItemKey(
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

const FBotanicusItemDefinition*
ABotanicusSalesDisplayActor::GetSelectedPlantDefinition(
	AActor* Interactor) const
{
	const FName SelectedItemKey = GetSelectedItemKey(Interactor);
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(SelectedItemKey) : nullptr;
	return Definition && Definition->bWholePlant
		? Definition
		: nullptr;
}

const FBotanicusItemDefinition*
ABotanicusSalesDisplayActor::GetDisplayedPlantDefinition() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	return Catalog
		? Catalog->FindItem(DisplayedPlantItemKey)
		: nullptr;
}

bool ABotanicusSalesDisplayActor::IsLocalPlayerTargetingDisplay(
	AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Controller || !World)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FVector TargetOrigin;
	FVector TargetExtent;
	GetActorBounds(true, TargetOrigin, TargetExtent);
	const FVector ToTarget = TargetOrigin - ViewLocation;
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER ||
		Distance > 450.0f ||
		FVector::DotProduct(
			ViewRotation.Vector(),
			ToTarget / Distance) <
			FMath::Cos(FMath::DegreesToRadians(22.0f)))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusSalesDisplayLook),
		false);
	QueryParams.AddIgnoredActor(Pawn);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			TargetOrigin,
			ECC_Visibility,
			QueryParams) &&
		Hit.GetActor() == this;
}

void ABotanicusSalesDisplayActor::RefreshVisuals()
{
	RefreshSalePotSlotTransform();
	const FBotanicusItemDefinition* Definition =
		GetDisplayedPlantDefinition();
	const bool bHasPlant =
		!DisplayedPlantItemKey.IsNone() && Definition;
	const UGameInstance* PlantGameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		PlantGameInstance
			? PlantGameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* PlantDefinition =
		Plants
			? Plants->FindPlantByHarvestItem(DisplayedPlantItemKey)
			: nullptr;
	UStaticMesh* MaturePlantMesh =
		PlantDefinition && !PlantDefinition->MatureGrowthMesh.IsNull()
			? PlantDefinition->MatureGrowthMesh.LoadSynchronous()
			: nullptr;
	float MaturePlantUniformScale = 1.0f;
	FVector MaturePlantHorizontalOffset = FVector::ZeroVector;
	float MaturePlantHeight = 0.0f;
	if (PlantVisual)
	{
		PlantVisual->SetVisibility(
			bHasPlant && !bPlantTakenByVisitor);
		float StemHeight = 80.0f;
		FVector FoliageShape;
		GetMatureDisplayedPlantShape(
			DisplayedPlantItemKey, StemHeight, FoliageShape);
		if (MaturePlantMesh)
		{
			if (PlantVisual->GetStaticMesh() != MaturePlantMesh)
			{
				PlantVisual->SetStaticMesh(MaturePlantMesh);
				PlantVisual->EmptyOverrideMaterials();
				PlantMaterial = nullptr;
			}
			const FBox MeshBounds = MaturePlantMesh->GetBoundingBox();
			MaturePlantHeight = FMath::Max(
				0.1f, PlantDefinition->MatureGrowthVisualHeight) *
				DisplayedPlantHeightMultiplier;
			MaturePlantUniformScale = MaturePlantHeight /
				FMath::Max(0.01f, MeshBounds.GetSize().Z);
			const FVector MeshCentre = MeshBounds.GetCenter();
			MaturePlantHorizontalOffset = FVector(
				-MeshCentre.X * MaturePlantUniformScale,
				-MeshCentre.Y * MaturePlantUniformScale,
				-MeshBounds.Min.Z * MaturePlantUniformScale);
			PlantVisual->SetRelativeScale3D(
				FVector(MaturePlantUniformScale));
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
			const FVector PlantScale = FoliageShape * 0.32f;
			PlantVisual->SetRelativeScale3D(PlantScale);
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
						SalesDisplayPlantVisualColor(
							Definition->PlantColorTag));
				}
			}
		}
	}
	float DisplayedPlantBaseHeight = 86.0f;
	if (PotVisual)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusItemCatalogSubsystem* Catalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusItemCatalogSubsystem>()
				: nullptr;
		const FBotanicusItemDefinition* PotDefinition =
			Catalog ? Catalog->FindItem(DisplayedPotItemKey) : nullptr;
		UStaticMesh* DisplayedPotMesh = nullptr;
		FVector DisplayedPotScale = FVector::OneVector;
		FRotator DisplayedPotRotation = FRotator::ZeroRotator;
		const UStaticMeshComponent* AuthoredPotComponent = nullptr;
		if (PotDefinition)
		{
			if (UClass* PotActorClass =
					PotDefinition->WorldActorClass.LoadSynchronous())
			{
				if (const ABotanicusInteractableActor* PotDefaults =
						Cast<ABotanicusInteractableActor>(
							PotActorClass->GetDefaultObject()))
				{
					AuthoredPotComponent = PotDefaults->Mesh;
				}
			}
			if (AuthoredPotComponent &&
				AuthoredPotComponent->GetStaticMesh())
			{
				DisplayedPotMesh = AuthoredPotComponent->GetStaticMesh();
				DisplayedPotScale =
					AuthoredPotComponent->GetRelativeScale3D();
				DisplayedPotRotation =
					AuthoredPotComponent->GetRelativeRotation();
			}
			else
			{
				DisplayedPotMesh =
					PotDefinition->WorldMesh.LoadSynchronous();
				DisplayedPotScale = PotDefinition->WorldScale;
			}
		}
		if (DisplayedPotMesh)
		{
			PotVisual->SetStaticMesh(DisplayedPotMesh);
			PotVisual->SetRelativeRotation(DisplayedPotRotation);
			PotVisual->SetRelativeScale3D(DisplayedPotScale);
			PotVisual->EmptyOverrideMaterials();
			if (AuthoredPotComponent)
			{
				for (int32 MaterialIndex = 0;
					 MaterialIndex < AuthoredPotComponent->GetNumMaterials();
					 ++MaterialIndex)
				{
					PotVisual->SetMaterial(
						MaterialIndex,
						AuthoredPotComponent->GetMaterial(MaterialIndex));
				}
			}

			const FBox AuthoredBounds =
				DisplayedPotMesh->GetBoundingBox().TransformBy(FTransform(
					DisplayedPotRotation,
					FVector::ZeroVector,
					DisplayedPotScale));
			PotVisual->SetRelativeLocation(FVector(
				0.0f,
				0.0f,
				-AuthoredBounds.Min.Z));
			DisplayedPlantBaseHeight =
				AuthoredBounds.GetSize().Z * 0.82f;
		}
		else if (UStaticMesh* DefaultPotMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			PotVisual->SetStaticMesh(DefaultPotMesh);
			PotVisual->SetRelativeLocation(FVector(
				0.0f,
				0.0f,
				10.0f));
			PotVisual->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.2f));
			DisplayedPlantBaseHeight = 17.0f;
		}
		PotVisual->SetVisibility(
			bHasPlant && !bPlantTakenByVisitor);
	}
	float StemHeight = 80.0f;
	FVector FoliageShape;
	GetMatureDisplayedPlantShape(
		DisplayedPlantItemKey, StemHeight, FoliageShape);
	StemHeight *= DisplayedPlantHeightMultiplier;
	if (StemVisual)
	{
		StemVisual->SetVisibility(
			bHasPlant && !bPlantTakenByVisitor &&
			MaturePlantMesh == nullptr);
		StemVisual->SetRelativeLocation(FVector(
			0.0f,
			0.0f,
			DisplayedPlantBaseHeight + StemHeight * 0.5f));
		StemVisual->SetRelativeScale3D(FVector(
			0.035f, 0.035f, StemHeight / 100.0f));
	}
	if (PlantVisual)
	{
		PlantVisual->SetRelativeLocation(
			MaturePlantMesh
				? FVector(
					MaturePlantHorizontalOffset.X,
					MaturePlantHorizontalOffset.Y,
					DisplayedPlantBaseHeight +
						MaturePlantHorizontalOffset.Z)
				: FVector(
					0.0f,
					0.0f,
					DisplayedPlantBaseHeight + StemHeight));
	}
	if (OccupiedDisplayWidget && bHasPlant)
	{
		const float SlotHeight = SalePotSlot
			? SalePotSlot->GetRelativeLocation().Z
			: GetDisplaySurfaceHeight();
		const float FoliageTop = MaturePlantMesh
			? SlotHeight + DisplayedPlantBaseHeight + MaturePlantHeight
			: SlotHeight + DisplayedPlantBaseHeight + StemHeight +
				50.0f * FoliageShape.Z * 0.32f;
		OccupiedDisplayWidget->SetRelativeLocation(FVector(
			0.0f,
			0.0f,
			FMath::Max(
				OccupiedDisplayWidgetHeight,
				FoliageTop + 28.0f)));
	}
	if (!StatusText)
	{
		return;
	}

	// The illustrated widget replaces the old multiline 3D text entirely.
	StatusText->SetVisibility(false);
	if (OccupiedDisplayWidget)
	{
		OccupiedDisplayWidget->SetVisibility(
			bHasPlant && !bPlantTakenByVisitor &&
			!ActorHasTag(TEXT("BotanicusPlacementPreview")));
	}
	if (!bHasPlant)
	{
		return;
	}

	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const int32 DisplayedPrice =
		GameState
			? GameState->GetTrendAdjustedSalePrice(*Definition)
			: FMath::Max(0, Definition->SalePrice);
	if (OccupiedDisplayWidget)
	{
		if (UBotanicusSalesDisplayOccupiedWidget* Widget =
			Cast<UBotanicusSalesDisplayOccupiedWidget>(
				OccupiedDisplayWidget->GetUserWidgetObject()))
		{
			FText PlantDisplayName = Definition->DisplayName;
			FString BasePlantKey = DisplayedPlantItemKey.ToString();
			if (BasePlantKey.RemoveFromEnd(TEXT("_Beautiful")) ||
				BasePlantKey.RemoveFromEnd(TEXT("_Exceptional")))
			{
				const UGameInstance* GameInstance = GetGameInstance();
				const UBotanicusItemCatalogSubsystem* ItemCatalog =
					GameInstance
						? GameInstance->GetSubsystem<
							UBotanicusItemCatalogSubsystem>()
						: nullptr;
				if (const FBotanicusItemDefinition* BaseDefinition =
					ItemCatalog
						? ItemCatalog->FindItem(FName(*BasePlantKey))
						: nullptr)
				{
					PlantDisplayName = BaseDefinition->DisplayName;
				}
			}
			Widget->SetSaleData(
				PlantDisplayName,
				FText::FromString(SalesDisplayPlantQualityLabel(
					Definition->PlantQualityTag)),
				DisplayedPrice);
		}
	}
}

void ABotanicusSalesDisplayActor::RefreshLocalAction()
{
	if (!ContextActionText ||
		ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		return;
	}
	const UWorld* World = GetWorld();
	const APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	ContextActionText->SetVisibility(false);
}

void ABotanicusSalesDisplayActor::SendInteractorMessage(
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

void ABotanicusSalesDisplayActor::OnRep_DisplayedPlant()
{
	RefreshVisuals();
}
