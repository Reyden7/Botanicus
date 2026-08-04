// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusWateringCanActor.h"

#include "BotanicusCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABotanicusWateringCanActor::ABotanicusWateringCanActor()
{
	PrimaryActorTick.bCanEverTick = false;
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
	FVector DropLocation =
		PreviousCarrier->GetActorLocation() +
		PreviousCarrier->GetActorForwardVector() * 85.0f;
	const float PlacementHalfHeight =
		FMath::Max(2.0f, GetPlacementBoxExtent().GetAbs().Z);

	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusWateringCanDropFloor),
		false);
	FloorQuery.AddIgnoredActor(PreviousCarrier);
	FloorQuery.AddIgnoredActor(this);
	const FVector TraceStart(
		DropLocation.X,
		DropLocation.Y,
		PreviousCarrier->GetActorLocation().Z + 160.0f);
	const FVector TraceEnd(
		DropLocation.X,
		DropLocation.Y,
		PreviousCarrier->GetActorLocation().Z - 500.0f);
	FHitResult FloorHit;
	if (GetWorld()->LineTraceSingleByChannel(
			FloorHit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			FloorQuery) &&
		FloorHit.ImpactNormal.Z >= 0.7f)
	{
		DropLocation.Z =
			FloorHit.ImpactPoint.Z + PlacementHalfHeight + 2.0f;
	}
	else
	{
		// Safe fallback for unusual levels without a visible floor beneath
		// the player.
		DropLocation.Z =
			PreviousCarrier->GetActorLocation().Z -
			88.0f +
			PlacementHalfHeight +
			2.0f;
	}

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
	ForceNetUpdate();
	return true;
}

bool ABotanicusWateringCanActor::AddWater(float Amount)
{
	if (!HasAuthority() || Amount <= 0.0f || IsFull())
	{
		return false;
	}
	WaterLevel = FMath::Clamp(WaterLevel + Amount, 0.0f, 1.0f);
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
	ForceNetUpdate();
}

void ABotanicusWateringCanActor::RestoreWaterLevel(float InWaterLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	WaterLevel = FMath::Clamp(InWaterLevel, 0.0f, 1.0f);
	ForceNetUpdate();
}

void ABotanicusWateringCanActor::OnRep_Carrier()
{
	ApplyCarrierState();
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
