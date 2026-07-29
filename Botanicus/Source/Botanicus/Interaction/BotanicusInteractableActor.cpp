// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/BotanicusInteractableActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABotanicusInteractableActor::ABotanicusInteractableActor()
{
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ABotanicusInteractableActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusInteractableActor, bInteractionEnabled);
}

FBotanicusInteractionPrompt ABotanicusInteractableActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.ActionText = InteractionAction;
	Prompt.TargetName = InteractionName;
	Prompt.bCanInteract = bInteractionEnabled;
	return Prompt;
}

bool ABotanicusInteractableActor::CanInteract_Implementation(AActor* Interactor) const
{
	return bInteractionEnabled;
}

void ABotanicusInteractableActor::Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority() && bInteractionEnabled && IsValid(Interactor))
	{
		ReceiveInteraction(Interactor);
	}
}

void ABotanicusInteractableActor::SetInteractionEnabled(bool bEnabled)
{
	if (!HasAuthority() || bInteractionEnabled == bEnabled)
	{
		return;
	}

	bInteractionEnabled = bEnabled;
	OnRep_InteractionEnabled();
	ForceNetUpdate();
}

void ABotanicusInteractableActor::OnRep_InteractionEnabled()
{
	ReceiveInteractionEnabledChanged(bInteractionEnabled);
}
