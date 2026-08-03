// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusPreparationWorkbenchActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Sales/BotanicusSalePotActor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
float PreparationSlotOffsetX(int32 SlotIndex, float SlotSpacing)
{
	switch (SlotIndex)
	{
	case 1:
		return -SlotSpacing;
	case 2:
		return SlotSpacing;
	case 3:
		return -SlotSpacing * 2.0f;
	case 4:
		return SlotSpacing * 2.0f;
	default:
		return 0.0f;
	}
}
}

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

	for (int32 SlotIndex = 0; SlotIndex < 5; ++SlotIndex)
	{
		UStaticMeshComponent* Marker =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(
					TEXT("PreparationSlot%d"),
					SlotIndex));
		Marker->SetupAttachment(SceneRoot);
		Marker->SetStaticMesh(
			CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
		Marker->SetRelativeScale3D(
			FVector(0.26f, 0.26f, 0.015f));
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SlotMarkers.Add(Marker);
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
	RefreshLevelVisuals();
}

void ABotanicusPreparationWorkbenchActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshLevelVisuals();
}

void ABotanicusPreparationWorkbenchActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusPreparationWorkbenchActor,
		WorkbenchLevel);
}

bool ABotanicusPreparationWorkbenchActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return (!HasPreparedPots() || bMoveContentsWithFurniture) &&
		Super::CanInteract_Implementation(Interactor);
}

void ABotanicusPreparationWorkbenchActor::BeginPlacement(
	ABotanicusCharacter* Character)
{
	if (bMoveContentsWithFurniture &&
		MovingPreparedPots.IsEmpty())
	{
		CapturePreparedPotTransforms();
	}
	Super::BeginPlacement(Character);
	ApplyPreparedPotTransforms();
}

void ABotanicusPreparationWorkbenchActor::UpdatePlacement(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	Super::UpdatePlacement(PlacementTransform, bIsValid);
	ApplyPreparedPotTransforms();
}

void ABotanicusPreparationWorkbenchActor::ConfirmPlacement()
{
	Super::ConfirmPlacement();
	ApplyPreparedPotTransforms();
	ClearPreparedPotTransforms();
	bMoveContentsWithFurniture = false;
}

void ABotanicusPreparationWorkbenchActor::CancelPlacement()
{
	Super::CancelPlacement();
	ApplyPreparedPotTransforms();
	ClearPreparedPotTransforms();
	bMoveContentsWithFurniture = false;
}

void ABotanicusPreparationWorkbenchActor::SetLocalPlacementPreview(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	if (bMoveContentsWithFurniture &&
		MovingPreparedPots.IsEmpty())
	{
		CapturePreparedPotTransforms();
	}
	Super::SetLocalPlacementPreview(PlacementTransform, bIsValid);
	ApplyPreparedPotTransforms();
}

FTransform ABotanicusPreparationWorkbenchActor::
	GetSalePotPreparationTransform(int32 SlotIndex) const
{
	const int32 SafeSlotIndex =
		FMath::Clamp(SlotIndex, 0, GetSlotCount() - 1);
	return FTransform(
		GetActorRotation(),
		GetActorTransform().TransformPosition(
			FVector(
				PreparationSlotOffsetX(
					SafeSlotIndex,
					SlotSpacing),
				0.0f,
				107.0f)));
}

bool ABotanicusPreparationWorkbenchActor::
	FindClosestAvailableSalePotSlot(
		const FVector& ReferenceLocation,
		FTransform& OutTransform,
		const ABotanicusSalePotActor* IgnoredPot) const
{
	bool bFoundSlot = false;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (int32 SlotIndex = 0;
		 SlotIndex < GetSlotCount();
		 ++SlotIndex)
	{
		if (IsSlotOccupied(SlotIndex, IgnoredPot))
		{
			continue;
		}
		const FTransform Candidate =
			GetSalePotPreparationTransform(SlotIndex);
		const float DistanceSquared = FVector::DistSquared2D(
			ReferenceLocation,
			Candidate.GetLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			OutTransform = Candidate;
			bFoundSlot = true;
		}
	}
	return bFoundSlot;
}

