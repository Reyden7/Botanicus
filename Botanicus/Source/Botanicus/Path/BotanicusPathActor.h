// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusPathActor.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EBotanicusPathType : uint8
{
	Standard,
	VisitorRoute
};

/** Replicated spline path created from the top-down planning view. */
UCLASS()
class BOTANICUS_API ABotanicusPathActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusPathActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeConfirmedPath(
		const TArray<FVector>& WorldPoints,
		EBotanicusPathType InPathType =
			EBotanicusPathType::Standard);
	void SetPreviewPath(
		const TArray<FVector>& WorldPoints,
		EBotanicusPathType InPathType =
			EBotanicusPathType::Standard);
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
	EBotanicusPathType GetPathType() const { return PathType; }
	bool IsVisitorRoute() const
	{
		return PathType == EBotanicusPathType::VisitorRoute;
	}

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UFUNCTION()
	void OnRep_PathPoints();

	UFUNCTION()
	void OnRep_JunctionPoints();

	UFUNCTION()
	void OnRep_PathType();

	void SetPathPointsInternal(
		const TArray<FVector>& WorldPoints,
		bool bIsPreview,
		EBotanicusPathType InPathType);
	void RebuildPathMeshes();

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Path")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(ReplicatedUsing=OnRep_PathPoints)
	TArray<FVector_NetQuantize10> PathPoints;

	UPROPERTY(ReplicatedUsing=OnRep_JunctionPoints)
	TArray<FVector_NetQuantize10> JunctionPoints;

	UPROPERTY(ReplicatedUsing=OnRep_PathType)
	EBotanicusPathType PathType =
		EBotanicusPathType::Standard;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SegmentMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> JunctionMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PathMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicPathMaterial;

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
