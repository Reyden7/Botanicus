// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Water/BotanicusWaterTypes.h"
#include "BotanicusWaterSubsystem.generated.h"

class UBotanicusWaterSettings;
class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
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
 * Bounded world-level store for wet zones. It owns pooled visual components;
 * impacts never spawn one Actor or material instance per frame.
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
		float CurrentPuddleRadius = 0.0f;
		float ImpactPulse = 0.0f;
		double LastImpactTime = 0.0;
		int32 WetnessVisualIndex = INDEX_NONE;
		int32 PuddleVisualIndex = INDEX_NONE;
		int32 ShapeVariant = 0;
		float ShapeRotationDegrees = 0.0f;
		FVector2D ShapeScale = FVector2D::UnitVector;
		EBotanicusWaterSurfaceClass SurfaceClass =
			EBotanicusWaterSurfaceClass::Horizontal;
		bool bCanCreatePuddle = false;
	};

	void EnsureVisualPools(const UBotanicusWaterSettings& Settings);
	void DestroyVisualPools();
	int32 FindZone(const FBotanicusWaterHit& WaterHit, float MergeRadius) const;
	int32 AllocateZone(const FBotanicusWaterHit& WaterHit,
		const FBotanicusWaterReceiveResult& Result,
		const UBotanicusWaterSettings& Settings);
	void ReleaseZoneVisuals(FWetZone& Zone);
	void RefreshZoneWorldTransform(FWetZone& Zone) const;
	void UpdateWetnessVisual(FWetZone& Zone,
		const UBotanicusWaterSettings& Settings);
	void UpdatePuddleVisual(FWetZone& Zone, float DeltaTime,
		const UBotanicusWaterSettings& Settings);
	void TriggerImpactEffect(const FVector& Location, const FVector& Normal);
	void TriggerRunoffEffect(const FVector& Location, const FVector& Normal);

	TArray<FWetZone> Zones;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> WetnessVisualPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WetnessMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> PuddleVisualPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ImpactEffectPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> RunoffEffectPool;

	TArray<int32> FreeWetnessVisuals;
	TArray<int32> FreePuddleVisuals;
	int32 NextImpactEffect = 0;
	int32 NextRunoffEffect = 0;
	bool bVisualPoolsInitialized = false;
};
