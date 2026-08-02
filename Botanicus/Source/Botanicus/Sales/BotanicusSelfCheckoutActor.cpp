// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sales/BotanicusSelfCheckoutActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusSelfCheckoutActor::ABotanicusSelfCheckoutActor()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractionName = NSLOCTEXT(
		"BotanicusSales",
		"SelfCheckout",
		"Caisse automatique");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube =
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	// The catalogue scale keeps the debug body exactly 40 cm wide and
	// 2 metres high. These pieces make the station readable as a checkout.
	ScreenVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Self Checkout Screen"));
	ScreenVisual->SetupAttachment(SceneRoot);
	ScreenVisual->SetStaticMesh(Cube);
	ScreenVisual->SetRelativeLocation(FVector(22.0f, 0.0f, 142.0f));
	ScreenVisual->SetRelativeScale3D(FVector(0.10f, 0.24f, 0.18f));
	ScreenVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ScannerVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Self Checkout Scanner"));
	ScannerVisual->SetupAttachment(SceneRoot);
	ScannerVisual->SetStaticMesh(Cube);
	ScannerVisual->SetRelativeLocation(FVector(24.0f, 0.0f, 92.0f));
	ScannerVisual->SetRelativeScale3D(FVector(0.08f, 0.25f, 0.05f));
	ScannerVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Self Checkout Status"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 225.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(16.0f);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABotanicusSelfCheckoutActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshVisuals();
}

void ABotanicusSelfCheckoutActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (StatusText)
	{
		StatusText->SetVisibility(false);
	}
}

FVector ABotanicusSelfCheckoutActor::GetCustomerStandLocation() const
{
	return GetActorLocation() +
		GetActorForwardVector() * 125.0f +
		FVector(0.0f, 0.0f, 84.0f);
}

bool ABotanicusSelfCheckoutActor::IsOperational() const
{
	return !ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
		IsInsideCheckoutZone() &&
		IsMountedInSelfCheckoutSlot();
}

ABotanicusVisitorCharacter*
ABotanicusSelfCheckoutActor::GetAssignedVisitor() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(World);
		 VisitorIt;
		 ++VisitorIt)
	{
		if (VisitorIt->GetAssignedSelfCheckout() == this)
		{
			return *VisitorIt;
		}
	}
	return nullptr;
}

bool ABotanicusSelfCheckoutActor::IsInsideCheckoutZone() const
{
	const UWorld* World = GetWorld();
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

bool ABotanicusSelfCheckoutActor::
	IsMountedInSelfCheckoutSlot() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABotanicusCashRegisterActor> RegisterIt(World);
		 RegisterIt;
		 ++RegisterIt)
	{
		if (!RegisterIt->ActorHasTag(
				TEXT("BotanicusPlacementPreview")) &&
			RegisterIt->FindSelfCheckoutSlotIndex(
				GetActorLocation(),
				20.0f) != INDEX_NONE)
		{
			return true;
		}
	}
	return false;
}

void ABotanicusSelfCheckoutActor::RefreshVisuals()
{
	if (!StatusText ||
		ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		return;
	}
	if (!IsMountedInSelfCheckoutSlot())
	{
		StatusText->SetText(FText::FromString(
			TEXT(
				"CAISSE AUTO\nA PLACER SUR UN EMPLACEMENT BLEU")));
		StatusText->SetTextRenderColor(FColor(255, 150, 70));
		return;
	}
	if (!IsInsideCheckoutZone())
	{
		StatusText->SetText(FText::FromString(
			TEXT("CAISSE AUTO\nA PLACER DANS LA ZONE CAISSE")));
		StatusText->SetTextRenderColor(FColor(255, 150, 70));
		return;
	}
	if (const ABotanicusVisitorCharacter* Visitor =
			GetAssignedVisitor())
	{
		StatusText->SetText(FText::FromString(
			Visitor->IsCheckoutPlantScanned()
				? TEXT("CAISSE AUTO\nPAIEMENT EN COURS")
				: TEXT("CAISSE AUTO\nSCAN EN COURS")));
		StatusText->SetTextRenderColor(FColor(100, 220, 255));
		return;
	}
	StatusText->SetText(
		FText::FromString(TEXT("CAISSE AUTO\nLIBRE")));
	StatusText->SetTextRenderColor(FColor(90, 255, 135));
}
