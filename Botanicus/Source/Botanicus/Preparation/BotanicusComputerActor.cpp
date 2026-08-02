// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusComputerActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusComputerActor::ABotanicusComputerActor()
{
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube =
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	if (Cube)
	{
		Mesh->SetStaticMesh(Cube);
	}
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	Mesh->SetRelativeScale3D(FVector(0.48f, 0.10f, 0.32f));

	ScreenFrame = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Computer Screen Frame"));
	ScreenFrame->SetupAttachment(SceneRoot);
	ScreenFrame->SetStaticMesh(Cube);
	ScreenFrame->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	ScreenFrame->SetRelativeScale3D(FVector(0.50f, 0.12f, 0.34f));
	ScreenFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ScreenSurface = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Computer Screen Surface"));
	ScreenSurface->SetupAttachment(SceneRoot);
	ScreenSurface->SetStaticMesh(Cube);
	ScreenSurface->SetRelativeLocation(FVector(0.0f, -12.5f, 10.0f));
	ScreenSurface->SetRelativeScale3D(
		FVector(0.42f, 0.015f, 0.26f));
	ScreenSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UMaterialInstanceDynamic* ScreenMaterial =
			ScreenSurface->CreateAndSetMaterialInstanceDynamic(0))
	{
		ScreenMaterial->SetVectorParameterValue(
			TEXT("Color"),
			FLinearColor(0.02f, 0.55f, 0.42f, 1.0f));
	}

	Stand = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Computer Stand"));
	Stand->SetupAttachment(SceneRoot);
	Stand->SetStaticMesh(Cube);
	Stand->SetRelativeLocation(FVector(0.0f, 0.0f, -22.0f));
	Stand->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.18f));
	Stand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Keyboard = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Computer Keyboard"));
	Keyboard->SetupAttachment(SceneRoot);
	Keyboard->SetStaticMesh(Cube);
	Keyboard->SetRelativeLocation(FVector(25.0f, -5.0f, -38.0f));
	Keyboard->SetRelativeScale3D(FVector(0.28f, 0.18f, 0.025f));
	Keyboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
		Controller->ClientOpenOrderCatalogFromComputer();
	}
}
