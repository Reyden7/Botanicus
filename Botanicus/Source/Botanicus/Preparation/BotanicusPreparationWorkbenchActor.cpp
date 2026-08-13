// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusPreparationWorkbenchActor.h"

#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusSalePotActor.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusPreparationWorkbenchActor::
ABotanicusPreparationWorkbenchActor()
{
	// Workbenches are authored through BP_WorkBench. Preserve the Blueprint's
	// meshes, materials and component transforms instead of replacing them
	// with the native catalogue cube used only as a fallback definition.
	bUseBlueprintAppearance = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	InteractionName =
		NSLOCTEXT(
			"BotanicusPreparation",
			"PreparationWorkbench",
			"Etabli de preparation");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	UpgradeTerminal =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("WorkbenchUpgradeTerminal"));
	UpgradeTerminal->SetupAttachment(SceneRoot);
	UpgradeTerminal->SetStaticMesh(
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
	UpgradeTerminal->SetRelativeLocation(
		FVector(0.0f, 105.0f, 70.0f));
	UpgradeTerminal->SetRelativeScale3D(FVector(0.20f));
	UpgradeTerminal->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);
	UpgradeTerminal->SetCollisionResponseToAllChannels(ECR_Ignore);
	UpgradeTerminal->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);
	UpgradeTerminal->SetCastShadow(false);

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

	PlacementPrompt =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("SalePotPlacementPrompt"));
	PlacementPrompt->SetupAttachment(SceneRoot);
	PlacementPrompt->SetRelativeLocation(
		FVector(0.0f, 0.0f, 165.0f));
	PlacementPrompt->SetText(
		NSLOCTEXT(
			"BotanicusPreparation",
			"PlaceSalePotPrompt",
			"CLIC GAUCHE POUR POSER LE POT SUR CE SLOT"));
	PlacementPrompt->SetTextRenderColor(FColor(90, 235, 255));
	PlacementPrompt->SetHorizontalAlignment(EHTA_Center);
	PlacementPrompt->SetVerticalAlignment(EVRTA_TextCenter);
	PlacementPrompt->SetWorldSize(16.0f);
	PlacementPrompt->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	PlacementPrompt->SetVisibility(false);
}

void ABotanicusPreparationWorkbenchActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshLevelVisuals();
}

void ABotanicusPreparationWorkbenchActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshLocalPlacementPrompt();
	if (HasAuthority() && !GetCarrier() && !IsInPlacementMode())
	{
		AlignPreparedPotsToSlots();
	}
}

void ABotanicusPreparationWorkbenchActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusPreparationWorkbenchActor,
		WorkbenchLevel);
}

FBotanicusInteractionPrompt
ABotanicusPreparationWorkbenchActor::
GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (IsUpgradeTerminalTargeted(Interactor))
	{
		FBotanicusInteractionPrompt Prompt;
		Prompt.ActionText =
			NSLOCTEXT(
				"BotanicusPreparation",
				"OpenWorkbenchUpgrades",
				"Ouvrir");
		Prompt.TargetName =
			NSLOCTEXT(
				"BotanicusPreparation",
				"WorkbenchUpgradeTerminal",
				"Ameliorations de l'atelier");
		Prompt.bCanInteract =
			CanAccessUpgradeTerminal(Interactor);
		return Prompt;
	}

	return Super::GetInteractionPrompt_Implementation(Interactor);
}

bool ABotanicusPreparationWorkbenchActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	if (IsUpgradeTerminalTargeted(Interactor))
	{
		return CanAccessUpgradeTerminal(Interactor);
	}

	return (!HasPreparedPots() || bMoveContentsWithFurniture) &&
		Super::CanInteract_Implementation(Interactor);
}

void ABotanicusPreparationWorkbenchActor::BeginPlacement(
	ABotanicusCharacter* Character)
{
	if (bMoveContentsWithFurniture &&
		MovingPreparedPots.IsEmpty())
	{
		AttachPreparedPotsForMove();
	}
	Super::BeginPlacement(Character);
}

void ABotanicusPreparationWorkbenchActor::UpdatePlacement(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	Super::UpdatePlacement(PlacementTransform, bIsValid);
}

void ABotanicusPreparationWorkbenchActor::ConfirmPlacement()
{
	Super::ConfirmPlacement();
	DetachPreparedPotsAfterMove();
	bMoveContentsWithFurniture = false;
}

void ABotanicusPreparationWorkbenchActor::CancelPlacement()
{
	Super::CancelPlacement();
	DetachPreparedPotsAfterMove();
	bMoveContentsWithFurniture = false;
}

void ABotanicusPreparationWorkbenchActor::SetLocalPlacementPreview(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	if (bMoveContentsWithFurniture &&
		MovingPreparedPots.IsEmpty())
	{
		AttachPreparedPotsForMove();
	}
	Super::SetLocalPlacementPreview(PlacementTransform, bIsValid);
}

