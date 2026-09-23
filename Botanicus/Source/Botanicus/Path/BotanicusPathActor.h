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

/** Level-authored, replicated spline path used by visitor navigation. */
UCLASS()
class BOTANICUS_API ABotanicusPathActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusPathActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void AddJunctionPoint(const FVector& WorldPoint);

	TArray<FVector> GetPathWorldPoints() const;
	/** Densified spline used by navigation and automatic junction detection. */
	TArray<FVector> GetNavigationWorldPoints(
		float MaximumPointSpacing = 120.0f) const;
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

	FVector SnapVisualPointToGround(const FVector& WorldPoint) const;
	void RebuildPathMeshes();

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Path")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(ReplicatedUsing=OnRep_PathPoints)
	TArray<FVector_NetQuantize10> PathPoints;

	UPROPERTY(ReplicatedUsing=OnRep_JunctionPoints)
	TArray<FVector_NetQuantize10> JunctionPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PathType,
		Category="Botanicus|Path", meta=(AllowPrivateAccess="true"))
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

	/** Local points used by paths placed directly in a level or Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Path",
		meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	TArray<FVector> EditorPathPoints =
	{
		FVector(-500.0f, 0.0f, 0.0f),
		FVector(500.0f, 0.0f, 0.0f)
	};

};
