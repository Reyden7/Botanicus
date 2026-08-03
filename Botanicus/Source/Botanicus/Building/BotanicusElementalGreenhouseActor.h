// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Building/BotanicusCatalogBuildingActor.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "Interaction/BotanicusInteractable.h"
#include "BotanicusElementalGreenhouseActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Upgradeable greenhouse whose interior enables growth for one plant element.
 * Levels 1-3 expand the usable footprint and the physical shell.
 */
UCLASS()
class BOTANICUS_API ABotanicusElementalGreenhouseActor
	: public ABotanicusCatalogBuildingActor,
	  public IBotanicusInteractable
{
	GENERATED_BODY()

public:
	ABotanicusElementalGreenhouseActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	EBotanicusPlantElement GetElement() const { return Element; }
	int32 GetGreenhouseLevel() const { return GreenhouseLevel; }
	bool ContainsWorldLocation(const FVector& WorldLocation) const;
	void RestoreGreenhouseLevel(int32 InLevel);

	static EBotanicusPlantElement FindGreenhouseElementAtLocation(
		const UWorld* World,
		const FVector& WorldLocation);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Element")
	EBotanicusPlantElement Element =
		EBotanicusPlantElement::Fire;

private:
	UFUNCTION()
	void OnRep_GreenhouseLevel();

	void RefreshGeometry();
	FVector GetLevelSize() const;
	int32 GetNextUpgradeCost() const;
	FString GetElementLabel() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> GrowingVolume;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FloorPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BackWallPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LeftWallPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RightWallPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FrontWallLeftPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FrontWallRightPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RoofPart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(ReplicatedUsing=OnRep_GreenhouseLevel)
	int32 GreenhouseLevel = 1;
};

UCLASS()
class BOTANICUS_API ABotanicusFireGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()

public:
	ABotanicusFireGreenhouseActor();
};

UCLASS()
class BOTANICUS_API ABotanicusWaterGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()

public:
	ABotanicusWaterGreenhouseActor();
};

UCLASS()
class BOTANICUS_API ABotanicusIceGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()

public:
	ABotanicusIceGreenhouseActor();
};

UCLASS()
class BOTANICUS_API ABotanicusShadowGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()

public:
	ABotanicusShadowGreenhouseActor();
};
