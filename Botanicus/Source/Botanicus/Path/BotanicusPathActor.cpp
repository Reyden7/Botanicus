// Copyright Epic Games, Inc. All Rights Reserved.

#include "Path/BotanicusPathActor.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusPathActor::ABotanicusPathActor()
{
	bReplicates = true;
	SetReplicateMovement(false);

	SplineComponent =
		CreateDefaultSubobject<USplineComponent>(TEXT("Path Spline"));
	SetRootComponent(SplineComponent);
	SplineComponent->SetClosedLoop(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		SegmentMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		JunctionMesh = CylinderMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialFinder.Succeeded())
	{
		PathMaterial = MaterialFinder.Object;
	}
}

void ABotanicusPathActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusPathActor, PathPoints);
	DOREPLIFETIME(ABotanicusPathActor, JunctionPoints);
}

void ABotanicusPathActor::InitializeConfirmedPath(
	const TArray<FVector>& WorldPoints)
{
	SetPathPointsInternal(WorldPoints, false);
	ForceNetUpdate();
}

void ABotanicusPathActor::SetPreviewPath(
	const TArray<FVector>& WorldPoints)
{
	SetPathPointsInternal(WorldPoints, true);
}

void ABotanicusPathActor::AddJunctionPoint(const FVector& WorldPoint)
{
	if (!HasAuthority() || bPreviewPath)
	{
		return;
	}

	for (const FVector_NetQuantize10& ExistingPoint : JunctionPoints)
	{
		if (FVector::DistSquared2D(FVector(ExistingPoint), WorldPoint) <
			FMath::Square(25.0f))
		{
			return;
		}
	}

	JunctionPoints.Add(FVector_NetQuantize10(WorldPoint));
	RebuildPathMeshes();
	ForceNetUpdate();
}

void ABotanicusPathActor::RestoreJunctionPoints(
	const TArray<FVector>& WorldPoints)
{
	if (!HasAuthority())
	{
		return;
	}

	JunctionPoints.Reset(WorldPoints.Num());
	for (const FVector& Point : WorldPoints)
	{
		JunctionPoints.Add(FVector_NetQuantize10(Point));
	}
	RebuildPathMeshes();
	ForceNetUpdate();
}

TArray<FVector> ABotanicusPathActor::GetPathWorldPoints() const
{
	TArray<FVector> Result;
	Result.Reserve(PathPoints.Num());
	for (const FVector_NetQuantize10& Point : PathPoints)
	{
		Result.Add(FVector(Point));
	}
	return Result;
}

TArray<FVector> ABotanicusPathActor::GetJunctionWorldPoints() const
{
	TArray<FVector> Result;
	Result.Reserve(JunctionPoints.Num());
	for (const FVector_NetQuantize10& Point : JunctionPoints)
	{
		Result.Add(FVector(Point));
	}
	return Result;
}

bool ABotanicusPathActor::FindClosestPoint(
	const FVector& WorldLocation,
	FVector& OutClosestPoint,
	float& OutDistance) const
{
	if (!SplineComponent || PathPoints.Num() < 2 || bPreviewPath)
	{
		return false;
	}

	OutClosestPoint =
		SplineComponent->FindLocationClosestToWorldLocation(
			WorldLocation,
			ESplineCoordinateSpace::World);
	OutDistance = FVector::Dist2D(WorldLocation, OutClosestPoint);
	return true;
}

bool ABotanicusPathActor::FindClosestSegment(
	const FVector& WorldLocation,
	int32& OutSegmentIndex,
	FVector& OutClosestPoint,
	float& OutDistance) const
{
	if (!SplineComponent || PathPoints.Num() < 2 || bPreviewPath)
	{
		return false;
	}

	const float InputKey =
		SplineComponent->FindInputKeyClosestToWorldLocation(WorldLocation);
	OutSegmentIndex = FMath::Clamp(
		FMath::FloorToInt(InputKey),
		0,
		PathPoints.Num() - 2);
	OutClosestPoint =
		SplineComponent->FindLocationClosestToWorldLocation(
			WorldLocation,
			ESplineCoordinateSpace::World);
	OutDistance = FVector::Dist2D(WorldLocation, OutClosestPoint);
	return true;
}

void ABotanicusPathActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildPathMeshes();
}

void ABotanicusPathActor::OnRep_PathPoints()
{
	bPreviewPath = false;
	RebuildPathMeshes();
}