FTransform ABotanicusPreparationWorkbenchActor::
GetSalePotPreparationTransform(
	int32 SlotIndex,
	const ABotanicusSalePotActor* Pot) const
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

		// Le marqueur définit la position horizontale et la rotation. Le pot
		// garde sa taille normale et reçoit sa propre correction verticale :
		// déplacer visuellement la plaque ne doit pas enfoncer le pot.
		SlotTransform.SetScale3D(FVector::OneVector);
		SlotTransform.AddToTranslation(
			SlotTransform.GetRotation().RotateVector(
				FVector(
					0.0f,
					0.0f,
					SalePotPlacementHeightOffset +
						(IsValid(Pot)
							? Pot->GetPreparationHeightAdjustment()
							: 0.0f))));

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
FindAimedAvailableSalePotSlot(
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	FTransform& OutTransform,
	int32& OutSlotIndex,
	const ABotanicusSalePotActor* IgnoredPot) const
{
	OutSlotIndex = INDEX_NONE;
	const FVector SafeViewDirection =
		ViewDirection.GetSafeNormal();
	if (SafeViewDirection.IsNearlyZero())
	{
		return false;
	}

	float BestAimDistanceSquared = FMath::Square(28.0f);
	float BestForwardDistance = TNumericLimits<float>::Max();
	for (int32 SlotIndex = 0;
		 SlotIndex < GetSlotCount();
		 ++SlotIndex)
	{
		if (IsSlotOccupied(SlotIndex, IgnoredPot))
		{
			continue;
		}

		const FTransform SlotTransform =
			GetSalePotPreparationTransform(SlotIndex);
		const FVector ToSlot =
			SlotTransform.GetLocation() - ViewLocation;
		const float ForwardDistance =
			FVector::DotProduct(ToSlot, SafeViewDirection);
		if (ForwardDistance <= 0.0f ||
			ForwardDistance > 600.0f)
		{
			continue;
		}

		const FVector ClosestPointOnViewRay =
			ViewLocation + SafeViewDirection * ForwardDistance;
		const float AimDistanceSquared =
			FVector::DistSquared(
				ClosestPointOnViewRay,
				SlotTransform.GetLocation());
		if (AimDistanceSquared < BestAimDistanceSquared ||
			(FMath::IsNearlyEqual(
				 AimDistanceSquared,
				 BestAimDistanceSquared) &&
			 ForwardDistance < BestForwardDistance))
		{
			BestAimDistanceSquared = AimDistanceSquared;
			BestForwardDistance = ForwardDistance;
			OutTransform = SlotTransform;
			OutSlotIndex = SlotIndex;
		}
	}

	return OutSlotIndex != INDEX_NONE;
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
	return GetLevelUnlockCost(WorkbenchLevel + 1);
}

int32 ABotanicusPreparationWorkbenchActor::GetLevelUnlockCost(
	int32 Level)
{
	if (Level < 2 || Level > 5)
	{
		return 0;
	}

	static const int32 LevelCosts[] = {
		0,
		0,
		500,
		900,
		1400,
		2000 };
	return LevelCosts[Level];
}

int32 ABotanicusPreparationWorkbenchActor::
GetLevelTransitionCost(int32 TargetLevel) const
{
	const int32 SafeTargetLevel = FMath::Clamp(TargetLevel, 1, 5);
	int32 Cost = 0;
	if (SafeTargetLevel > WorkbenchLevel)
	{
		for (int32 Level = WorkbenchLevel + 1;
			 Level <= SafeTargetLevel;
			 ++Level)
		{
			Cost += GetLevelUnlockCost(Level);
		}
	}
	else
	{
		for (int32 Level = SafeTargetLevel + 1;
			 Level <= WorkbenchLevel;
			 ++Level)
		{
			Cost -= GetLevelUnlockCost(Level);
		}
	}
	return Cost;
}

bool ABotanicusPreparationWorkbenchActor::UpgradeWorkbench()
{
	return SetWorkbenchLevel(WorkbenchLevel + 1);
}

bool ABotanicusPreparationWorkbenchActor::
CanChangeWorkbenchLevel(int32 TargetLevel) const
{
	if (TargetLevel < 1 || TargetLevel > 5)
	{
		return false;
	}

	if (TargetLevel >= WorkbenchLevel)
	{
		return true;
	}

	TArray<ABotanicusSalePotActor*> Pots;
	GetPreparedPots(Pots);
	for (const ABotanicusSalePotActor* Pot : Pots)
	{
		if (!IsValid(Pot))
		{
			continue;
		}

		int32 ClosestSlotIndex = INDEX_NONE;
		float ClosestDistanceSquared =
			TNumericLimits<float>::Max();
		for (int32 SlotIndex = 0;
			 SlotIndex < WorkbenchLevel;
			 ++SlotIndex)
		{
			const float DistanceSquared =
				FVector::DistSquared(
					Pot->GetActorLocation(),
					GetSalePotPreparationTransform(
						SlotIndex).GetLocation());
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestSlotIndex = SlotIndex;
			}
		}

		if (ClosestSlotIndex >= TargetLevel)
		{
			return false;
		}
	}

	return true;
}

