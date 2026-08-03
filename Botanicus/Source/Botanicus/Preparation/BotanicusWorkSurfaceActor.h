// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "BotanicusWorkSurfaceActor.generated.h"

/**
 * Rectangular furniture with a continuous free-placement top.
 * Items resting on it keep their exact relative position when furniture mode
 * moves the surface.
 */
UCLASS()
class BOTANICUS_API ABotanicusWorkSurfaceActor
	: public ABotanicusLargeEquipmentActor
{
	GENERATED_BODY()

public:
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void BeginPlacement(ABotanicusCharacter* Character) override;
	virtual void UpdatePlacement(
		const FTransform& PlacementTransform,
		bool bIsValid) override;
	virtual void ConfirmPlacement() override;
	virtual void CancelPlacement() override;
	virtual void SetLocalPlacementPreview(
		const FTransform& PlacementTransform,
		bool bIsValid) override;

	static bool IsCatalogItemCompatible(
		const UObject* Context,
		FName InItemKey);

	bool GetFreePlacementTransform(
		FName InItemKey,
		const FVector& DesiredWorldLocation,
		float DesiredYaw,
		const FVector& ItemExtent,
		FTransform& OutTransform) const;
	bool HasSurfaceContents() const;
	bool ContainsSurfaceActor(const AActor* Actor) const;
	void GetSurfaceContents(TArray<AActor*>& OutActors) const;
	void SetMoveContentsWithFurniture(bool bEnabled);

private:
	bool IsActorRestingOnSurface(
		const AActor* Actor,
		const FVector& ActorExtent) const;
	void CaptureSurfaceContentTransforms();
	void ApplySurfaceContentTransforms();
	void ClearSurfaceContentTransforms();

	TArray<TWeakObjectPtr<AActor>> MovingSurfaceContents;
	TArray<FTransform> MovingSurfaceContentRelativeTransforms;
	bool bMoveContentsWithFurniture = false;
};
