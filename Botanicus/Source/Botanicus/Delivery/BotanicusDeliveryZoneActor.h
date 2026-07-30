// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusDeliveryZoneActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Temporary replicated delivery pad used by the equipment-order prototype. */
UCLASS()
class BOTANICUS_API ABotanicusDeliveryZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusDeliveryZoneActor();

	FVector GetParcelSpawnLocation(int32 ParcelIndex = 0) const;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Pad;
};