bool ABotanicusPreparationWorkbenchActor::
SetWorkbenchLevel(int32 TargetLevel)
{
	if (!HasAuthority() ||
		TargetLevel == WorkbenchLevel ||
		!CanChangeWorkbenchLevel(TargetLevel))
	{
		return false;
	}

	WorkbenchLevel = TargetLevel;
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
		AttachPreparedPotsForMove();
	}
	else
	{
		DetachPreparedPotsAfterMove();
	}
}

bool ABotanicusPreparationWorkbenchActor::
CanAccessUpgradeTerminal(const AActor* Interactor) const
{
	return IsValid(Interactor) &&
		IsValid(UpgradeTerminal) &&
		!IsValid(GetCarrier()) &&
		!IsInPlacementMode() &&
		FVector::DistSquared2D(
			Interactor->GetActorLocation(),
			UpgradeTerminal->GetComponentLocation()) <=
			FMath::Square(230.0f);
}

bool ABotanicusPreparationWorkbenchActor::
IsUpgradeTerminalTargeted(const AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AController* Controller =
		Pawn ? Pawn->GetController() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Controller || !World ||
		!CanAccessUpgradeTerminal(Interactor))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const FBoxSphereBounds TerminalBounds =
		UpgradeTerminal->Bounds;
	const FVector ToTerminal =
		TerminalBounds.Origin - ViewLocation;
	const float DistanceAlongView =
		FVector::DotProduct(ToTerminal, ViewDirection);
	if (DistanceAlongView < 0.0f || DistanceAlongView > 430.0f)
	{
		return false;
	}

	// The visible tablet is mounted almost flush against the workbench.  Its
	// parent mesh can therefore win the visibility trace even while the
	// crosshair is directly over the screen.  Validate the aim against the
	// terminal bounds first, then accept a visibility hit on either the
	// terminal itself or its owning workbench.
	const FVector ClosestPointOnViewRay =
		ViewLocation + ViewDirection * DistanceAlongView;
	const float AimTolerance = FMath::Max(
		TerminalBounds.SphereRadius + 8.0f,
		18.0f);
	if (FVector::DistSquared(
			ClosestPointOnViewRay,
			TerminalBounds.Origin) >
		FMath::Square(AimTolerance))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusWorkbenchUpgradeTerminal),
		false,
		Interactor);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			ViewLocation + ViewDirection * 430.0f,
			ECC_Visibility,
			QueryParams) &&
		Hit.GetActor() == this;
}

