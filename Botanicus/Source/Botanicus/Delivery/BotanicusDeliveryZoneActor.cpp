// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryZoneActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
	Pad->SetCollisionProfileName(TEXT("BlockAll"));
	Pad->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	Pad->SetRelativeScale3D(FVector(4.8f, 4.8f, 0.15f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Pad->SetStaticMesh(CubeFinder.Object);
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
			16.0f);
}
