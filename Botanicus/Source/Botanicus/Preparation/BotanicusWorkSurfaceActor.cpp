// Copyright Epic Games, Inc. All Rights Reserved.

#include "Preparation/BotanicusWorkSurfaceActor.h"

#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

bool ABotanicusWorkSurfaceActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return (!HasSurfaceContents() || bMoveContentsWithFurniture) &&
		Super::CanInteract_Implementation(Interactor);
}

void ABotanicusWorkSurfaceActor::BeginPlacement(
	ABotanicusCharacter* Character)
{
	if (bMoveContentsWithFurniture &&
		MovingSurfaceContents.IsEmpty())
	{
		CaptureSurfaceContentTransforms();
	}
	Super::BeginPlacement(Character);
	ApplySurfaceContentTransforms();
}

void ABotanicusWorkSurfaceActor::UpdatePlacement(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	Super::UpdatePlacement(PlacementTransform, bIsValid);
	ApplySurfaceContentTransforms();
}

void ABotanicusWorkSurfaceActor::ConfirmPlacement()
{
	Super::ConfirmPlacement();
	ApplySurfaceContentTransforms();
	ClearSurfaceContentTransforms();
	bMoveContentsWithFurniture = false;
}

void ABotanicusWorkSurfaceActor::CancelPlacement()
{
	Super::CancelPlacement();
	ApplySurfaceContentTransforms();
	ClearSurfaceContentTransforms();
	bMoveContentsWithFurniture = false;
}

void ABotanicusWorkSurfaceActor::SetLocalPlacementPreview(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	if (bMoveContentsWithFurniture &&
		MovingSurfaceContents.IsEmpty())
	{
		CaptureSurfaceContentTransforms();
	}
	Super::SetLocalPlacementPreview(PlacementTransform, bIsValid);
	ApplySurfaceContentTransforms();
}

bool ABotanicusWorkSurfaceActor::IsCatalogItemCompatible(
	const UObject* Context,
	FName InItemKey)
{
	const UWorld* World = Context ? Context->GetWorld() : nullptr;
	const UGameInstance* GameInstance =
		World ? World->GetGameInstance() : nullptr;
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(InItemKey) : nullptr;
	return Definition &&
		Definition->WeightClass ==
			EBotanicusItemWeightClass::Hotbar &&
		Definition->Category != EBotanicusItemCategory::Equipment &&
		InItemKey != TEXT("PottingSoil") &&
		InItemKey != TEXT("SalesDisplay");
}

bool ABotanicusWorkSurfaceActor::GetFreePlacementTransform(
	FName InItemKey,
	const FVector& DesiredWorldLocation,
	float DesiredYaw,
	const FVector& ItemExtent,
	FTransform& OutTransform) const
{
	if (!IsCatalogItemCompatible(this, InItemKey))
	{
		return false;
	}

	const FVector SurfaceExtent = GetPlacementBoxExtent().GetAbs();
	const FVector SafeItemExtent = ItemExtent.GetAbs();
	const FVector LocalPoint =
		GetActorTransform().InverseTransformPosition(
			DesiredWorldLocation);
	const float AvailableX =
		SurfaceExtent.X - SafeItemExtent.X - 3.0f;
	const float AvailableY =
		SurfaceExtent.Y - SafeItemExtent.Y - 3.0f;
	if (AvailableX <= 0.0f ||
		AvailableY <= 0.0f ||
		FMath::Abs(LocalPoint.X) > AvailableX ||
		FMath::Abs(LocalPoint.Y) > AvailableY)
	{
		return false;
	}

	const FVector ClampedLocalPoint(
		FMath::Clamp(LocalPoint.X, -AvailableX, AvailableX),
		FMath::Clamp(LocalPoint.Y, -AvailableY, AvailableY),
		SurfaceExtent.Z + SafeItemExtent.Z + 3.0f);
	OutTransform = FTransform(
		FRotator(0.0f, DesiredYaw, 0.0f),
		GetActorTransform().TransformPosition(
			ClampedLocalPoint));
	return true;
}

