// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visuals/BotanicusPotSoilVisualActor.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusPotSoilVisualActor::ABotanicusPotSoilVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);

	SoilMesh = CreateDefaultSubobject<UProceduralMeshComponent>(
		TEXT("Procedural Soil"));
	SoilMesh->SetupAttachment(SceneRoot);
	SoilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoilMesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Botanicus/Materials/Soil/M_BotanicusSoilWetPatchesV2.M_BotanicusSoilWetPatchesV2"));
	if (MaterialFinder.Succeeded())
	{
		SoilMaterial = MaterialFinder.Object;
		SoilMesh->SetMaterial(0, SoilMaterial);
	}
}

void ABotanicusPotSoilVisualActor::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildSoilMesh();
}

void ABotanicusPotSoilVisualActor::ConfigureSoilShape(
	const FVector2D& BottomRadii,
	const FVector2D& TopRadii,
	float FullHeight,
	float FillAlpha,
	bool bVisible,
	bool bSquareProfile,
	float Wetness)
{
	CurrentBottomRadii = FVector2D(
		FMath::Max(0.5f, BottomRadii.X),
		FMath::Max(0.5f, BottomRadii.Y));
	CurrentTopRadii = FVector2D(
		FMath::Max(0.5f, TopRadii.X),
		FMath::Max(0.5f, TopRadii.Y));
	CurrentFullHeight = FMath::Max(0.5f, FullHeight);
	CurrentFillAlpha = FMath::Clamp(FillAlpha, 0.0f, 1.0f);
	bCurrentVisible = bVisible;
	bCurrentSquareProfile = bSquareProfile;
	CurrentWetness = FMath::Clamp(Wetness, 0.0f, 1.0f);
	RebuildSoilMesh();
}

void ABotanicusPotSoilVisualActor::RefreshSoilMaterial()
{
	if (!SoilMesh || !SoilMaterial)
	{
		return;
	}
	if (!SoilDynamicMaterial)
	{
		SoilDynamicMaterial =
			UMaterialInstanceDynamic::Create(SoilMaterial, this);
	}
	if (SoilDynamicMaterial)
	{
		const float VisualWetness = FMath::Pow(
			CurrentWetness,
			FMath::Clamp(WetnessResponseExponent, 0.1f, 2.0f));
		SoilDynamicMaterial->SetScalarParameterValue(
			TEXT("Wetness"),
			VisualWetness);
		SoilMesh->SetMaterial(0, SoilDynamicMaterial);
	}
}

void ABotanicusPotSoilVisualActor::RebuildSoilMesh()
{
	if (!SoilMesh)
	{
		return;
	}

	SoilMesh->ClearAllMeshSections();
	SoilMesh->SetVisibility(bCurrentVisible);
	if (!bCurrentVisible)
	{
		return;
	}

	const int32 Segments = FMath::Clamp(RadialSegments, 12, 128);
	const float EffectiveFill = FMath::Lerp(
		FMath::Clamp(MinimumFillFraction, 0.001f, 0.25f),
		1.0f,
		CurrentFillAlpha);
	const float CurrentHeight = CurrentFullHeight * EffectiveFill;
	const FVector2D FilledTopRadii = FMath::Lerp(
		CurrentBottomRadii,
		CurrentTopRadii,
		EffectiveFill);
	const float SkirtBottomZ = FMath::Max(
		0.0f,
		CurrentHeight - FMath::Clamp(SurfaceThickness, 0.0f, 5.0f));
	const auto GetProfilePoint =
		[this](float CosAngle, float SinAngle, const FVector2D& Radii)
		{
			if (bCurrentSquareProfile)
			{
				const float Divisor = FMath::Max(
					FMath::Abs(CosAngle),
					FMath::Abs(SinAngle));
				return FVector2D(
					Radii.X * CosAngle / FMath::Max(KINDA_SMALL_NUMBER, Divisor),
					Radii.Y * SinAngle / FMath::Max(KINDA_SMALL_NUMBER, Divisor));
			}
			return FVector2D(
				Radii.X * CosAngle,
				Radii.Y * SinAngle);
		};

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve(Segments * 3 + 2);
	Normals.Reserve(Segments * 3 + 2);
	UVs.Reserve(Segments * 3 + 2);
	Colors.Reserve(Segments * 3 + 2);
	Tangents.Reserve(Segments * 3 + 2);

	// Only generate a very short skirt immediately below the visible surface.
	// The pot itself hides it; unlike the old full cone it can never appear as
	// a large wall while the soil is being filled.
	for (int32 Ring = 0; Ring < 2; ++Ring)
	{
		const float Z = Ring == 0 ? SkirtBottomZ : CurrentHeight;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle =
				2.0f * PI * static_cast<float>(Index) /
				static_cast<float>(Segments);
			const float CosAngle = FMath::Cos(Angle);
			const float SinAngle = FMath::Sin(Angle);
			const FVector2D ProfilePoint =
				GetProfilePoint(CosAngle, SinAngle, FilledTopRadii);
			Vertices.Add(FVector(
				ProfilePoint.X,
				ProfilePoint.Y,
				Z));
			Normals.Add(FVector(CosAngle, SinAngle, 0.0f).GetSafeNormal());
			UVs.Add(FVector2D(
				static_cast<float>(Index) / static_cast<float>(Segments),
				static_cast<float>(Ring)));
			Colors.Add(FLinearColor::White);
			Tangents.Add(FProcMeshTangent(-SinAngle, CosAngle, 0.0f));
		}
	}

	// A dedicated top ring gives the visible soil surface upward normals and
	// planar UVs instead of reusing the stretched side-wall vertices.
	const int32 TopCapRingStart = Vertices.Num();
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float Angle =
			2.0f * PI * static_cast<float>(Index) /
			static_cast<float>(Segments);
		const float CosAngle = FMath::Cos(Angle);
		const float SinAngle = FMath::Sin(Angle);
		const FVector2D ProfilePoint =
			GetProfilePoint(CosAngle, SinAngle, FilledTopRadii);
		Vertices.Add(FVector(
			ProfilePoint.X,
			ProfilePoint.Y,
			CurrentHeight));
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(
			0.5f + ProfilePoint.X / FilledTopRadii.X * 0.5f,
			0.5f + ProfilePoint.Y / FilledTopRadii.Y * 0.5f));
		Colors.Add(FLinearColor::White);
		Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
	}

	const int32 TopCenter = Vertices.Add(FVector(0.0f, 0.0f, CurrentHeight));
	Normals.Add(FVector::UpVector);
	UVs.Add(FVector2D(0.5f, 0.5f));
	Colors.Add(FLinearColor::White);
	Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));

	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const int32 Next = (Index + 1) % Segments;
		const int32 LowerA = Index;
		const int32 LowerB = Next;
		const int32 UpperA = Segments + Index;
		const int32 UpperB = Segments + Next;
		Triangles.Append({LowerA, UpperB, UpperA, LowerA, LowerB, UpperB});

		// Unreal uses clockwise front faces. Reverse the previous cap winding so
		// the horizontal soil surface is visible when viewed from above.
		Triangles.Append({
			TopCenter,
			TopCapRingStart + Next,
			TopCapRingStart + Index});
	}

	SoilMesh->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		Colors,
		Tangents,
		false);
	RefreshSoilMaterial();
}
