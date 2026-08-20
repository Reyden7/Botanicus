// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusLargeEquipmentActor.h"

#include "BotanicusCharacter.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "ItemDataAsset.h"
#include "ItemDataSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool IsFurnitureEquipmentKey(const FName ItemKey)
	{
		const FString ItemKeyString = ItemKey.ToString();
		return ItemKey == TEXT("PreparationWorkbench")
			|| ItemKeyString.StartsWith(TEXT("StorageShelf"))
			|| ItemKeyString.StartsWith(TEXT("WorkSurface"))
			|| ItemKeyString.StartsWith(TEXT("Climate"));
	}
}

ABotanicusLargeEquipmentActor::ABotanicusLargeEquipmentActor()
{
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;

	InteractionAction =
		NSLOCTEXT("BotanicusDelivery", "CarryEquipment", "Porter");
	InteractionName =
		NSLOCTEXT(
			"BotanicusDelivery",
			"LargeEquipment",
			"Gros équipement");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	Mesh->SetRelativeScale3D(FVector(1.0f, 0.65f, 0.75f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		ValidPlacementMaterialFinder(
			TEXT("/Game/Botanicus/Materials/Silhouette/M_Silhouette_Hologram_Blue.M_Silhouette_Hologram_Blue"));
	if (ValidPlacementMaterialFinder.Succeeded())
	{
		ValidPlacementMaterial = ValidPlacementMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		InvalidPlacementMaterialFinder(
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_CanNot_Build.MI_CanNot_Build"));
	if (InvalidPlacementMaterialFinder.Succeeded())
	{
		InvalidPlacementMaterial = InvalidPlacementMaterialFinder.Object;
	}

	InteractionIndicator =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("Interaction Indicator"));
	InteractionIndicator->SetupAttachment(SceneRoot);
	InteractionIndicator->SetRelativeLocation(
		FVector(0.0f, 0.0f, 125.0f));
	InteractionIndicator->SetText(
		FText::FromString(TEXT("[ E ] PORTER")));
	InteractionIndicator->SetTextRenderColor(FColor(255, 210, 30));
	InteractionIndicator->SetHorizontalAlignment(EHTA_Center);
	InteractionIndicator->SetVerticalAlignment(EVRTA_TextCenter);
	InteractionIndicator->SetWorldSize(30.0f);
	InteractionIndicator->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
}

void ABotanicusLargeEquipmentActor::SetInteractionIndicatorVisibility(
	bool bVisible)
{
	if (InteractionIndicator)
	{
		InteractionIndicator->SetVisibility(bVisible);
	}
}

void ABotanicusLargeEquipmentActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() &&
		bCooperativeCarry &&
		IsValid(Carrier) &&
		((IsValid(Helper) &&
		  FVector::DistSquared(
			  Carrier->GetActorLocation(),
			  Helper->GetActorLocation()) >
			  FMath::Square(700.0f)) ||
		 (!IsValid(Helper) &&
		  FVector::DistSquared(
			  Carrier->GetActorLocation(),
			  GetActorLocation()) >
			  FMath::Square(700.0f))))
	{
		EndCooperativeHold(Carrier);
	}

	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	APlayerCameraManager* CameraManager =
		UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!InteractionIndicator || !LocalPawn || !CameraManager)
	{
		return;
	}

	if (IsFurnitureEquipmentKey(ItemKey))
	{
		InteractionIndicator->SetVisibility(false);
		return;
	}

	const FVector CameraToEquipment =
		GetActorLocation() - CameraManager->GetCameraLocation();
	const float CameraDistance = CameraToEquipment.Size();
	const bool bCanJoinHeavyCarry =
		IsWaitingForHelper() &&
		LocalPawn != Carrier;
	const bool bShowIndicator =
		(!IsValid(Carrier) || bCanJoinHeavyCarry) &&
		CanInteract_Implementation(LocalPawn) &&
		FVector::DistSquared(
			LocalPawn->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(500.0f) &&
		CameraDistance > KINDA_SMALL_NUMBER &&
		FVector::DotProduct(
			CameraManager->GetCameraRotation().Vector(),
			CameraToEquipment / CameraDistance) >=
			FMath::Cos(FMath::DegreesToRadians(22.0f));
	InteractionIndicator->SetText(
		FText::FromString(
			bCanJoinHeavyCarry
				? TEXT("[ E MAINTENU ] AIDER")
				: TEXT("[ E MAINTENU ] SOULEVER")));
	InteractionIndicator->SetVisibility(bShowIndicator);
	if (bShowIndicator)
	{
		InteractionIndicator->SetWorldRotation(
			(CameraManager->GetCameraLocation() -
			 InteractionIndicator->GetComponentLocation()).Rotation());
	}
}

void ABotanicusLargeEquipmentActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusLargeEquipmentActor, Carrier);
	DOREPLIFETIME(ABotanicusLargeEquipmentActor, Helper);
	DOREPLIFETIME(ABotanicusLargeEquipmentActor, ItemKey);
	DOREPLIFETIME(ABotanicusLargeEquipmentActor, bPlacementMode);
	DOREPLIFETIME(ABotanicusLargeEquipmentActor, bPlacementValid);
	DOREPLIFETIME(ABotanicusLargeEquipmentActor, bCooperativeCarry);
}

