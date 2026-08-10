// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/BotanicusInteractableActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

	SkeletalMeshVisual = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("Skeletal Mesh Visual"));
	SkeletalMeshVisual->SetupAttachment(SceneRoot);
	SkeletalMeshVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshVisual->SetVisibility(false);
}

void ABotanicusInteractableActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const bool bShowSkeletalMesh =
		UsesSkeletalMeshAppearance() &&
		SkeletalMeshVisual &&
		SkeletalMeshVisual->GetSkeletalMeshAsset();
	if (Mesh)
	{
		// The static component remains registered and can still serve as the
		// placement/collision proxy for an animated appearance.
		Mesh->SetVisibility(!bShowSkeletalMesh, true);
	}
	if (SkeletalMeshVisual)
	{
		SkeletalMeshVisual->SetVisibility(bShowSkeletalMesh, true);
	}
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
