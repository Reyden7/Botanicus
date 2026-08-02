// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusWaterReserveActor.h"

#include "BotanicusCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusWateringCanActor.h"

ABotanicusWaterReserveActor::ABotanicusWaterReserveActor()
{
	PrimaryActorTick.bCanEverTick = true;
	ReserveText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Reserve Label"));
	ReserveText->SetupAttachment(SceneRoot);
	ReserveText->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
	ReserveText->SetHorizontalAlignment(EHTA_Center);
	ReserveText->SetVerticalAlignment(EVRTA_TextCenter);
	ReserveText->SetWorldSize(17.0f);
	ReserveText->SetTextRenderColor(FColor(80, 255, 110));
	ReserveText->SetText(
		NSLOCTEXT(
			"BotanicusGrowing",
			"WaterReserveLabel",
			"RESERVE D'EAU\nCLIC GAUCHE : REMPLIR L'ARROSOIR"));
	ReserveText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABotanicusWaterReserveActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	if (ReserveText && Controller && Controller->PlayerCameraManager)
	{
		ReserveText->SetWorldRotation(
			(Controller->PlayerCameraManager->GetCameraLocation() -
			 ReserveText->GetComponentLocation()).Rotation());
	}
}

void ABotanicusWaterReserveActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (ReserveText)
	{
		ReserveText->SetVisibility(false);
	}
}

bool ABotanicusWaterReserveActor::TryRefill(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character))
	{
		return false;
	}
	ABotanicusWateringCanActor* WateringCan =
		Character->GetHeldWateringCan();
	if (!IsValid(WateringCan) || WateringCan->IsFull())
	{
		return false;
	}
	WateringCan->Refill();
	return true;
}
