// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/BotanicusInteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UBotanicusInteractionComponent::UBotanicusInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	SetIsReplicatedByDefault(true);

	CurrentPrompt.bCanInteract = false;
}

void UBotanicusInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBotanicusInteractionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Possession can happen after BeginPlay in a network game, so local control is
	// evaluated continuously instead of permanently disabling this component.
	if (!IsLocallyControlledOwner())
	{
		return;
	}

	RefreshFocus();
}

AActor* UBotanicusInteractionComponent::GetFocusedActor() const
{
	return FocusedActor;
}

FBotanicusInteractionPrompt UBotanicusInteractionComponent::GetCurrentPrompt() const
{
	return CurrentPrompt;
}

void UBotanicusInteractionComponent::TryInteract()
{
	if (!IsLocallyControlledOwner())
	{
		return;
	}

	RefreshFocus();

	AActor* TargetActor = FocusedActor;
	if (!IsValid(TargetActor) || !CurrentPrompt.bCanInteract)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		if (IsServerInteractionValid(TargetActor))
		{
			IBotanicusInteractable::Execute_Interact(TargetActor, GetOwner());
		}
		return;
	}

	ServerTryInteract(TargetActor);
}

void UBotanicusInteractionComponent::ClearFocus()
{
	SetFocusedActor(nullptr);
}

void UBotanicusInteractionComponent::RefreshFocus()
{
	AActor* NewFocusedActor = nullptr;
	TraceForInteractable(NewFocusedActor);
	SetFocusedActor(NewFocusedActor);
}

bool UBotanicusInteractionComponent::TraceForInteractable(AActor*& OutActor) const
{
	OutActor = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * InteractionDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BotanicusInteraction), false, GetOwner());
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		TraceEnd,
		TraceChannel,
		QueryParams);

	if (bDrawDebugTrace)
	{
		const FVector DebugEnd = bHit ? Hit.ImpactPoint : TraceEnd;
		DrawDebugLine(
			World,
			ViewLocation,
			DebugEnd,
			bHit ? FColor::Green : FColor::Red,
			false,
			0.06f,
			0,
			1.5f);
	}

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (IsValid(HitActor) && HitActor->Implements<UBotanicusInteractable>())
	{
		OutActor = HitActor;
		return true;
	}

	return false;
}

bool UBotanicusInteractionComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return false;
	}

	if (AController* Controller = OwnerPawn->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}

	OutLocation = OwnerPawn->GetPawnViewLocation();
	OutRotation = OwnerPawn->GetViewRotation();
	return true;
}

bool UBotanicusInteractionComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

void UBotanicusInteractionComponent::SetFocusedActor(AActor* NewFocusedActor)
{
	FBotanicusInteractionPrompt NewPrompt;
	NewPrompt.bCanInteract = false;

	if (IsValid(NewFocusedActor) && NewFocusedActor->Implements<UBotanicusInteractable>())
	{
		NewPrompt = IBotanicusInteractable::Execute_GetInteractionPrompt(NewFocusedActor, GetOwner());
		NewPrompt.bCanInteract =
			NewPrompt.bCanInteract &&
			IBotanicusInteractable::Execute_CanInteract(NewFocusedActor, GetOwner());
	}
	else
	{
		NewFocusedActor = nullptr;
	}

	const bool bActorChanged = FocusedActor != NewFocusedActor;
	const bool bPromptChanged =
		!CurrentPrompt.ActionText.EqualTo(NewPrompt.ActionText) ||
		!CurrentPrompt.TargetName.EqualTo(NewPrompt.TargetName) ||
		CurrentPrompt.bCanInteract != NewPrompt.bCanInteract;

	if (!bActorChanged && !bPromptChanged)
	{
		return;
	}

	FocusedActor = NewFocusedActor;
	CurrentPrompt = NewPrompt;
	OnFocusChanged.Broadcast(FocusedActor, CurrentPrompt);
}

bool UBotanicusInteractionComponent::IsServerInteractionValid(AActor* TargetActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!IsValid(TargetActor) ||
		!TargetActor->Implements<UBotanicusInteractable>())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	const float AllowedDistance = InteractionDistance + 50.0f;
	const FVector TargetLocation = TargetActor->GetActorLocation();
	if (FVector::DistSquared(ViewLocation, TargetLocation) > FMath::Square(AllowedDistance))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BotanicusServerInteraction), false, OwnerActor);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		TargetLocation,
		TraceChannel,
		QueryParams);

	if (!bHit || Hit.GetActor() != TargetActor)
	{
		return false;
	}

	return IBotanicusInteractable::Execute_CanInteract(TargetActor, GetOwner());
}

void UBotanicusInteractionComponent::ServerTryInteract_Implementation(AActor* TargetActor)
{
	if (IsServerInteractionValid(TargetActor))
	{
		IBotanicusInteractable::Execute_Interact(TargetActor, GetOwner());
	}
}
