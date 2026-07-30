// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusDeliveryParcelActor.generated.h"

class UTextRenderComponent;

/** Replicated small parcel that becomes one private hotbar item when collected. */
UCLASS()
class BOTANICUS_API ABotanicusDeliveryParcelActor
	: public ABotanicusInteractableActor
{
	GENERATED_BODY()

public:
	ABotanicusDeliveryParcelActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	void InitializeParcel(FName InItemKey, int32 InQuantity);
	FName GetItemKey() const { return ItemKey; }
	int32 GetQuantity() const { return Quantity; }

private:
	UPROPERTY(Replicated)
	FName ItemKey = TEXT("SeedPacket_Test");

	UPROPERTY(Replicated)
	int32 Quantity = 1;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionIndicator;
};
