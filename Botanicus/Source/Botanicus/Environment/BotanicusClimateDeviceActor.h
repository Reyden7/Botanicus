// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "BotanicusClimateDeviceActor.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBotanicusClimateDeviceType : uint8
{
	Heater UMETA(DisplayName="Chauffage"),
	Cooler UMETA(DisplayName="Refroidisseur"),
	GrowLight UMETA(DisplayName="Lampe horticole"),
	Mister UMETA(DisplayName="Brumisateur"),
	Shade UMETA(DisplayName="Ombrière"),
	Dehumidifier UMETA(DisplayName="Déshumidificateur")
};

/** Purchasable furniture item with a circular local climate zone. */
UCLASS(Abstract)
class BOTANICUS_API ABotanicusClimateDeviceActor
	: public ABotanicusLargeEquipmentActor
{
	GENERATED_BODY()

public:
	ABotanicusClimateDeviceActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FVector GetPlacementBoxExtent() const override;

	virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(
		AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	EBotanicusClimateDeviceType GetDeviceType() const { return DeviceType; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	bool IsClimateDeviceEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	float GetInfluenceRadius() const { return InfluenceRadius; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	float GetMaximumEffectStrength() const { return MaximumEffectStrength; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	float GetPowerLevel() const { return PowerLevel; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	int32 GetDeviceLevel() const { return DeviceLevel; }

	/** Maximum normalized power allowed by this purchased model. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	float GetMaximumPowerLevel() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Climate Device")
	float GetInfluenceStrengthAtLocation(const FVector& WorldLocation) const;

	/** Adds this device's temperature, humidity and light deltas in its zone. */
	void GetEnvironmentDeltasAtLocation(
		const FVector& WorldLocation,
		float& OutTemperatureDelta,
		float& OutHumidityDelta,
		float& OutLuminosityDelta) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Botanicus|Climate Device")
	void SetClimateDeviceEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Botanicus|Climate Device")
	void SetClimateDevicePowerLevel(float InPowerLevel);

	void RestoreClimateDeviceState(
		bool bInEnabled,
		float InInfluenceRadius,
		float InMaximumEffectStrength,
		float InPowerLevel = 1.0f);

protected:
	virtual void OnEquipmentDefinitionApplied() override;
	void ConfigureDevice(
		EBotanicusClimateDeviceType InType,
		float InMaximumEffectStrength,
		const FLinearColor& InColor);
	UStaticMeshComponent* AddDevicePart(
		FName ComponentName,
		const FVector& Location,
		const FVector& Size,
		bool bCollisionEnabled = true);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category="Botanicus|Climate Device")
	EBotanicusClimateDeviceType DeviceType =
		EBotanicusClimateDeviceType::Heater;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_DeviceState,
		BlueprintReadOnly, Category="Botanicus|Climate Device",
		meta=(ClampMin="50.0", ClampMax="3000.0", Units="cm"))
	float InfluenceRadius = 500.0f;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_DeviceState,
		BlueprintReadOnly, Category="Botanicus|Climate Device")
	float MaximumEffectStrength = 12.0f;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_DeviceState,
		BlueprintReadOnly, Category="Botanicus|Climate Device")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_DeviceState,
		BlueprintReadOnly, Category="Botanicus|Climate Device",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float PowerLevel = 1.0f;

	/** Fixed equipment tier. It is selected by purchasing another model. */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_DeviceState,
		BlueprintReadOnly, Category="Botanicus|Climate Device",
		meta=(ClampMin="1", ClampMax="4"))
	int32 DeviceLevel = 4;

private:
	UFUNCTION()
	void OnRep_DeviceState();

	void RefreshDeviceVisuals();
	void RefreshInfluenceZoneGeometry();
	void ApplyLevelFromItemKey();
	FText GetDeviceDisplayName() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> DeviceVisualRoot;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> InfluenceZoneParts;

	UPROPERTY(Transient)
	FLinearColor DeviceColor = FLinearColor(1.0f, 0.35f, 0.05f, 1.0f);
};

UCLASS()
class BOTANICUS_API ABotanicusHeatingDeviceActor
	: public ABotanicusClimateDeviceActor
{
	GENERATED_BODY()
public:
	ABotanicusHeatingDeviceActor();
};

UCLASS()
class BOTANICUS_API ABotanicusCoolingDeviceActor
	: public ABotanicusClimateDeviceActor
{
	GENERATED_BODY()
public:
	ABotanicusCoolingDeviceActor();
};

UCLASS()
class BOTANICUS_API ABotanicusGrowLightDeviceActor
	: public ABotanicusClimateDeviceActor
{
	GENERATED_BODY()
public:
	ABotanicusGrowLightDeviceActor();
};

UCLASS()
class BOTANICUS_API ABotanicusMisterDeviceActor
	: public ABotanicusClimateDeviceActor
{
	GENERATED_BODY()
public:
	ABotanicusMisterDeviceActor();
};

/** Adjustable artificial shade that locally lowers received luminosity. */
UCLASS()
class BOTANICUS_API ABotanicusShadeDeviceActor
	: public ABotanicusClimateDeviceActor
{
	GENERATED_BODY()
public:
	ABotanicusShadeDeviceActor();
};

/** Adjustable climate device that locally lowers air humidity. */
UCLASS()
class BOTANICUS_API ABotanicusDehumidifierDeviceActor
	: public ABotanicusClimateDeviceActor
{
	GENERATED_BODY()
public:
	ABotanicusDehumidifierDeviceActor();
};
