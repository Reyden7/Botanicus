// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusWateringCanActor.h"

#include "BotanicusCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ABotanicusWateringCanActor::ABotanicusWateringCanActor()
{
	PrimaryActorTick.bCanEverTick = true;
	WaterLevelText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Water Level"));
	WaterLevelText->SetupAttachment(SceneRoot);
	WaterLevelText->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	WaterLevelText->SetHorizontalAlignment(EHTA_Center);
	WaterLevelText->SetVerticalAlignment(EVRTA_TextCenter);
	WaterLevelText->SetWorldSize(18.0f);
	WaterLevelText->SetTextRenderColor(FColor(80, 220, 255));
	WaterLevelText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshWaterDisplay();
}

void ABotanicusWateringCanActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	if (WaterLevelText && Controller && Controller->PlayerCameraManager)
	{
		WaterLevelText->SetWorldRotation(
			(Controller->PlayerCameraManager->GetCameraLocation() -
			 WaterLevelText->GetComponentLocation()).Rotation());
	}
}

void ABotanicusWateringCanActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusWateringCanActor, Carrier);
	DOREPLIFETIME(ABotanicusWateringCanActor, WaterLevel);
}

void ABotanicusWateringCanActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (WaterLevelText)
	{
		WaterLevelText->SetVisibility(false);
	}
}

bool ABotanicusWateringCanActor::TryPickUp(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character) ||
		IsValid(Carrier) || IsValid(Character->GetHeldWateringCan()))
	{
		return false;
	}

	Carrier = Character;
	Character->SetHeldWateringCan(this);
	SetOwner(Character->GetController());
	ApplyCarrierState();
	ForceNetUpdate();
	return true;
}

void ABotanicusWateringCanActor::Drop()
{
	if (!HasAuthority() || !IsValid(Carrier))
	{
		return;
	}

	ABotanicusCharacter* PreviousCarrier = Carrier;
	const FVector DropLocation =
		PreviousCarrier->GetActorLocation() +
		PreviousCarrier->GetActorForwardVector() * 85.0f +
		FVector(0.0f, 0.0f, 25.0f);
	const FRotator DropRotation(0.0f, PreviousCarrier->GetActorRotation().Yaw, 0.0f);
	PreviousCarrier->SetHeldWateringCan(nullptr);
	Carrier = nullptr;
	SetOwner(nullptr);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(
		DropLocation,
		DropRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ApplyCarrierState();
	ForceNetUpdate();
}

bool ABotanicusWateringCanActor::ConsumeWater(float Amount)
{
	if (!HasAuthority() || Amount <= 0.0f || !HasWater())
	{
		return false;
	}
	WaterLevel = FMath::Clamp(WaterLevel - Amount, 0.0f, 1.0f);
	RefreshWaterDisplay();
	ForceNetUpdate();
	return true;
}

void ABotanicusWateringCanActor::Refill()
{
	if (!HasAuthority())
	{
		return;
	}
	WaterLevel = 1.0f;
	RefreshWaterDisplay();
	ForceNetUpdate();
}

void ABotanicusWateringCanActor::RestoreWaterLevel(float InWaterLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	WaterLevel = FMath::Clamp(InWaterLevel, 0.0f, 1.0f);
	RefreshWaterDisplay();
	ForceNetUpdate();
}

void ABotanicusWateringCanActor::OnRep_Carrier()
{
	ApplyCarrierState();
}

void ABotanicusWateringCanActor::OnRep_WaterLevel()
{
	RefreshWaterDisplay();
}

void ABotanicusWateringCanActor::ApplyCarrierState()
{
	if (IsValid(Carrier))
	{
		AttachToComponent(
			Carrier->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			TEXT("hand_r"));
		SetActorRelativeLocation(FVector(5.0f, 2.0f, -4.0f));
		SetActorRelativeRotation(FRotator(15.0f, -85.0f, 15.0f));
		SetActorEnableCollision(false);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorEnableCollision(true);
	}
}

void ABotanicusWateringCanActor::RefreshWaterDisplay()
{
	if (!WaterLevelText)
	{
		return;
	}
	WaterLevelText->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("EAU : %d%%"),
				FMath::RoundToInt(WaterLevel * 100.0f))));
	WaterLevelText->SetTextRenderColor(
		WaterLevel > 0.2f
			? FColor(80, 220, 255)
			: FColor(255, 120, 80));
}
