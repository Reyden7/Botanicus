// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusComputerActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Camera/CameraComponent.h"

ABotanicusComputerActor::ABotanicusComputerActor()
{
	// Computers are always authored through BP_Item_CommandComputer. Keep its
	// meshes, materials and component transforms instead of applying the native
	// catalogue's primitive fallback at runtime.
	bUseBlueprintAppearance = true;

	InteractionAction =
		NSLOCTEXT(
			"BotanicusComputer",
			"UseComputer",
			"Utiliser");

	InteractionName =
		NSLOCTEXT(
			"BotanicusComputer",
			"ComputerName",
			"Ordinateur de commande");

	// Le composant Mesh est fourni par ABotanicusPlaceableItemActor.
	// Le véritable mesh du PC sera choisi dans le Blueprint.
	Mesh->SetRelativeLocation(FVector::ZeroVector);
	Mesh->SetRelativeRotation(FRotator::ZeroRotator);
	Mesh->SetRelativeScale3D(FVector::OneVector);

	InteractionCamera = CreateDefaultSubobject<UCameraComponent>(
		TEXT("Computer Interaction Camera"));
	InteractionCamera->SetupAttachment(SceneRoot);
	// The default assumes that the front of the computer faces +X. Designers
	// can fine-tune this inherited component in BP_Item_CommandComputer.
	InteractionCamera->SetRelativeLocation(FVector(85.0f, 0.0f, 72.0f));
	InteractionCamera->SetRelativeRotation(FRotator(-7.0f, 180.0f, 0.0f));
	InteractionCamera->SetFieldOfView(48.0f);
	InteractionCamera->bUsePawnControlRotation = false;
}

FBotanicusInteractionPrompt
ABotanicusComputerActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;

	Prompt.ActionText =
		NSLOCTEXT(
			"BotanicusComputer",
			"ComputerPrompt",
			"Appuyer sur E pour ouvrir le panneau");

	Prompt.TargetName = InteractionName;

	Prompt.bCanInteract =
		CanInteract_Implementation(Interactor);

	return Prompt;
}

bool ABotanicusComputerActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return bInteractionEnabled &&
		!ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
		IsValid(Cast<ABotanicusCharacter>(Interactor));
}

void ABotanicusComputerActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!HasAuthority() ||
		!CanInteract_Implementation(Interactor))
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);

	if (ABotanicusPlayerController* Controller =
		Character
		? Cast<ABotanicusPlayerController>(
			Character->GetController())
		: nullptr)
	{
		Controller->ClientOpenOrderCatalogFromComputer(this);
	}
}