bool ABotanicusPreparationWorkbenchActor::
	IsLocationOnPreparationSlot(
		const FVector& WorldLocation,
		float Tolerance) const
{
	for (int32 SlotIndex = 0;
		 SlotIndex < GetSlotCount();
		 ++SlotIndex)
	{
		if (FVector::DistSquared(
				WorldLocation,
				GetSalePotPreparationTransform(SlotIndex).
					GetLocation()) <= FMath::Square(Tolerance))
		{
			return true;
		}
	}
	return false;
}

bool ABotanicusPreparationWorkbenchActor::
	IsSalePotSlotAvailable(
		const ABotanicusSalePotActor* IgnoredPot) const
{
	for (int32 SlotIndex = 0;
		 SlotIndex < GetSlotCount();
		 ++SlotIndex)
	{
		if (!IsSlotOccupied(SlotIndex, IgnoredPot))
		{
			return true;
		}
	}
	return false;
}

int32 ABotanicusPreparationWorkbenchActor::GetUpgradeCost() const
{
	static const int32 UpgradeCosts[] = {
		500,
		900,
		1400,
		2000};
	return WorkbenchLevel >= 1 && WorkbenchLevel < 5
		? UpgradeCosts[WorkbenchLevel - 1]
		: 0;
}

bool ABotanicusPreparationWorkbenchActor::UpgradeWorkbench()
{
	if (!HasAuthority() || !CanUpgrade())
	{
		return false;
	}
	++WorkbenchLevel;
	RefreshLevelVisuals();
	ForceNetUpdate();
	return true;
}

void ABotanicusPreparationWorkbenchActor::RestoreWorkbenchLevel(
	int32 InLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	WorkbenchLevel = FMath::Clamp(InLevel, 1, 5);
	RefreshLevelVisuals();
	ForceNetUpdate();
}

bool ABotanicusPreparationWorkbenchActor::HasPreparedPots() const
{
	TArray<ABotanicusSalePotActor*> Pots;
	GetPreparedPots(Pots);
	return !Pots.IsEmpty();
}

void ABotanicusPreparationWorkbenchActor::GetPreparedPots(
	TArray<ABotanicusSalePotActor*>& OutPots) const
{
	OutPots.Reset();
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<ABotanicusSalePotActor> PotIt(GetWorld());
		 PotIt;
		 ++PotIt)
	{
		if (PotIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}
		if (IsLocationOnPreparationSlot(
				PotIt->GetActorLocation(),
				55.0f))
		{
			OutPots.Add(*PotIt);
		}
	}
}

void ABotanicusPreparationWorkbenchActor::SetMoveContentsWithFurniture(
	bool bEnabled)
{
	bMoveContentsWithFurniture = bEnabled;
	if (bEnabled)
	{
		CapturePreparedPotTransforms();
	}
	else
	{
		ClearPreparedPotTransforms();
	}
}

bool ABotanicusPreparationWorkbenchActor::IsSlotOccupied(
	int32 SlotIndex,
	const ABotanicusSalePotActor* IgnoredPot) const
{
	const UWorld* World = GetWorld();
	if (!World || !SlotMarkers.IsValidIndex(SlotIndex))
	{
		return true;
	}
	const FVector SlotLocation =
		GetSalePotPreparationTransform(SlotIndex).GetLocation();
	for (TActorIterator<ABotanicusSalePotActor> PotIt(World);
		 PotIt;
		 ++PotIt)
	{
		if (*PotIt != IgnoredPot &&
			FVector::DistSquared(
				PotIt->GetActorLocation(),
				SlotLocation) <= FMath::Square(45.0f))
		{
			return true;
		}
	}
	return false;
}