FBotanicusInteractionPrompt
ABotanicusLargeEquipmentActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.ActionText = IsValid(Carrier)
		? NSLOCTEXT("BotanicusDelivery", "DropEquipment", "Déposer")
		: NSLOCTEXT("BotanicusDelivery", "CarryEquipment", "Porter");
	Prompt.TargetName = InteractionName;
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	return Prompt;
}

bool ABotanicusLargeEquipmentActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	const ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	if (!Character)
	{
		return false;
	}

	if (Carrier == Character)
	{
		return true;
	}
	if (IsValid(Carrier))
	{
		return false;
	}

	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(
			 GetWorld());
		 EquipmentIt;
		 ++EquipmentIt)
	{
		if (EquipmentIt->Carrier == Character)
		{
			return false;
		}
	}
	return true;
}

void ABotanicusLargeEquipmentActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	if (!CanInteract_Implementation(Character))
	{
		return;
	}

	if (Carrier == Character)
	{
		Drop();
	}
	else
	{
		PickUp(Character);
	}
}

void ABotanicusLargeEquipmentActor::BeginFurnitureMove(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character) || IsValid(Carrier) ||
		!ABotanicusLargeEquipmentActor::CanInteract_Implementation(Character))
	{
		return;
	}
	PickUp(Character);
}

