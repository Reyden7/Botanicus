// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "BotanicusPreparationWorkbenchActor.generated.h"

class ABotanicusSalePotActor;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Carryable preparation bench with one snap point for a sale pot. */
UCLASS()
class BOTANICUS_API ABotanicusPreparationWorkbenchActor
	: public ABotanicusLargeEquipmentActor
{
	GENERATED_BODY()

public:
	ABotanicusPreparationWorkbenchActor();

	FTransform GetSalePotPreparationTransform() const;
	bool IsSalePotSlotAvailable(
		const ABotanicusSalePotActor* IgnoredPot = nullptr) const;

private:
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Legs;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> PreparationLabel;
};
