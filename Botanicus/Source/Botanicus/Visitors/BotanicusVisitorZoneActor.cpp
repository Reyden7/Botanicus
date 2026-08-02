// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visitors/BotanicusVisitorZoneActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusVisitorZoneActor::ABotanicusVisitorZoneActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(
		TEXT("VisitorZoneBounds"));
	SetRootComponent(ZoneBounds);
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ZoneVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("VisitorZoneVisual"));
	ZoneVisual->SetupAttachment(ZoneBounds);
	ZoneVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		ZoneVisual->SetStaticMesh(CubeFinder.Object);
	}

	ZoneLabel = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("VisitorZoneLabel"));
	ZoneLabel->SetupAttachment(ZoneBounds);
	ZoneLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
	ZoneLabel->SetHorizontalAlignment(EHTA_Center);
	ZoneLabel->SetVerticalAlignment(EVRTA_TextCenter);
	ZoneLabel->SetWorldSize(24.0f);
	ZoneLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshVisuals();
}

void ABotanicusVisitorZoneActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusVisitorZoneActor, ZoneType);
	DOREPLIFETIME(ABotanicusVisitorZoneActor, BoxExtent);
}

void ABotanicusVisitorZoneActor::InitializeZone(
	EBotanicusVisitorZoneType InZoneType,
	const FVector& InBoxExtent)
{
	if (!HasAuthority())
	{
		return;
	}
	ZoneType = InZoneType;
	BoxExtent = FVector(
		FMath::Max(50.0f, InBoxExtent.X),
		FMath::Max(50.0f, InBoxExtent.Y),
		FMath::Max(2.0f, InBoxExtent.Z));
	DynamicMaterial = nullptr;
	RefreshVisuals();
	ForceNetUpdate();
}

bool ABotanicusVisitorZoneActor::ContainsPoint2D(
	const FVector& WorldPoint) const
{
	const FVector LocalPoint =
		GetActorTransform().InverseTransformPosition(WorldPoint);
	return FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
		FMath::Abs(LocalPoint.Y) <= BoxExtent.Y;
}

void ABotanicusVisitorZoneActor::OnRep_ZoneConfiguration()
{
	DynamicMaterial = nullptr;
	RefreshVisuals();
}

void ABotanicusVisitorZoneActor::RefreshVisuals()
{
	if (ZoneBounds)
	{
		ZoneBounds->SetBoxExtent(BoxExtent);
	}
	if (ZoneVisual)
	{
		ZoneVisual->SetRelativeScale3D(
			FVector(
				BoxExtent.X / 50.0f,
				BoxExtent.Y / 50.0f,
				BoxExtent.Z / 50.0f));
	}

	FLinearColor ZoneColor = FLinearColor::White;
	FText Label;
	switch (ZoneType)
	{
	case EBotanicusVisitorZoneType::Parking:
		ZoneColor = FLinearColor(0.08f, 0.35f, 0.85f, 1.0f);
		Label = FText::FromString(TEXT("PARKING PNJ"));
		break;
	case EBotanicusVisitorZoneType::SalesArea:
		ZoneColor = FLinearColor(0.12f, 0.8f, 0.3f, 1.0f);
		Label = FText::FromString(TEXT("ESPACE VENTE PNJ"));
		break;
	case EBotanicusVisitorZoneType::Checkout:
		ZoneColor = FLinearColor(0.95f, 0.55f, 0.05f, 1.0f);
		Label = FText::FromString(TEXT("CAISSE PNJ"));
		break;
	}

	if (ZoneVisual && !DynamicMaterial)
	{
		UMaterialInterface* BaseMaterial =
			ZoneVisual->GetMaterial(0);
		DynamicMaterial =
			UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(
				TEXT("Color"),
				ZoneColor);
			ZoneVisual->SetMaterial(0, DynamicMaterial);
		}
	}
	if (ZoneLabel)
	{
		ZoneLabel->SetText(Label);
		ZoneLabel->SetTextRenderColor(ZoneColor.ToFColor(true));
	}
}
