// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusPotSoilVisualActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;

/**
 * Reusable soil volume whose truncated-cone profile is generated from the
 * inside dimensions supplied by its owning pot.
 */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusPotSoilVisualActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusPotSoilVisualActor();

	void ConfigureSoilShape(
		const FVector2D& BottomRadii,
		const FVector2D& TopRadii,
		float FullHeight,
		float FillAlpha,
		bool bVisible,
		bool bSquareProfile = false,
		float Wetness = 0.0f);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProceduralMeshComponent> SoilMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Soil")
	TObjectPtr<UMaterialInterface> SoilMaterial;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Botanicus|Soil",
		meta=(ClampMin="12", ClampMax="128"))
	int32 RadialSegments = 48;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Botanicus|Soil",
		meta=(ClampMin="0.001", ClampMax="0.25"))
	float MinimumFillFraction = 0.02f;

	/** Thickness of the small rim below the horizontal soil surface. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Botanicus|Soil",
		meta=(ClampMin="0.0", ClampMax="5.0", Units="cm"))
	float SurfaceThickness = 0.75f;

	/** Values below 1 make the soil darken earlier as soon as it is watered. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category="Botanicus|Soil|Wetness",
		meta=(ClampMin="0.1", ClampMax="2.0"))
	float WetnessResponseExponent = 0.5f;

private:
	void RebuildSoilMesh();
	void RefreshSoilMaterial();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SoilDynamicMaterial;

	FVector2D CurrentBottomRadii = FVector2D(20.0f, 20.0f);
	FVector2D CurrentTopRadii = FVector2D(30.0f, 30.0f);
	float CurrentFullHeight = 30.0f;
	float CurrentFillAlpha = 1.0f;
	bool bCurrentVisible = true;
	bool bCurrentSquareProfile = false;
	float CurrentWetness = 0.0f;
};
