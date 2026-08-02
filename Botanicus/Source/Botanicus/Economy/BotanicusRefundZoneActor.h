// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusRefundZoneActor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Player-only planning surface that automatically removes eligible placed
 * objects and refunds 80% of their catalogue price to the shared wallet.
 */
UCLASS()
class BOTANICUS_API ABotanicusRefundZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusRefundZoneActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeZone(const FVector& InBoxExtent);
	FVector GetZoneExtent() const { return BoxExtent; }

private:
	UFUNCTION()
	void OnRep_ZoneConfiguration();

	void RefreshVisuals();
	void ProcessRefundableObjects();
	bool IsObjectFullyInside(
		const AActor* Actor,
		const FVector& ObjectExtent) const;
	bool TryRefundObject(
		AActor* Actor,
		FName ItemKey,
		int32 Quantity);
	void NotifyPlayers(const FString& Message) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> ZoneBounds;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ZoneVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ZoneLabel;

	UPROPERTY(ReplicatedUsing=OnRep_ZoneConfiguration)
	FVector BoxExtent = FVector(220.0f, 150.0f, 6.0f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	TSet<TWeakObjectPtr<AActor>> RejectedActors;
	float RefundScanAccumulator = 0.0f;
};
