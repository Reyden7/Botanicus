// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryZoneActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusDeliveryZoneActor::ABotanicusDeliveryZoneActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Delivery Pad"));
	Pad->SetupAttachment(Root);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pad->SetRelativeLocation(FVector(0.0f, 0.0f, 0.25f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		Pad->SetStaticMesh(PlaneFinder.Object);
	}

	ZoneLabel = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("DeliveryZoneLabel"));
	ZoneLabel->SetupAttachment(Root);
	ZoneLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	ZoneLabel->SetHorizontalAlignment(EHTA_Center);
	ZoneLabel->SetVerticalAlignment(EVRTA_TextCenter);
	ZoneLabel->SetWorldSize(24.0f);
	ZoneLabel->SetText(
		NSLOCTEXT(
			"BotanicusDelivery",
			"DeliveryZoneLabel",
			"ZONE DE LIVRAISON"));
	ZoneLabel->SetTextRenderColor(FColor(255, 210, 45));
	ZoneLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshVisuals();
}

void ABotanicusDeliveryZoneActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SnapToUnderlyingGround();
	}
}

void ABotanicusDeliveryZoneActor::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisuals();
}

void ABotanicusDeliveryZoneActor::RefreshVisuals()
{
	if (Pad)
	{
		Pad->SetRelativeLocation(FVector(0.0f, 0.0f, 0.25f));
		Pad->SetRelativeScale3D(
			FVector(ZoneSize.X / 100.0f, ZoneSize.Y / 100.0f, 1.0f));
		Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

FVector ABotanicusDeliveryZoneActor::GetParcelSpawnLocation(
	int32 ParcelIndex) const
{
	const int32 Column = FMath::Abs(ParcelIndex) % 3;
	const int32 Row = FMath::Abs(ParcelIndex) / 3;
	return GetActorLocation() +
		FVector(
			(Column - 1) * 160.0f,
			(Row % 3 - 1) * 160.0f,
			0.0f);
}

void ABotanicusDeliveryZoneActor::SnapToUnderlyingGround()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusDeliveryZoneGround),
		false,
		this);
	FHitResult GroundHit;
	const FVector Location = GetActorLocation();
	if (World->LineTraceSingleByChannel(
			GroundHit,
			Location + FVector(0.0f, 0.0f, 1.0f),
			Location - FVector(0.0f, 0.0f, 1000.0f),
			ECC_Visibility,
			QueryParams))
	{
		SetActorLocation(
			FVector(Location.X, Location.Y, GroundHit.ImpactPoint.Z),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		ForceNetUpdate();
	}
}
