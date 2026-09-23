// Copyright Epic Games, Inc. All Rights Reserved.

#include "Path/BotanicusPathActor.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshFinder.Succeeded())
	{
		SegmentMesh = PlaneMeshFinder.Object;
		JunctionMesh = PlaneMeshFinder.Object;
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
	DOREPLIFETIME(ABotanicusPathActor, PathType);
}

void ABotanicusPathActor::AddJunctionPoint(const FVector& WorldPoint)
{
	if (!HasAuthority())
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

	JunctionPoints.Add(FVector_NetQuantize10(
		SnapVisualPointToGround(WorldPoint)));
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

TArray<FVector> ABotanicusPathActor::GetNavigationWorldPoints(
	float MaximumPointSpacing) const
{
	TArray<FVector> Result;
	if (!SplineComponent || PathPoints.Num() < 2)
	{
		return Result;
	}

	const float SplineLength = SplineComponent->GetSplineLength();
	const float SafeSpacing = FMath::Max(25.0f, MaximumPointSpacing);
	const int32 StepCount = FMath::Max(
		1,
		FMath::CeilToInt(SplineLength / SafeSpacing));
	Result.Reserve(StepCount + 1);
	for (int32 StepIndex = 0; StepIndex <= StepCount; ++StepIndex)
	{
		const float Distance =
			SplineLength * static_cast<float>(StepIndex) /
			static_cast<float>(StepCount);
		Result.Add(SplineComponent->GetLocationAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::World));
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
	if (!SplineComponent || PathPoints.Num() < 2)
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
	if (!SplineComponent || PathPoints.Num() < 2)
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
	if (!EditorPathPoints.IsEmpty())
	{
		PathPoints.Reset(EditorPathPoints.Num());
		for (const FVector& LocalPoint : EditorPathPoints)
		{
			PathPoints.Add(FVector_NetQuantize10(
				SnapVisualPointToGround(
					GetActorTransform().TransformPosition(LocalPoint))));
		}
	}
	RebuildPathMeshes();
}

void ABotanicusPathActor::OnRep_PathPoints()
{
	RebuildPathMeshes();
}

void ABotanicusPathActor::OnRep_JunctionPoints()
{
	RebuildPathMeshes();
}

void ABotanicusPathActor::OnRep_PathType()
{
	DynamicPathMaterial = nullptr;
	RebuildPathMeshes();
}

FVector ABotanicusPathActor::SnapVisualPointToGround(
	const FVector& WorldPoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return WorldPoint;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusPathGround),
		false,
		this);
	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(
			GroundHit,
			WorldPoint + FVector(0.0f, 0.0f, 1.0f),
			WorldPoint - FVector(0.0f, 0.0f, 1000.0f),
			ECC_Visibility,
			QueryParams))
	{
		return FVector(
			WorldPoint.X,
			WorldPoint.Y,
			GroundHit.ImpactPoint.Z + 0.25f);
	}
	return WorldPoint;
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

	if (PathMaterial && !DynamicPathMaterial)
	{
		DynamicPathMaterial =
			UMaterialInstanceDynamic::Create(PathMaterial, this);
		if (DynamicPathMaterial)
		{
			DynamicPathMaterial->SetVectorParameterValue(
				TEXT("Color"),
				PathType == EBotanicusPathType::VisitorRoute
					? FLinearColor(0.04f, 0.85f, 0.8f, 1.0f)
					: FLinearColor(0.32f, 0.32f, 0.32f, 1.0f));
		}
	}

	if (!SegmentMesh || PathPoints.Num() < 2)
	{
		return;
	}

	const FVector2D SegmentScale(
		PathWidth / 100.0f,
		1.0f);

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
		if (DynamicPathMaterial)
		{
			Segment->SetMaterial(0, DynamicPathMaterial);
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
		Segment->SetCollisionProfileName(
			UCollisionProfile::NoCollision_ProfileName);
		Segment->SetRenderCustomDepth(false);
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
		if (DynamicPathMaterial)
		{
			Junction->SetMaterial(0, DynamicPathMaterial);
		}
		Junction->SetWorldLocation(FVector(JunctionPoint));
		Junction->SetWorldScale3D(
			FVector(
				PathWidth / 100.0f,
				PathWidth / 100.0f,
				1.0f));
		Junction->SetCollisionProfileName(
			UCollisionProfile::NoCollision_ProfileName);
		JunctionComponents.Add(Junction);
	}
}
