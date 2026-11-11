// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusDeliveryZoneActor.generated.h"

class ABotanicusDeliveryShip;
struct FBotanicusPendingOrder;
class ABotanicusDeliveryParcelActor;
class USceneComponent;
class UBoxComponent;
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
	/** Select an unobstructed drop inside the receiving deck; no post-impact teleport. */
	bool StartParcelDelivery(ABotanicusDeliveryParcelActor* Parcel, int32 ParcelIndex,
		bool& bOutGroundFallback) const;

    /** Unscaled reference based on the mesh receiving surface, not detection bounds. */
    FTransform GetDeliveryFrame() const;
    bool EnqueueOrder(const FBotanicusPendingOrder& Order);
    bool HasQueuedOrders() const { return DeliveryQueue.Num() > 0; }
    bool DropNextOrder(ABotanicusDeliveryShip* Ship);
    void OnShipFinished(ABotanicusDeliveryShip* Ship);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Delivery")
    TObjectPtr<USceneComponent> DeliveryDropCenter;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Botanicus|Delivery")
    TSoftClassPtr<ABotanicusDeliveryShip> DeliveryShipClass;
private:
    struct FQueuedDelivery
    {
        FGuid OrderId;
        FName ItemKey;
        int32 Quantity;
    };
    TArray<FQueuedDelivery> DeliveryQueue;
    TSet<FGuid> QueuedOrderIds;
    UPROPERTY(Transient) TObjectPtr<ABotanicusDeliveryShip> ActiveShip;
    FTimerHandle ShipRestartTimer;
    void StartNextShip();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Delivery",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float DeliverySpawnHeight = 2000.0f;

	void SnapToUnderlyingGround();
	void RefreshVisuals();
	void RefreshWalkableCollision();

	/** Opt-in for the imported delivery platform; the legacy plane remains a visual marker. */
	UPROPERTY(EditAnywhere, Category="Botanicus|Platform Collision")
	bool bUseWalkablePlatformCollision = false;

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Platform Collision")
	TObjectPtr<UBoxComponent> WalkableDeck;
	UPROPERTY(VisibleAnywhere, Category="Botanicus|Platform Collision")
	TObjectPtr<UBoxComponent> RampLanding;
	UPROPERTY(VisibleAnywhere, Category="Botanicus|Platform Collision")
	TObjectPtr<UBoxComponent> WalkableRamp;

	/** Measured from the imported mesh, in Delivery Pad coordinates. */
	UPROPERTY(EditAnywhere, Category="Botanicus|Platform Collision")
	FVector RampTop = FVector(0.16855f,139.7598f,31.53161f);
	UPROPERTY(EditAnywhere, Category="Botanicus|Platform Collision")
	FVector RampBottom = FVector(0.16855f,228.44444f,-0.14235f);
	UPROPERTY(EditAnywhere, Category="Botanicus|Platform Collision",meta=(ClampMin="1"))
	float RampHalfWidth = 79.73f;

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
