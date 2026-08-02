// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusCashRegisterActor.generated.h"

class ABotanicusVisitorCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Starter checkout station used by players to scan and collect sales. */
UCLASS()
class BOTANICUS_API ABotanicusCashRegisterActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusCashRegisterActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	FVector GetCustomerStandLocation() const;
	ABotanicusVisitorCharacter* GetCheckoutCustomer() const;
	void HandleCheckoutAction(AActor* Interactor);
	bool IsOperational() const { return IsInsideCheckoutZone(); }

private:
	bool IsInsideCheckoutZone() const;
	bool IsLocalPlayerTargetingRegister() const;
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CounterVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RegisterVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ScreenVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ContextActionText;
};
