// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusCatalogBuildingActor.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Native complete-building prefab used when a map has no editor-authored EBS
 * template. One replicated actor owns the full structure and therefore moves,
 * saves and restores atomically.
 */
UCLASS(Abstract)
class BOTANICUS_API ABotanicusCatalogBuildingActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusCatalogBuildingActor();

protected:
	void BuildGeometry(float Width, float Depth, float Height);
	UStaticMeshComponent* AddBuildingPart(
		FName ComponentName,
		const FVector& Location,
		const FVector& Size,
		bool bCollisionEnabled = true);

	UPROPERTY(VisibleAnywhere, Category="Building")
	TObjectPtr<USceneComponent> BuildingRoot;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
};

UCLASS()
class BOTANICUS_API ABotanicusCompactGreenhouseActor
	: public ABotanicusCatalogBuildingActor
{
	GENERATED_BODY()

public:
	ABotanicusCompactGreenhouseActor();
};

UCLASS()
class BOTANICUS_API ABotanicusWorkshopGreenhouseActor
	: public ABotanicusCatalogBuildingActor
{
	GENERATED_BODY()

public:
	ABotanicusWorkshopGreenhouseActor();
};
