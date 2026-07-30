// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusLargeEquipmentActor.h"

#include "BotanicusCharacter.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

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
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_Can_Build.MI_Can_Build"));
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

	const FVector CameraToEquipment =
		GetActorLocation() - CameraManager->GetCameraLocation();
	const float CameraDistance = CameraToEquipment.Size();
	const bool bCanJoinHeavyCarry =
		IsWaitingForHelper() &&
		LocalPawn != Carrier;
	const bool bShowIndicator =
		(!IsValid(Carrier) || bCanJoinHeavyCarry) &&
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

	Mesh->SetOverlayMaterial(
		bPlacementMode
			? (bPlacementValid
				   ? ValidPlacementMaterial
				   : InvalidPlacementMaterial)
			: nullptr);

	if (InteractionIndicator)
	{
		InteractionIndicator->SetVisibility(false);
	}
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
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::OnRep_ItemKey()
{
	ApplyItemDefinition();
}

void ABotanicusLargeEquipmentActor::ApplyItemDefinition()
{
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

	if (UStaticMesh* DefinitionMesh =
		Definition->WorldMesh.LoadSynchronous())
	{
		Mesh->SetStaticMesh(DefinitionMesh);
	}
	Mesh->SetRelativeScale3D(Definition->WorldScale);
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