void ABotanicusPathActor::OnRep_JunctionPoints()
{
	RebuildPathMeshes();
}

void ABotanicusPathActor::SetPathPointsInternal(
	const TArray<FVector>& WorldPoints,
	bool bIsPreview)
{
	PathPoints.Reset(WorldPoints.Num());
	for (const FVector& Point : WorldPoints)
	{
		PathPoints.Add(FVector_NetQuantize10(Point));
	}
	bPreviewPath = bIsPreview;
	RebuildPathMeshes();
}

void ABotanicusPathActor::RebuildPathMeshes()
{
	for (USplineMeshComponent* Segment : SegmentComponents)
	{
		if (IsValid(Segment))
		{
			Segment->DestroyComponent();
		}
	}
	SegmentComponents.Reset();
	for (UStaticMeshComponent* Junction : JunctionComponents)
	{
		if (IsValid(Junction))
		{
			Junction->DestroyComponent();
		}
	}
	JunctionComponents.Reset();

	if (!SplineComponent)
	{
		return;
	}

	SplineComponent->ClearSplinePoints(false);
	for (const FVector_NetQuantize10& WorldPoint : PathPoints)
	{
		const FVector LocalPoint =
			GetActorTransform().InverseTransformPosition(FVector(WorldPoint));
		SplineComponent->AddSplinePoint(
			LocalPoint,
			ESplineCoordinateSpace::Local,
			false);
	}
	SplineComponent->UpdateSpline();

	if (!SegmentMesh || PathPoints.Num() < 2)
	{
		return;
	}

	const FVector2D SegmentScale(
		PathWidth / 100.0f,
		PathThickness / 100.0f);

	for (int32 SegmentIndex = 0;
		 SegmentIndex < PathPoints.Num() - 1;
		 ++SegmentIndex)
	{
		USplineMeshComponent* Segment =
			NewObject<USplineMeshComponent>(this);
		if (!Segment)
		{
			continue;
		}

		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetupAttachment(SplineComponent);
		Segment->RegisterComponent();
		Segment->SetStaticMesh(SegmentMesh);
		Segment->SetForwardAxis(ESplineMeshAxis::X, false);
		if (PathMaterial)
		{
			Segment->SetMaterial(0, PathMaterial);
		}

		FVector StartPosition;
		FVector StartTangent;
		FVector EndPosition;
		FVector EndTangent;
		SplineComponent->GetLocationAndTangentAtSplinePoint(
			SegmentIndex,
			StartPosition,
			StartTangent,
			ESplineCoordinateSpace::Local);
		SplineComponent->GetLocationAndTangentAtSplinePoint(
			SegmentIndex + 1,
			EndPosition,
			EndTangent,
			ESplineCoordinateSpace::Local);

		Segment->SetStartAndEnd(
			StartPosition,
			StartTangent,
			EndPosition,
			EndTangent,
			false);
		Segment->SetStartScale(SegmentScale, false);
		Segment->SetEndScale(SegmentScale, true);
		Segment->SetCollisionEnabled(
			bPreviewPath
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryAndPhysics);
		Segment->SetCollisionProfileName(
			bPreviewPath
				? UCollisionProfile::NoCollision_ProfileName
				: UCollisionProfile::BlockAll_ProfileName);
		Segment->SetRenderCustomDepth(bPreviewPath);
		Segment->SetCustomDepthStencilValue(1);
		SegmentComponents.Add(Segment);
	}

	if (!JunctionMesh)
	{
		return;
	}

	for (const FVector_NetQuantize10& JunctionPoint : JunctionPoints)
	{
		UStaticMeshComponent* Junction =
			NewObject<UStaticMeshComponent>(this);
		if (!Junction)
		{
			continue;
		}

		Junction->SetMobility(EComponentMobility::Movable);
		Junction->SetupAttachment(SplineComponent);
		Junction->RegisterComponent();
		Junction->SetStaticMesh(JunctionMesh);
		if (PathMaterial)
		{
			Junction->SetMaterial(0, PathMaterial);
		}
		Junction->SetWorldLocation(FVector(JunctionPoint));
		Junction->SetWorldScale3D(
			FVector(
				PathWidth / 100.0f,
				PathWidth / 100.0f,
				PathThickness / 100.0f));
		Junction->SetCollisionProfileName(
			UCollisionProfile::BlockAll_ProfileName);
		JunctionComponents.Add(Junction);
	}
}