void ABotanicusPreparationWorkbenchActor::RefreshLevelVisuals()
{
	WorkbenchLevel = FMath::Clamp(WorkbenchLevel, 1, 5);
	const int32 MeshIndex = WorkbenchLevel - 1;
	const bool bHasLevelMesh =
		LevelMeshes.IsValidIndex(MeshIndex) &&
		LevelMeshes[MeshIndex];
	const bool bCustomVisual =
		IsUsingItemDataMesh() && !bHasLevelMesh;
	if (bHasLevelMesh)
	{
		Mesh->SetStaticMesh(LevelMeshes[MeshIndex]);
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}
	else if (bCustomVisual)
	{
		Mesh->SetRelativeLocation(FVector::ZeroVector);
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
		Mesh->SetRelativeScale3D(
			FVector(
				1.2f + (WorkbenchLevel - 1) * 0.75f,
				0.6f,
				0.12f));
	}

	const bool bShowProceduralLegs =
		!bCustomVisual && !bHasLevelMesh;
	for (UStaticMeshComponent* Leg : Legs)
	{
		if (Leg)
		{
			Leg->SetVisibility(bShowProceduralLegs);
			Leg->SetCollisionEnabled(
				bShowProceduralLegs
					? ECollisionEnabled::QueryAndPhysics
					: ECollisionEnabled::NoCollision);
		}
	}

	const float LegX =
		95.0f + (WorkbenchLevel - 1) * 37.5f;
	for (int32 LegIndex = 0;
		 LegIndex < Legs.Num();
		 ++LegIndex)
	{
		if (Legs[LegIndex])
		{
			FVector Location =
				Legs[LegIndex]->GetRelativeLocation();
			Location.X = LegIndex < 2 ? LegX : -LegX;
			Legs[LegIndex]->SetRelativeLocation(Location);
		}
	}

	for (int32 SlotIndex = 0;
		 SlotIndex < SlotMarkers.Num();
		 ++SlotIndex)
	{
		const bool bActive = SlotIndex < WorkbenchLevel;
		SlotMarkers[SlotIndex]->SetVisibility(bActive);
		if (bActive)
		{
			SlotMarkers[SlotIndex]->SetRelativeLocation(
				FVector(
					PreparationSlotOffsetX(
						SlotIndex,
						SlotSpacing),
					0.0f,
					103.0f));
		}
	}
	if (PreparationLabel)
	{
		PreparationLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("ATELIER NIVEAU %d - %d POTS"),
					WorkbenchLevel,
					WorkbenchLevel)));
	}
}

void ABotanicusPreparationWorkbenchActor::
	OnEquipmentDefinitionApplied()
{
	RefreshLevelVisuals();
}

void ABotanicusPreparationWorkbenchActor::OnRep_WorkbenchLevel()
{
	RefreshLevelVisuals();
}

void ABotanicusPreparationWorkbenchActor::CapturePreparedPotTransforms()
{
	MovingPreparedPots.Reset();
	MovingPreparedPotRelativeTransforms.Reset();
	TArray<ABotanicusSalePotActor*> Pots;
	GetPreparedPots(Pots);
	for (ABotanicusSalePotActor* Pot : Pots)
	{
		if (!IsValid(Pot))
		{
			continue;
		}
		MovingPreparedPots.Add(Pot);
		MovingPreparedPotRelativeTransforms.Add(
			Pot->GetActorTransform().GetRelativeTransform(
				GetActorTransform()));
	}
}

void ABotanicusPreparationWorkbenchActor::ApplyPreparedPotTransforms()
{
	for (int32 Index = 0;
		 MovingPreparedPots.IsValidIndex(Index) &&
		 MovingPreparedPotRelativeTransforms.IsValidIndex(Index);
		 ++Index)
	{
		ABotanicusSalePotActor* Pot = MovingPreparedPots[Index].Get();
		if (!IsValid(Pot))
		{
			continue;
		}
		Pot->SetActorTransform(
			MovingPreparedPotRelativeTransforms[Index] *
				GetActorTransform(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (HasAuthority())
		{
			Pot->SetNetDormancy(DORM_Awake);
			Pot->FlushNetDormancy();
			Pot->ForceNetUpdate();
		}
	}
}

void ABotanicusPreparationWorkbenchActor::ClearPreparedPotTransforms()
{
	MovingPreparedPots.Reset();
	MovingPreparedPotRelativeTransforms.Reset();
}
