// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sales/BotanicusCashRegisterActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BotanicusGameState.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusCashRegisterActor::ABotanicusCashRegisterActor()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractionName =
		NSLOCTEXT("BotanicusSales", "CashRegister", "Caisse");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube =
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	Mesh->SetStaticMesh(Cube);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f));
	Mesh->SetRelativeScale3D(FVector(0.75f, 0.42f, 0.48f));

	CounterVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Checkout Counter"));
	CounterVisual->SetupAttachment(SceneRoot);
	CounterVisual->SetStaticMesh(Cube);
	CounterVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f));
	CounterVisual->SetRelativeScale3D(FVector(0.75f, 0.42f, 0.48f));
	CounterVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RegisterVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Cash Register"));
	RegisterVisual->SetupAttachment(SceneRoot);
	RegisterVisual->SetStaticMesh(Cube);
	RegisterVisual->SetRelativeLocation(FVector(-8.0f, 0.0f, 105.0f));
	RegisterVisual->SetRelativeScale3D(FVector(0.28f, 0.24f, 0.14f));
	RegisterVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ScreenVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Register Screen"));
	ScreenVisual->SetupAttachment(SceneRoot);
	ScreenVisual->SetStaticMesh(Cube);
	ScreenVisual->SetRelativeLocation(FVector(20.0f, 0.0f, 119.0f));
	ScreenVisual->SetRelativeScale3D(FVector(0.20f, 0.08f, 0.16f));
	ScreenVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Checkout Status"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(16.0f);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ContextActionText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Checkout Action"));
	ContextActionText->SetupAttachment(SceneRoot);
	ContextActionText->SetRelativeLocation(FVector(0.0f, 0.0f, 185.0f));
	ContextActionText->SetHorizontalAlignment(EHTA_Center);
	ContextActionText->SetVerticalAlignment(EVRTA_TextCenter);
	ContextActionText->SetWorldSize(18.0f);
	ContextActionText->SetTextRenderColor(FColor(70, 255, 110));
	ContextActionText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContextActionText->SetVisibility(false);

	for (int32 SlotIndex = 0;
		 SlotIndex < SelfCheckoutSlotCount;
		 ++SlotIndex)
	{
		UStaticMeshComponent* SlotVisual =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(
					TEXT("Self Checkout Slot %d"),
					SlotIndex + 1));
		SlotVisual->SetupAttachment(SceneRoot);
		SlotVisual->SetStaticMesh(Cube);
		const FTransform SlotTransform =
			GetSelfCheckoutSlotTransform(SlotIndex);
		SlotVisual->SetRelativeLocation(
			GetActorTransform().InverseTransformPosition(
				SlotTransform.GetLocation()) +
			FVector(0.0f, 0.0f, 2.0f));
		SlotVisual->SetRelativeRotation(FRotator::ZeroRotator);
		SlotVisual->SetRelativeScale3D(
			FVector(0.60f, 0.40f, 0.04f));
		SlotVisual->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
		SelfCheckoutSlotVisuals.Add(SlotVisual);
	}
	SelfCheckoutZoneLabel =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("Self Checkout Zone Label"));
	SelfCheckoutZoneLabel->SetupAttachment(SceneRoot);
	SelfCheckoutZoneLabel->SetRelativeLocation(
		FVector(0.0f, 0.0f, 215.0f));
	SelfCheckoutZoneLabel->SetHorizontalAlignment(EHTA_Center);
	SelfCheckoutZoneLabel->SetVerticalAlignment(EVRTA_TextCenter);
	SelfCheckoutZoneLabel->SetWorldSize(15.0f);
	SelfCheckoutZoneLabel->SetTextRenderColor(
		FColor(70, 160, 255));
	SelfCheckoutZoneLabel->SetText(
		FText::FromString(
			TEXT("ZONE CAISSES AUTOMATIQUES\n6 EMPLACEMENTS")));
	SelfCheckoutZoneLabel->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	SelfCheckoutZoneLabel->SetVisibility(false);

	RefreshVisuals();
}

void ABotanicusCashRegisterActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && GetItemKey().IsNone())
	{
		InitializePlacedItem(TEXT("CashRegister"), 1);
	}
	InitializeVisualMaterials();
}

