// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusDeliveryZoneActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Replicated, persistent starter delivery area for catalogue orders. */
UCLASS()
class BOTANICUS_API ABotanicusDeliveryZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusDeliveryZoneActor();
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	FVector GetParcelSpawnLocation(int32 ParcelIndex = 0) const;

private:
	void SnapToUnderlyingGround();
	void RefreshVisuals();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Zone",
		meta=(AllowPrivateAccess="true", ClampMin="100.0"))
	FVector2D ZoneSize = FVector2D(480.0f, 480.0f);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ZoneLabel;
};
