// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Building/BotanicusCatalogBuildingActor.h"
#include "Interaction/BotanicusInteractable.h"
#include "BotanicusElementalGreenhouseActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * The nursery's single upgradeable greenhouse. Its base temperature, air
 * humidity and luminosity are replicated so future climate equipment can
 * build local microclimates on top of them.
 */
UCLASS()
class BOTANICUS_API ABotanicusGreenhouseActor
	: public ABotanicusCatalogBuildingActor,
	  public IBotanicusInteractable
{
	GENERATED_BODY()

public:
	ABotanicusGreenhouseActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category="Botanicus|Greenhouse")
	int32 GetGreenhouseLevel() const { return GreenhouseLevel; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Greenhouse|Environment")
	float GetTemperatureCelsius() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Greenhouse|Environment")
	float GetAirHumidityPercent() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Greenhouse|Environment")
	float GetLuminosityPercent() const;

	/** Resolves the seasonal base at this location. Equipment modifiers plug in here next. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Greenhouse|Environment")
	void GetEnvironmentAtLocation(
		const FVector& WorldLocation,
		float& OutTemperatureCelsius,
		float& OutAirHumidityPercent,
		float& OutLuminosityPercent) const;

	/** Legacy/manual fallback values used only when no shared season state exists. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Botanicus|Greenhouse|Environment")
	void SetEnvironmentValues(
		float InTemperatureCelsius,
		float InAirHumidityPercent,
		float InLuminosityPercent);

	bool ContainsWorldLocation(const FVector& WorldLocation) const;
	void RestoreGreenhouseState(
		int32 InLevel,
		float InTemperatureCelsius,
		float InAirHumidityPercent,
		float InLuminosityPercent);

	static ABotanicusGreenhouseActor* FindGreenhouseAtLocation(
		const UWorld* World,
		const FVector& WorldLocation);

private:
	UFUNCTION()
	void OnRep_GreenhouseState();

	void RefreshGeometry();
	FVector GetLevelSize() const;
	int32 GetNextUpgradeCost() const;

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

	UPROPERTY(ReplicatedUsing=OnRep_GreenhouseState)
	int32 GreenhouseLevel = 1;

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing=OnRep_GreenhouseState,
		Category="Botanicus|Greenhouse|Environment",
		meta=(ClampMin="-50.0", ClampMax="100.0", Units="Celsius"))
	float TemperatureCelsius = 20.0f;

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing=OnRep_GreenhouseState,
		Category="Botanicus|Greenhouse|Environment",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float AirHumidityPercent = 50.0f;

	UPROPERTY(
		EditAnywhere,
		ReplicatedUsing=OnRep_GreenhouseState,
		Category="Botanicus|Greenhouse|Environment",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float LuminosityPercent = 50.0f;
};

/** Legacy class paths kept only so old maps and saves migrate safely. */
UCLASS()
class BOTANICUS_API ABotanicusElementalGreenhouseActor
	: public ABotanicusGreenhouseActor
{
	GENERATED_BODY()
};

UCLASS()
class BOTANICUS_API ABotanicusFireGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()
};

UCLASS()
class BOTANICUS_API ABotanicusWaterGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()
};

UCLASS()
class BOTANICUS_API ABotanicusIceGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()
};

UCLASS()
class BOTANICUS_API ABotanicusShadowGreenhouseActor
	: public ABotanicusElementalGreenhouseActor
{
	GENERATED_BODY()
};

UCLASS()
class BOTANICUS_API ABotanicusCompactGreenhouseActor
	: public ABotanicusGreenhouseActor
{
	GENERATED_BODY()
};

UCLASS()
class BOTANICUS_API ABotanicusWorkshopGreenhouseActor
	: public ABotanicusGreenhouseActor
{
	GENERATED_BODY()
};
