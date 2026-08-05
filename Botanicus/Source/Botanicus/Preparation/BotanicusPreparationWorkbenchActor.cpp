// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusPreparationWorkbenchActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
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
		FMath::Clamp(
			SlotIndex,
			0,
			GetSlotCount() - 1);

	if (SlotMarkers.IsValidIndex(SafeSlotIndex) &&
		IsValid(SlotMarkers[SafeSlotIndex]))
	{
		FTransform SlotTransform =
			SlotMarkers[SafeSlotIndex]->GetComponentTransform();

		// Le marqueur sert uniquement à définir la position
		// et la rotation. Le pot doit garder sa taille normale.
		SlotTransform.SetScale3D(FVector::OneVector);

		return SlotTransform;
	}

	FTransform FallbackTransform = GetActorTransform();
	FallbackTransform.SetScale3D(FVector::OneVector);
	return FallbackTransform;
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
		2000 };
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
	WorkbenchLevel =
		FMath::Clamp(WorkbenchLevel, 1, 5);

	const int32 MeshIndex = WorkbenchLevel - 1;

	// Un mesh complet différent pour chaque niveau.
	if (LevelMeshes.IsValidIndex(MeshIndex) &&
		IsValid(LevelMeshes[MeshIndex]))
	{
		Mesh->SetStaticMesh(LevelMeshes[MeshIndex]);

		// Les meshes doivent avoir leur taille définitive.
		// Ils ne sont plus étirés selon le niveau.
		Mesh->SetRelativeLocation(FVector::ZeroVector);
		Mesh->SetRelativeRotation(FRotator::ZeroRotator);
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Mesh manquant pour l'atelier de preparation niveau %d"),
			WorkbenchLevel);
	}

	// Active seulement les emplacements disponibles pour le niveau.
	// Leur position reste celle définie dans le Blueprint.
	for (int32 SlotIndex = 0;
		SlotIndex < SlotMarkers.Num();
		++SlotIndex)
	{
		if (!IsValid(SlotMarkers[SlotIndex]))
		{
			continue;
		}

		const bool bActive =
			SlotIndex < WorkbenchLevel;

		SlotMarkers[SlotIndex]->SetVisibility(bActive);
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