// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusSelfCheckoutActor.generated.h"

class ABotanicusVisitorCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Purchasable station where one visitor scans and pays without a player. */
UCLASS()
class BOTANICUS_API ABotanicusSelfCheckoutActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusSelfCheckoutActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	FVector GetCustomerStandLocation() const;
	bool IsOperational() const;
	ABotanicusVisitorCharacter* GetAssignedVisitor() const;

private:
	bool IsInsideCheckoutZone() const;
	bool IsMountedInSelfCheckoutSlot() const;
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ScreenVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ScannerVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;
};