void ABotanicusCashRegisterActor::InitializeVisualMaterials()
{
	// Dynamic material instances are runtime objects. Creating them in the
	// native constructor makes child Blueprint defaults reference transient
	// objects and prevents those Blueprints from being saved.
	if (ScreenVisual)
	{
		if (UMaterialInstanceDynamic* ScreenMaterial =
				ScreenVisual->CreateAndSetMaterialInstanceDynamic(0))
		{
			ScreenMaterial->SetVectorParameterValue(
				TEXT("Color"),
				FLinearColor(0.03f, 0.65f, 0.30f, 1.0f));
		}
	}

	for (UStaticMeshComponent* SlotVisual : SelfCheckoutSlotVisuals)
	{
		if (SlotVisual)
		{
			if (UMaterialInstanceDynamic* SlotMaterial =
					SlotVisual->CreateAndSetMaterialInstanceDynamic(0))
			{
				SlotMaterial->SetVectorParameterValue(
					TEXT("Color"),
					FLinearColor(0.08f, 0.42f, 0.95f, 1.0f));
			}
		}
	}
}

void ABotanicusCashRegisterActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshVisuals();
}

void ABotanicusCashRegisterActor::ConfigureAsLocalPreview(bool bIsValid)
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
	for (UStaticMeshComponent* SlotVisual :
		 SelfCheckoutSlotVisuals)
	{
		if (SlotVisual)
		{
			SlotVisual->SetVisibility(false);
		}
	}
	if (SelfCheckoutZoneLabel)
	{
		SelfCheckoutZoneLabel->SetVisibility(false);
	}
}

FVector ABotanicusCashRegisterActor::GetCustomerStandLocation() const
{
	return GetActorLocation() +
		GetActorForwardVector() * 155.0f +
		FVector(0.0f, 0.0f, 84.0f);
}

FTransform ABotanicusCashRegisterActor::
	GetSelfCheckoutSlotTransform(int32 SlotIndex) const
{
	const int32 ClampedIndex =
		FMath::Clamp(
			SlotIndex,
			0,
			SelfCheckoutSlotCount - 1);
	const int32 Side = ClampedIndex < 3 ? -1 : 1;
	const int32 SideIndex =
		ClampedIndex < 3 ? 2 - ClampedIndex : ClampedIndex - 3;
	const float LateralDistance =
		95.0f + SideIndex * 80.0f;
	return FTransform(
		GetActorRotation(),
		GetActorLocation() +
			GetActorRightVector() *
				(Side * LateralDistance));
}

int32 ABotanicusCashRegisterActor::FindSelfCheckoutSlotIndex(
	const FVector& WorldLocation,
	float Tolerance) const
{
	for (int32 SlotIndex = 0;
		 SlotIndex < SelfCheckoutSlotCount;
		 ++SlotIndex)
	{
		if (FVector::DistSquared2D(
				WorldLocation,
				GetSelfCheckoutSlotTransform(SlotIndex).
					GetLocation()) <=
			FMath::Square(FMath::Max(1.0f, Tolerance)))
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

bool ABotanicusCashRegisterActor::
	FindClosestAvailableSelfCheckoutSlot(
		const FVector& RequestedLocation,
		FTransform& OutTransform,
		const AActor* IgnoredSelfCheckout) const
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World
			? World->GetGameState<ABotanicusGameState>()
			: nullptr;
	if (!World || !GameState ||
		GameState->GetMainShopLevel() < 3)
	{
		return false;
	}

	float BestDistanceSquared = TNumericLimits<float>::Max();
	bool bFound = false;
	for (int32 SlotIndex = 0;
		 SlotIndex < SelfCheckoutSlotCount;
		 ++SlotIndex)
	{
		const FTransform SlotTransform =
			GetSelfCheckoutSlotTransform(SlotIndex);
		bool bOccupied = false;
		for (TActorIterator<ABotanicusSelfCheckoutActor>
				 CheckoutIt(World);
			 CheckoutIt;
			 ++CheckoutIt)
		{
			if (*CheckoutIt == IgnoredSelfCheckout ||
				CheckoutIt->ActorHasTag(
					TEXT("BotanicusPlacementPreview")))
			{
				continue;
			}
			if (FVector::DistSquared2D(
					CheckoutIt->GetActorLocation(),
					SlotTransform.GetLocation()) <=
				FMath::Square(35.0f))
			{
				bOccupied = true;
				break;
			}
		}
		if (bOccupied)
		{
			continue;
		}
		const float DistanceSquared =
			FVector::DistSquared2D(
				RequestedLocation,
				SlotTransform.GetLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			OutTransform = SlotTransform;
			bFound = true;
		}
	}
	return bFound;
}

ABotanicusVisitorCharacter*
ABotanicusCashRegisterActor::GetCheckoutCustomer() const
{
	UWorld* World = GetWorld();
	if (!World || !IsInsideCheckoutZone())
	{
		return nullptr;
	}

	ABotanicusVisitorCharacter* Customer = nullptr;
	float BestDistanceSquared = FMath::Square(260.0f);
	const FVector StandLocation = GetCustomerStandLocation();
	for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(World);
		 VisitorIt;
		 ++VisitorIt)
	{
		if (!VisitorIt->CanUsePlayerCheckout())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(
			VisitorIt->GetActorLocation(),
			StandLocation);
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Customer = *VisitorIt;
		}
	}
	return Customer;
}