bool ABotanicusPreparationWorkbenchActor::IsSlotOccupied(
	int32 SlotIndex,
	const ABotanicusSalePotActor* IgnoredPot) const
{
	const UWorld* World = GetWorld();
	if (!World ||
		SlotIndex < 0 ||
		SlotIndex >= GetSlotCount() ||
		!SlotMarkers.IsValidIndex(SlotIndex))
	{
		return true;
	}

	for (TActorIterator<ABotanicusSalePotActor> PotIt(World);
		 PotIt;
		 ++PotIt)
	{
		if (*PotIt == IgnoredPot ||
			PotIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}

		// Attribute each pot to one and only one active slot. Testing every
		// slot with an independent radius allowed a pot to occupy two nearby
		// markers on the final workbench meshes, effectively limiting a level
		// 2 workbench to one usable slot.
		int32 ClosestSlotIndex = INDEX_NONE;
		float ClosestDistanceSquared = FMath::Square(60.0f);
		for (int32 CandidateSlotIndex = 0;
			 CandidateSlotIndex < GetSlotCount();
			 ++CandidateSlotIndex)
		{
			const float DistanceSquared = FVector::DistSquared(
				PotIt->GetActorLocation(),
				GetSalePotPreparationTransform(CandidateSlotIndex).
					GetLocation());
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestSlotIndex = CandidateSlotIndex;
			}
		}

		if (ClosestSlotIndex == SlotIndex)
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
		// BP_WorkBench authors the inherited Mesh and PreparationSlot
		// components in the same local frame. Resetting only the mesh here
		// separates it from the slots at runtime (most visibly on the starter
		// workbench). Identity transforms are only appropriate for the native
		// fallback class.
		if (!UsesBlueprintAppearance())
		{
			Mesh->SetRelativeLocation(FVector::ZeroVector);
			Mesh->SetRelativeRotation(FRotator::ZeroRotator);
			Mesh->SetRelativeScale3D(FVector::OneVector);
		}
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

void ABotanicusPreparationWorkbenchActor::AlignPreparedPotsToSlots()
{
	TArray<ABotanicusSalePotActor*> Pots;
	GetPreparedPots(Pots);
	for (ABotanicusSalePotActor* Pot : Pots)
	{
		if (!IsValid(Pot))
		{
			continue;
		}

		int32 ClosestSlotIndex = INDEX_NONE;
		float ClosestDistanceSquared = FMath::Square(60.0f);
		for (int32 SlotIndex = 0; SlotIndex < GetSlotCount(); ++SlotIndex)
		{
			const FTransform SlotTransform =
				GetSalePotPreparationTransform(SlotIndex);
			const float DistanceSquared = FVector::DistSquared2D(
				Pot->GetActorLocation(),
				SlotTransform.GetLocation());
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestSlotIndex = SlotIndex;
			}
		}

		if (ClosestSlotIndex == INDEX_NONE)
		{
			continue;
		}
		const FTransform SlotTransform =
			GetSalePotPreparationTransform(ClosestSlotIndex, Pot);
		if (!Pot->GetActorTransform().Equals(SlotTransform, 0.1f))
		{
			Pot->SetActorTransform(
				SlotTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Pot->ForceNetUpdate();
		}
	}
}

void ABotanicusPreparationWorkbenchActor::OnRep_WorkbenchLevel()
{
	RefreshLevelVisuals();
}

void ABotanicusPreparationWorkbenchActor::AttachPreparedPotsForMove()
{
	DetachPreparedPotsAfterMove();
	MovingPreparedPots.Reset();

	TArray<ABotanicusSalePotActor*> Pots;
	GetPreparedPots(Pots);
	for (ABotanicusSalePotActor* Pot : Pots)
	{
		if (!IsValid(Pot))
		{
			continue;
		}

		MovingPreparedPots.Add(Pot);
		Pot->AttachToActor(
			this,
			FAttachmentTransformRules::KeepWorldTransform);
		if (HasAuthority())
		{
			Pot->SetNetDormancy(DORM_Awake);
			Pot->FlushNetDormancy();
			Pot->ForceNetUpdate();
		}
	}
}

void ABotanicusPreparationWorkbenchActor::DetachPreparedPotsAfterMove()
{
	for (const TWeakObjectPtr<ABotanicusSalePotActor>& PotPtr :
		 MovingPreparedPots)
	{
		ABotanicusSalePotActor* Pot = PotPtr.Get();
		if (!IsValid(Pot) || Pot->GetAttachParentActor() != this)
		{
			continue;
		}

		Pot->DetachFromActor(
			FDetachmentTransformRules::KeepWorldTransform);
		if (HasAuthority())
		{
			Pot->SetNetDormancy(DORM_Awake);
			Pot->FlushNetDormancy();
			Pot->ForceNetUpdate();
		}
	}
	MovingPreparedPots.Reset();
}

void ABotanicusPreparationWorkbenchActor::
RefreshLocalPlacementPrompt()
{
	if (!PlacementPrompt)
	{
		return;
	}

	PlacementPrompt->SetVisibility(false);
	UWorld* World = GetWorld();
	APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	ABotanicusCharacter* Character =
		Controller && Controller->IsLocalController()
			? Cast<ABotanicusCharacter>(Controller->GetPawn())
			: nullptr;
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!Controller ||
		!Controller->PlayerCameraManager ||
		!Character ||
		!QuickBar ||
		(QuickBar->GetSelectedSlot().ItemKey != TEXT("SalePot") &&
		 QuickBar->GetSelectedSlot().ItemKey != TEXT("SalePotSquare")) ||
		!IsSalePotSlotAvailable())
	{
		return;
	}

	const FVector CameraLocation =
		Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraToWorkbench =
		GetActorLocation() - CameraLocation;
	const float CameraDistance = CameraToWorkbench.Size();
	if (CameraDistance <= KINDA_SMALL_NUMBER ||
		CameraDistance > 600.0f ||
		FVector::DistSquared(
			Character->GetActorLocation(),
			GetActorLocation()) > FMath::Square(500.0f) ||
		FVector::DotProduct(
			Controller->PlayerCameraManager->
				GetCameraRotation().Vector(),
			CameraToWorkbench / CameraDistance) <
			FMath::Cos(FMath::DegreesToRadians(28.0f)))
	{
		return;
	}

	PlacementPrompt->SetVisibility(true);
	PlacementPrompt->SetWorldRotation(
		(CameraLocation -
		 PlacementPrompt->GetComponentLocation()).Rotation());
}
