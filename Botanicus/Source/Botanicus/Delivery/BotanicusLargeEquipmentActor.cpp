// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusLargeEquipmentActor.h"

#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
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
	const bool bShowIndicator =
		!IsValid(Carrier) &&
		FVector::DistSquared(
			LocalPawn->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(500.0f) &&
		CameraDistance > KINDA_SMALL_NUMBER &&
		FVector::DotProduct(
			CameraManager->GetCameraRotation().Vector(),
			CameraToEquipment / CameraDistance) >=
			FMath::Cos(FMath::DegreesToRadians(22.0f));
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
	Carrier = Character;
	SetOwner(Character ? Character->GetController() : nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::Drop()
{
	if (!Carrier)
	{
		return;
	}

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
	SetOwner(nullptr);
	ApplyCarryState();
	ForceNetUpdate();
}

void ABotanicusLargeEquipmentActor::ApplyCarryState()
{
	const bool bIsCarried = IsValid(Carrier);
	SetActorEnableCollision(!bIsCarried);
	Mesh->SetCollisionEnabled(
		bIsCarried
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

	if (InteractionIndicator)
	{
		InteractionIndicator->SetVisibility(false);
	}
}

void ABotanicusLargeEquipmentActor::OnRep_Carrier()
{
	ApplyCarryState();
}
