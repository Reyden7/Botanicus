// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusVisitorZoneActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EBotanicusVisitorZoneType : uint8
{
	Parking,
	SalesArea,
	Checkout
};

/** Planning marker used to define the tycoon visitor circuit. */
UCLASS()
class BOTANICUS_API ABotanicusVisitorZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusVisitorZoneActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeZone(
		EBotanicusVisitorZoneType InZoneType,
		const FVector& InBoxExtent);
	bool ContainsPoint2D(const FVector& WorldPoint) const;

	EBotanicusVisitorZoneType GetZoneType() const
	{
		return ZoneType;
	}
	FVector GetZoneExtent() const { return BoxExtent; }

private:
	UFUNCTION()
	void OnRep_ZoneConfiguration();

	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> ZoneBounds;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ZoneVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ZoneLabel;

	UPROPERTY(ReplicatedUsing=OnRep_ZoneConfiguration)
	EBotanicusVisitorZoneType ZoneType =
		EBotanicusVisitorZoneType::SalesArea;

	UPROPERTY(ReplicatedUsing=OnRep_ZoneConfiguration)
	FVector BoxExtent = FVector(500.0f, 400.0f, 8.0f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
};
