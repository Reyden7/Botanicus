// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Water/BotanicusWaterTypes.h"
#include "BotanicusWaterSubsystem.generated.h"

class UBotanicusWaterSettings;
class UPrimitiveComponent;

namespace BotanicusWaterSurface
{
	BOTANICUS_API EBotanicusWaterSurfaceClass ClassifySurface(
		const FVector& SurfaceNormal,
		const UBotanicusWaterSettings& Settings);

	BOTANICUS_API FBotanicusWaterReceiveResult DistributeImpactWater(
		float EmittedAmount,
		const FVector& SurfaceNormal,
		const UBotanicusWaterSettings& Settings);
}

/**
 * Bounded world-level store for functional wet zones created by water impacts.
 */
UCLASS()
class BOTANICUS_API UBotanicusWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Water")
	FBotanicusWaterReceiveResult ApplyWaterHit(
		const FBotanicusWaterHit& WaterHit);

	UFUNCTION(BlueprintPure, Category="Botanicus|Water")
	int32 GetTrackedZoneCount() const { return Zones.Num(); }

private:
	struct FWetZone
	{
		FVector Location = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FVector LocalLocation = FVector::ZeroVector;
		FVector LocalNormal = FVector::UpVector;
		TWeakObjectPtr<UPrimitiveComponent> SurfaceComponent;
		TWeakObjectPtr<AActor> SurfaceActor;
		float AccumulatedWater = 0.0f;
		EBotanicusWaterSurfaceClass SurfaceClass =
			EBotanicusWaterSurfaceClass::Horizontal;
		bool bCanCreatePuddle = false;
	};

	int32 FindZone(const FBotanicusWaterHit& WaterHit, float MergeRadius) const;
	int32 AllocateZone(const FBotanicusWaterHit& WaterHit,
		const FBotanicusWaterReceiveResult& Result,
		const UBotanicusWaterSettings& Settings);
	void RefreshZoneWorldTransform(FWetZone& Zone) const;

	TArray<FWetZone> Zones;
};