bool ABotanicusWorkSurfaceActor::IsActorRestingOnSurface(
	const AActor* Actor,
	const FVector& ActorExtent) const
{
	if (!IsValid(Actor) || Actor == this ||
		Actor->ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		return false;
	}
	const FVector SurfaceExtent = GetPlacementBoxExtent().GetAbs();
	const FVector SafeActorExtent = ActorExtent.GetAbs();
	const FVector LocalPoint =
		GetActorTransform().InverseTransformPosition(
			Actor->GetActorLocation());
	const float ExpectedCenterZ =
		SurfaceExtent.Z + SafeActorExtent.Z + 3.0f;
	return FMath::Abs(LocalPoint.X) <=
			SurfaceExtent.X - FMath::Min(SafeActorExtent.X, 8.0f) &&
		FMath::Abs(LocalPoint.Y) <=
			SurfaceExtent.Y - FMath::Min(SafeActorExtent.Y, 8.0f) &&
		FMath::Abs(LocalPoint.Z - ExpectedCenterZ) <= 18.0f;
}

void ABotanicusWorkSurfaceActor::GetSurfaceContents(
	TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ABotanicusPlaceableItemActor> It(GetWorld());
		 It;
		 ++It)
	{
		if (IsActorRestingOnSurface(
				*It,
				It->GetPlacementBoxExtent()))
		{
			OutActors.AddUnique(*It);
		}
	}
	for (TActorIterator<ABotanicusDeliveryParcelActor> It(GetWorld());
		 It;
		 ++It)
	{
		if (IsActorRestingOnSurface(
				*It,
				It->GetParcelHalfExtent()))
		{
			OutActors.AddUnique(*It);
		}
	}
}

bool ABotanicusWorkSurfaceActor::HasSurfaceContents() const
{
	TArray<AActor*> Contents;
	GetSurfaceContents(Contents);
	return !Contents.IsEmpty();
}

bool ABotanicusWorkSurfaceActor::ContainsSurfaceActor(
	const AActor* Actor) const
{
	if (const ABotanicusPlaceableItemActor* Placeable =
		Cast<ABotanicusPlaceableItemActor>(Actor))
	{
		return IsActorRestingOnSurface(
			Placeable,
			Placeable->GetPlacementBoxExtent());
	}
	if (const ABotanicusDeliveryParcelActor* Parcel =
		Cast<ABotanicusDeliveryParcelActor>(Actor))
	{
		return IsActorRestingOnSurface(
			Parcel,
			Parcel->GetParcelHalfExtent());
	}
	return false;
}

void ABotanicusWorkSurfaceActor::SetMoveContentsWithFurniture(
	bool bEnabled)
{
	bMoveContentsWithFurniture = bEnabled;
	if (bEnabled)
	{
		CaptureSurfaceContentTransforms();
	}
	else
	{
		ClearSurfaceContentTransforms();
	}
}

void ABotanicusWorkSurfaceActor::CaptureSurfaceContentTransforms()
{
	MovingSurfaceContents.Reset();
	MovingSurfaceContentRelativeTransforms.Reset();
	TArray<AActor*> Contents;
	GetSurfaceContents(Contents);
	for (AActor* Actor : Contents)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		MovingSurfaceContents.Add(Actor);
		MovingSurfaceContentRelativeTransforms.Add(
			Actor->GetActorTransform().GetRelativeTransform(
				GetActorTransform()));
	}
}

void ABotanicusWorkSurfaceActor::ApplySurfaceContentTransforms()
{
	for (int32 Index = 0;
		 MovingSurfaceContents.IsValidIndex(Index) &&
		 MovingSurfaceContentRelativeTransforms.IsValidIndex(Index);
		 ++Index)
	{
		AActor* Actor = MovingSurfaceContents[Index].Get();
		if (!IsValid(Actor))
		{
			continue;
		}
		Actor->SetActorTransform(
			MovingSurfaceContentRelativeTransforms[Index] *
				GetActorTransform(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (HasAuthority())
		{
			Actor->SetNetDormancy(DORM_Awake);
			Actor->FlushNetDormancy();
			Actor->ForceNetUpdate();
		}
	}
}

void ABotanicusWorkSurfaceActor::ClearSurfaceContentTransforms()
{
	MovingSurfaceContents.Reset();
	MovingSurfaceContentRelativeTransforms.Reset();
}
