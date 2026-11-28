// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusComputerActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Preparation/BotanicusTerminalEyesComponent.h"

ABotanicusComputerActor::ABotanicusComputerActor()
{
	bUseBlueprintAppearance = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SceneRoot->SetMobility(EComponentMobility::Static);
	Mesh->SetMobility(EComponentMobility::Static);
	TerminalEyes = CreateDefaultSubobject<UBotanicusTerminalEyesComponent>(TEXT("Terminal Eyes"));

	InteractionAction =
		NSLOCTEXT(
			"BotanicusComputer",
			"UseComputer",
			"Utiliser");

	InteractionName =
		NSLOCTEXT(
			"BotanicusComputer",
			"FixedTerminalName",
			"Borne de commande");

	InteractionCamera = CreateDefaultSubobject<UCameraComponent>(
		TEXT("Computer Interaction Camera"));
	InteractionCamera->SetupAttachment(SceneRoot);
	// The default assumes that the front of the computer faces +X. Designers
	// can fine-tune this inherited component in BP_Item_CommandComputer.
	InteractionCamera->SetRelativeLocation(FVector(85.0f, 0.0f, 72.0f));
	InteractionCamera->SetRelativeRotation(FRotator(-7.0f, 180.0f, 0.0f));
	InteractionCamera->SetFieldOfView(48.0f);
	InteractionCamera->bUsePawnControlRotation = false;

	ScreenInteractionTarget = CreateDefaultSubobject<USceneComponent>(TEXT("ScreenInteractionTarget"));
	ScreenInteractionTarget->SetupAttachment(InteractionCamera);
	ScreenInteractionTarget->SetRelativeLocation(FVector(60.0, 0.0, 0.0));
	ScreenInteractionTarget->bEditableWhenInherited = true;
}

FVector ABotanicusComputerActor::GetScreenInteractionLocation() const
{
	return ScreenInteractionTarget->GetComponentLocation();
}

FName ABotanicusComputerActor::LegacyItemKey()
{
	static const FName Key(TEXT("CommandComputer"));
	return Key;
}

void ABotanicusComputerActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Old Blueprint and instance defaults may still carry the former furniture settings.
	SetReplicateMovement(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
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
		Controller->OpenOrderCatalogFromComputer(*this);
	}
}
