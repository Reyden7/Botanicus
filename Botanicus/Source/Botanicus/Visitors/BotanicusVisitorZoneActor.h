// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusVisitorZoneActor.generated.h"

class ABotanicusCashRegisterActor;
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
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

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

    /** Opt-in on the main checkout area: follow the restored/moved register without changing its payment rules. */
    UPROPERTY(EditInstanceOnly, Category="Botanicus|Checkout") bool bFollowCheckoutRegister=false;
private:
    void RefreshCheckoutArea();
    TWeakObjectPtr<ABotanicusCashRegisterActor> CheckoutRegister;
    FTimerHandle CheckoutAreaTimer;
	UFUNCTION()
	void OnRep_ZoneConfiguration();

	void RefreshVisuals();
	void SnapToUnderlyingGround();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> ZoneBounds;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ZoneVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ZoneLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ZoneConfiguration,
		Category="Botanicus|Zone", meta=(AllowPrivateAccess="true"))
	EBotanicusVisitorZoneType ZoneType =
		EBotanicusVisitorZoneType::SalesArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ZoneConfiguration,
		Category="Botanicus|Zone", meta=(AllowPrivateAccess="true", ClampMin="2.0"))
	FVector BoxExtent = FVector(500.0f, 400.0f, 8.0f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
};