void ABotanicusLargeEquipmentActor::PickUp(
	ABotanicusCharacter* Character)
{
	PlacementOriginTransform = GetActorTransform();
	Carrier = Character;
	bPlacementMode = false;
	bPlacementValid = false;
	SetOwner(Character ? Character->GetController() : nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::BeginCooperativeHold(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() ||
		!bCooperativeCarry ||
		!IsValid(Character) ||
		bPlacementMode)
	{
		return;
	}

	if (!IsValid(Carrier))
	{
		PlacementOriginTransform = GetActorTransform();
		Carrier = Character;
		Helper = nullptr;
		SetOwner(Character->GetController());
	}
	else if (Carrier != Character && !IsValid(Helper))
	{
		Helper = Character;
		bPlacementMode = true;
		bPlacementValid = false;
		ApplyCarrierMovementPenalty();
	}

	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::EndCooperativeHold(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() ||
		!bCooperativeCarry ||
		!IsValid(Character) ||
		(Character != Carrier && Character != Helper))
	{
		return;
	}

	RestoreCarrierMovement();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorTransform(
		PlacementOriginTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Carrier = nullptr;
	Helper = nullptr;
	bPlacementMode = false;
	bPlacementValid = false;
	SetOwner(nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::Drop()
{
	if (!Carrier)
	{
		return;
	}

	RestoreCarrierMovement();
	const FVector DropLocation =
		Carrier->GetActorLocation() +
		Carrier->GetActorForwardVector() * 150.0f +
		FVector(0.0f, 0.0f, 65.0f);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(
		DropLocation,
		FRotator(0.0f, Carrier->GetActorRotation().Yaw, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Carrier = nullptr;
	bPlacementMode = false;
	bPlacementValid = false;
	SetOwner(nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::ApplyCarryState()
{
	const bool bIsCarried =
		IsValid(Carrier) &&
		!bPlacementMode &&
		!bCooperativeCarry;
	const bool bHasPlacementReservation =
		bPlacementMode || bIsCarried;
	SetActorEnableCollision(!bHasPlacementReservation);
	Mesh->SetCollisionEnabled(
		bHasPlacementReservation
			? ECollisionEnabled::NoCollision
			: ECollisionEnabled::QueryAndPhysics);

	if (bIsCarried)
	{
		AttachToActor(
			Carrier,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(FVector(115.0f, 0.0f, 70.0f));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else if (GetAttachParentActor())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	UMaterialInterface* PreviewMaterial = nullptr;
	if (bPlacementMode)
	{
		PreviewMaterial = bPlacementValid
			? ValidPlacementMaterial
			: InvalidPlacementMaterial;
		ApplyPlacementMaterial(PreviewMaterial);
	}
	else
	{
		RestorePlacementMaterials();
	}
	TInlineComponentArray<UMeshComponent*> MeshComponents(this);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->SetOverlayMaterial(nullptr);
			MeshComponent->SetRenderCustomDepth(bPlacementMode);
			MeshComponent->SetCustomDepthStencilValue(
				bPlacementValid ? 1 : 2);
		}
	}

	if (InteractionIndicator)
	{
		InteractionIndicator->SetVisibility(false);
	}
}

void ABotanicusLargeEquipmentActor::ApplyPlacementMaterial(
	UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}
	if (PreviewMaterialMeshes.IsEmpty())
	{
		TInlineComponentArray<UMeshComponent*> MeshComponents(this);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent)
			{
				continue;
			}
			const int32 MaterialCount = MeshComponent->GetNumMaterials();
			PreviewMaterialMeshes.Add(MeshComponent);
			PreviewMaterialCounts.Add(MaterialCount);
			for (int32 Index = 0; Index < MaterialCount; ++Index)
			{
				PreviewOriginalMaterials.Add(MeshComponent->GetMaterial(Index));
			}
		}
	}
	for (int32 MeshIndex = 0; MeshIndex < PreviewMaterialMeshes.Num(); ++MeshIndex)
	{
		if (UMeshComponent* MeshComponent = PreviewMaterialMeshes[MeshIndex])
		{
			for (int32 Index = 0; Index < PreviewMaterialCounts[MeshIndex]; ++Index)
			{
				MeshComponent->SetMaterial(Index, Material);
			}
			MeshComponent->SetOverlayMaterial(nullptr);
		}
	}
}

void ABotanicusLargeEquipmentActor::RestorePlacementMaterials()
{
	int32 MaterialOffset = 0;
	for (int32 MeshIndex = 0; MeshIndex < PreviewMaterialMeshes.Num(); ++MeshIndex)
	{
		const int32 MaterialCount = PreviewMaterialCounts.IsValidIndex(MeshIndex)
			? PreviewMaterialCounts[MeshIndex]
			: 0;
		if (UMeshComponent* MeshComponent = PreviewMaterialMeshes[MeshIndex])
		{
			for (int32 Index = 0; Index < MaterialCount; ++Index)
			{
				if (PreviewOriginalMaterials.IsValidIndex(MaterialOffset + Index))
				{
					MeshComponent->SetMaterial(Index, PreviewOriginalMaterials[MaterialOffset + Index]);
				}
			}
			MeshComponent->SetOverlayMaterial(nullptr);
		}
		MaterialOffset += MaterialCount;
	}
	PreviewMaterialMeshes.Reset();
	PreviewOriginalMaterials.Reset();
	PreviewMaterialCounts.Reset();
}

void ABotanicusLargeEquipmentActor::OnRep_Carrier()
{
	ApplyCarryState();
}

void ABotanicusLargeEquipmentActor::OnRep_PlacementState()
{
	ApplyCarryState();
}

void ABotanicusLargeEquipmentActor::InitializeEquipment(
	FName InItemKey)
{
	if (!InItemKey.IsNone())
	{
		ItemKey = InItemKey;
	}
	ApplyItemDefinition();
	OnEquipmentDefinitionApplied();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::OnRep_ItemKey()
{
	ApplyItemDefinition();
	OnEquipmentDefinitionApplied();
}

void ABotanicusLargeEquipmentActor::ApplyItemDefinition()
{
	UStaticMesh* ResolvedMesh = nullptr;
	bUsingItemDataMesh = false;

	if (!UsesBlueprintAppearance() &&
		IsFurnitureEquipmentKey(ItemKey))
	{
		const UItemDataAsset* ItemData =
			UItemDataSubsystem::Get(this).GetItemDataAsset(ItemKey);
		if (ItemData)
		{
			ResolvedMesh =
				ItemData->GetItemStaticMesh().LoadSynchronous();
			bUsingItemDataMesh = IsValid(ResolvedMesh);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Definition)
	{
		return;
	}

	if (!UsesBlueprintAppearance() && !ResolvedMesh)
	{
		ResolvedMesh = Definition->WorldMesh.LoadSynchronous();
	}
	if (!UsesBlueprintAppearance() && ResolvedMesh)
	{
		Mesh->SetStaticMesh(ResolvedMesh);
	}
	if (!UsesBlueprintAppearance())
	{
		Mesh->SetRelativeScale3D(
			bUsingItemDataMesh
				? FVector::OneVector
				: Definition->WorldScale);
	}
	InteractionName = Definition->DisplayName;
	bCooperativeCarry =
		Definition->WeightClass ==
			EBotanicusItemWeightClass::TwoPlayerCarry;
	CarryMovementSpeedMultiplier =
		FMath::Clamp(
			Definition->CarryMovementSpeedMultiplier,
			0.1f,
			1.0f);
	ApplyCarryState();
}

void ABotanicusLargeEquipmentActor::ApplyCarrierMovementPenalty()
{
	if (!HasAuthority())
	{
		return;
	}
	if (IsValid(Carrier))
	{
		Carrier->SetEquipmentCarryState(
			bCooperativeCarry
				? EBotanicusEquipmentCarryRole::Primary
				: EBotanicusEquipmentCarryRole::Solo,
			CarryMovementSpeedMultiplier);
	}
	if (IsValid(Helper))
	{
		Helper->SetEquipmentCarryState(
			EBotanicusEquipmentCarryRole::Helper,
			CarryMovementSpeedMultiplier);
	}
}

void ABotanicusLargeEquipmentActor::RestoreCarrierMovement()
{
	if (!HasAuthority())
	{
		return;
	}
	if (IsValid(Carrier))
	{
		Carrier->SetEquipmentCarryState(
			EBotanicusEquipmentCarryRole::None,
			1.0f);
	}
	if (IsValid(Helper))
	{
		Helper->SetEquipmentCarryState(
			EBotanicusEquipmentCarryRole::None,
			1.0f);
	}
}

FVector ABotanicusLargeEquipmentActor::GetPlacementBoxExtent() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return FVector(50.0f);
	}

	return Mesh->GetStaticMesh()->GetBounds().BoxExtent *
		Mesh->GetComponentScale().GetAbs();
}

void ABotanicusLargeEquipmentActor::BeginPlacement(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() || Carrier != Character)
	{
		return;
	}

	bPlacementMode = true;
	bPlacementValid = false;
	ApplyCarrierMovementPenalty();
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::UpdatePlacement(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	if (!HasAuthority() || !bPlacementMode || !IsValid(Carrier))
	{
		return;
	}

	bPlacementValid = bIsValid;
	SetActorTransform(
		PlacementTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::ConfirmPlacement()
{
	if (!HasAuthority() || !bPlacementMode || !bPlacementValid)
	{
		return;
	}

	RestoreCarrierMovement();
	Carrier = nullptr;
	Helper = nullptr;
	bPlacementMode = false;
	bPlacementValid = false;
	SetOwner(nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::CancelPlacement()
{
	if (!HasAuthority() || !bPlacementMode || !IsValid(Carrier))
	{
		return;
	}

	RestoreCarrierMovement();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorTransform(
		PlacementOriginTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Carrier = nullptr;
	Helper = nullptr;
	bPlacementMode = false;
	bPlacementValid = false;
	SetOwner(nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::SetLocalPlacementPreview(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	if (HasAuthority())
	{
		return;
	}

	bPlacementMode = true;
	bPlacementValid = bIsValid;
	ApplyCarryState();
	SetActorTransform(
		PlacementTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}