void ABotanicusCashRegisterActor::HandleCheckoutAction(
	AActor* Interactor)
{
	if (!HasAuthority() || !IsValid(Interactor))
	{
		return;
	}
	if (ABotanicusVisitorCharacter* Customer = GetCheckoutCustomer())
	{
		Customer->HandlePlayerCheckoutAction();
	}
}

bool ABotanicusCashRegisterActor::IsInsideCheckoutZone() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		if (ZoneIt->GetZoneType() ==
				EBotanicusVisitorZoneType::Checkout &&
			ZoneIt->ContainsPoint2D(GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

bool ABotanicusCashRegisterActor::
	IsLocalPlayerTargetingRegister() const
{
	UWorld* World = GetWorld();
	APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Controller || !Controller->IsLocalController() || !Pawn ||
		FVector::DistSquared(
			Pawn->GetActorLocation(),
			GetActorLocation()) > FMath::Square(450.0f))
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
		FVector::DotProduct(
			ViewRotation.Vector(),
			ToTarget / Distance) <
			FMath::Cos(FMath::DegreesToRadians(24.0f)))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusCashRegisterLook),
		false,
		Pawn);
	FHitResult Hit;
	const bool bHit =
		World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			TargetOrigin,
			ECC_Visibility,
			QueryParams);
	return !bHit || Hit.GetActor() == this;
}

void ABotanicusCashRegisterActor::RefreshVisuals()
{
	if (!StatusText || !ContextActionText ||
		ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		return;
	}

	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const bool bShowSelfCheckoutSlots =
		GameState && GameState->GetMainShopLevel() >= 3;
	for (UStaticMeshComponent* SlotVisual :
		 SelfCheckoutSlotVisuals)
	{
		if (SlotVisual)
		{
			SlotVisual->SetVisibility(bShowSelfCheckoutSlots);
		}
	}
	if (SelfCheckoutZoneLabel)
	{
		SelfCheckoutZoneLabel->SetVisibility(
			bShowSelfCheckoutSlots);
	}

	if (!IsInsideCheckoutZone())
	{
		StatusText->SetText(
			FText::FromString(
				TEXT("CAISSE\nA PLACER DANS LA ZONE CAISSE")));
		StatusText->SetTextRenderColor(FColor(255, 150, 70));
		ContextActionText->SetVisibility(false);
		return;
	}

	ABotanicusVisitorCharacter* Customer = GetCheckoutCustomer();
	if (!Customer)
	{
		StatusText->SetText(
			FText::FromString(TEXT("CAISSE\nEN ATTENTE D'UN CLIENT")));
		StatusText->SetTextRenderColor(FColor(160, 230, 180));
		ContextActionText->SetVisibility(false);
		return;
	}

	StatusText->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("CAISSE\nCLIENT : %d CREDITS"),
				Customer->GetCheckoutPrice())));
	StatusText->SetTextRenderColor(FColor(120, 245, 160));
	const bool bTargeted = IsLocalPlayerTargetingRegister();
	ContextActionText->SetVisibility(bTargeted);
	ContextActionText->SetText(
		FText::FromString(
			Customer->IsCheckoutPlantScanned()
				? FString::Printf(
					TEXT("CLIQUE GAUCHE : ENCAISSER %d CREDITS"),
					Customer->GetCheckoutPrice())
				: TEXT("CLIQUE GAUCHE : SCANNER LA PLANTE")));
}
