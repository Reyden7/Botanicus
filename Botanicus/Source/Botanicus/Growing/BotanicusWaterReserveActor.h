// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusWaterReserveActor.generated.h"

class ABotanicusCharacter;
class UTextRenderComponent;

/** Purchasable, placeable water source used to refill a held watering can. */
UCLASS()
class BOTANICUS_API ABotanicusWaterReserveActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusWaterReserveActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	bool TryRefill(ABotanicusCharacter* Character);

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> ReserveText;
};
