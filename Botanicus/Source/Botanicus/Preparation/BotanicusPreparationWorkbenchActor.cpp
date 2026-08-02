// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusPreparationWorkbenchActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Sales/BotanicusSalePotActor.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusPreparationWorkbenchActor::
	ABotanicusPreparationWorkbenchActor()
{
	InteractionName =
		NSLOCTEXT(
			"BotanicusPreparation",
			"PreparationWorkbench",
			"Etabli de preparation");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	Mesh->SetRelativeScale3D(FVector(1.2f, 0.6f, 0.12f));

	const FVector LegLocations[] = {
		FVector(95.0f, 45.0f, 42.0f),
		FVector(95.0f, -45.0f, 42.0f),
		FVector(-95.0f, 45.0f, 42.0f),
		FVector(-95.0f, -45.0f, 42.0f)};
	for (int32 LegIndex = 0; LegIndex < 4; ++LegIndex)
	{
		UStaticMeshComponent* Leg =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("WorkbenchLeg%d"), LegIndex));
		Leg->SetupAttachment(SceneRoot);
		Leg->SetStaticMesh(
			CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
		Leg->SetRelativeLocation(LegLocations[LegIndex]);
		Leg->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.84f));
		Leg->SetCollisionProfileName(TEXT("BlockAll"));
		Legs.Add(Leg);
	}

	PreparationLabel =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("PreparationSlotLabel"));
	PreparationLabel->SetupAttachment(SceneRoot);
	PreparationLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	PreparationLabel->SetText(
		FText::FromString(TEXT("EMPLACEMENT POT DE VENTE")));
	PreparationLabel->SetTextRenderColor(FColor(80, 220, 255));
	PreparationLabel->SetHorizontalAlignment(EHTA_Center);
	PreparationLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PreparationLabel->SetWorldSize(14.0f);
	PreparationLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FTransform ABotanicusPreparationWorkbenchActor::
	GetSalePotPreparationTransform() const
{
	return FTransform(
		GetActorRotation(),
		GetActorTransform().TransformPosition(
			FVector(0.0f, 0.0f, 107.0f)));
}

bool ABotanicusPreparationWorkbenchActor::
	IsSalePotSlotAvailable(
		const ABotanicusSalePotActor* IgnoredPot) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FVector SlotLocation =
		GetSalePotPreparationTransform().GetLocation();
	for (TActorIterator<ABotanicusSalePotActor> PotIt(World);
		 PotIt;
		 ++PotIt)
	{
		if (*PotIt != IgnoredPot &&
			FVector::DistSquared(
				PotIt->GetActorLocation(),
				SlotLocation) <= FMath::Square(45.0f))
		{
			return false;
		}
	}
	return true;
}
