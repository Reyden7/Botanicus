// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusPathActor.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** Replicated spline path created from the top-down planning view. */
UCLASS()
class BOTANICUS_API ABotanicusPathActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusPathActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeConfirmedPath(const TArray<FVector>& WorldPoints);
	void SetPreviewPath(const TArray<FVector>& WorldPoints);
	void AddJunctionPoint(const FVector& WorldPoint);
	void RestoreJunctionPoints(const TArray<FVector>& WorldPoints);

	TArray<FVector> GetPathWorldPoints() const;
	TArray<FVector> GetJunctionWorldPoints() const;
	bool FindClosestPoint(
		const FVector& WorldLocation,
		FVector& OutClosestPoint,
		float& OutDistance) const;
	bool FindClosestSegment(
		const FVector& WorldLocation,
		int32& OutSegmentIndex,
		FVector& OutClosestPoint,
		float& OutDistance) const;
	bool IsPreviewPath() const { return bPreviewPath; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UFUNCTION()
	void OnRep_PathPoints();

	UFUNCTION()
	void OnRep_JunctionPoints();

	void SetPathPointsInternal(
		const TArray<FVector>& WorldPoints,
		bool bIsPreview);
	void RebuildPathMeshes();

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Path")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(ReplicatedUsing=OnRep_PathPoints)
	TArray<FVector_NetQuantize10> PathPoints;

	UPROPERTY(ReplicatedUsing=OnRep_JunctionPoints)
	TArray<FVector_NetQuantize10> JunctionPoints;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SegmentMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> JunctionMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PathMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> SegmentComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> JunctionComponents;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Path", meta=(ClampMin="20.0"))
	float PathWidth = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Path", meta=(ClampMin="1.0"))
	float PathThickness = 12.0f;

	bool bPreviewPath = false;
};
