// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/BotanicusCatalogBuildingActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusCatalogBuildingActor::ABotanicusCatalogBuildingActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	BuildingRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("BuildingRoot"));
	SetRootComponent(BuildingRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
}

UStaticMeshComponent* ABotanicusCatalogBuildingActor::AddBuildingPart(
	FName ComponentName,
	const FVector& Location,
	const FVector& Size,
	bool bCollisionEnabled)
{
	UStaticMeshComponent* Part =
		CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
	Part->SetupAttachment(BuildingRoot);
	Part->SetStaticMesh(CubeMesh);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Size / 100.0f);
	Part->SetCollisionEnabled(
		bCollisionEnabled
			? ECollisionEnabled::QueryAndPhysics
			: ECollisionEnabled::NoCollision);
	Part->SetCollisionResponseToAllChannels(ECR_Block);
	return Part;
}

void ABotanicusCatalogBuildingActor::BuildGeometry(
	float Width,
	float Depth,
	float Height)
{
	const float WallThickness = 35.0f;
	const float FloorThickness = 40.0f;
	const float RoofThickness = 30.0f;
	const float DoorWidth = 260.0f;
	const float FrontSegmentWidth =
		FMath::Max(100.0f, (Width - DoorWidth) * 0.5f);

	AddBuildingPart(
		TEXT("Floor"),
		FVector(0.0f, 0.0f, FloorThickness * 0.5f),
		FVector(Width, Depth, FloorThickness));
	AddBuildingPart(
		TEXT("BackWall"),
		FVector(0.0f, Depth * 0.5f, Height * 0.5f),
		FVector(Width, WallThickness, Height));
	AddBuildingPart(
		TEXT("LeftWall"),
		FVector(-Width * 0.5f, 0.0f, Height * 0.5f),
		FVector(WallThickness, Depth, Height));
	AddBuildingPart(
		TEXT("RightWall"),
		FVector(Width * 0.5f, 0.0f, Height * 0.5f),
		FVector(WallThickness, Depth, Height));
	AddBuildingPart(
		TEXT("FrontWallLeft"),
		FVector(
			-(DoorWidth + FrontSegmentWidth) * 0.5f,
			-Depth * 0.5f,
			Height * 0.5f),
		FVector(FrontSegmentWidth, WallThickness, Height));
	AddBuildingPart(
		TEXT("FrontWallRight"),
		FVector(
			(DoorWidth + FrontSegmentWidth) * 0.5f,
			-Depth * 0.5f,
			Height * 0.5f),
		FVector(FrontSegmentWidth, WallThickness, Height));
	AddBuildingPart(
		TEXT("Roof"),
		FVector(0.0f, 0.0f, Height + RoofThickness * 0.5f),
		FVector(Width + 80.0f, Depth + 80.0f, RoofThickness));
}

ABotanicusCompactGreenhouseActor::ABotanicusCompactGreenhouseActor()
{
	BuildGeometry(1200.0f, 900.0f, 520.0f);
}

ABotanicusWorkshopGreenhouseActor::ABotanicusWorkshopGreenhouseActor()
{
	BuildGeometry(1800.0f, 1200.0f, 620.0f);
}
