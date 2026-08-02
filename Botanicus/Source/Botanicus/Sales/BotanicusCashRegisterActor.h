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
	FTransform GetSelfCheckoutSlotTransform(int32 SlotIndex) const;
	bool FindClosestAvailableSelfCheckoutSlot(
		const FVector& RequestedLocation,
		FTransform& OutTransform,
		const AActor* IgnoredSelfCheckout = nullptr) const;
	int32 FindSelfCheckoutSlotIndex(
		const FVector& WorldLocation,
		float Tolerance = 35.0f) const;
	static constexpr int32 SelfCheckoutSlotCount = 6;

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

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>>
		SelfCheckoutSlotVisuals;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> SelfCheckoutZoneLabel;
};
