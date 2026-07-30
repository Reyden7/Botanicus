// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryParcelActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusDeliveryParcelActor::ABotanicusDeliveryParcelActor()
{
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;

	InteractionAction =
		NSLOCTEXT("BotanicusDelivery", "CollectParcel", "Récupérer");
	InteractionName =
		NSLOCTEXT("BotanicusDelivery", "SmallParcel", "Colis livré");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.55f, 0.45f, 0.35f));

	InteractionIndicator =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("Interaction Indicator"));
	InteractionIndicator->SetupAttachment(SceneRoot);
	InteractionIndicator->SetRelativeLocation(
		FVector(0.0f, 0.0f, 105.0f));
	InteractionIndicator->SetText(FText::FromString(TEXT("[ E ]")));
	InteractionIndicator->SetTextRenderColor(FColor(255, 210, 30));
	InteractionIndicator->SetHorizontalAlignment(EHTA_Center);
	InteractionIndicator->SetVerticalAlignment(EVRTA_TextCenter);
	InteractionIndicator->SetWorldSize(38.0f);
	InteractionIndicator->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
}

void ABotanicusDeliveryParcelActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, ItemKey);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, Quantity);
}

void ABotanicusDeliveryParcelActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	APlayerCameraManager* CameraManager =
		UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!InteractionIndicator || !LocalPawn || !CameraManager)
	{
		return;
	}

	const FVector CameraToParcel =
		GetActorLocation() - CameraManager->GetCameraLocation();
	const float CameraDistance = CameraToParcel.Size();
	const bool bPlayerCanSeePrompt =
		FVector::DistSquared(
			LocalPawn->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(450.0f) &&
		CameraDistance > KINDA_SMALL_NUMBER &&
		FVector::DotProduct(
			CameraManager->GetCameraRotation().Vector(),
			CameraToParcel / CameraDistance) >=
			FMath::Cos(FMath::DegreesToRadians(22.0f));
	InteractionIndicator->SetVisibility(bPlayerCanSeePrompt);
	if (bPlayerCanSeePrompt)
	{
		InteractionIndicator->SetWorldRotation(
			(CameraManager->GetCameraLocation() -
			 InteractionIndicator->GetComponentLocation()).Rotation());
	}
}

void ABotanicusDeliveryParcelActor::InitializeParcel(
	FName InItemKey,
	int32 InQuantity)
{
	if (!HasAuthority())
	{
		return;
	}

	ItemKey = InItemKey;
	Quantity = FMath::Max(1, InQuantity);
	ForceNetUpdate();
}

void ABotanicusDeliveryParcelActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!HasAuthority() || !bInteractionEnabled)
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	int32 AddedSlotIndex = INDEX_NONE;
	if (!QuickBar ||
		!QuickBar->AddItem(ItemKey, Quantity, AddedSlotIndex))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Delivery parcel collection failed for %s (QuickBar=%s, ItemKey=%s, Quantity=%d)."),
			*GetNameSafe(Character),
			*GetNameSafe(QuickBar),
			*ItemKey.ToString(),
			Quantity);
		if (ABotanicusPlayerController* Controller =
				Character
					? Cast<ABotanicusPlayerController>(
						Character->GetController())
					: nullptr)
		{
			Controller->ClientMessage(
				TEXT("Hotbar pleine : impossible de récupérer ce colis."));
		}
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Player %s collected delivery parcel %s into hotbar slot %d."),
		*GetNameSafe(Character),
		*GetName(),
		AddedSlotIndex + 1);
	SetInteractionEnabled(false);
	Destroy();
}
