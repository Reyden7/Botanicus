// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/BotanicusWaterSubsystem.h"

#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Water/BotanicusWaterSettings.h"
#include "Water/BotanicusWaterTrajectory.h"

namespace
{
	constexpr int32 PuddleCustomDataWater = 0;
	constexpr int32 PuddleCustomDataImpactPulse = 1;
	constexpr int32 PuddleCustomDataShape = 2;
	constexpr int32 PuddleCustomDataCount = 3;
	constexpr int32 MaximumPuddleShapeVariants = 6;

	const TCHAR* SurfaceClassName(EBotanicusWaterSurfaceClass SurfaceClass)
	{
		switch (SurfaceClass)
		{
		case EBotanicusWaterSurfaceClass::Horizontal:
			return TEXT("Horizontal");
		case EBotanicusWaterSurfaceClass::Inclined:
			return TEXT("Inclined");
		default:
			return TEXT("Vertical");
		}
	}
}

EBotanicusWaterSurfaceClass BotanicusWaterSurface::ClassifySurface(
	const FVector& SurfaceNormal,
	const UBotanicusWaterSettings& Settings)
{
	const float UpDot = SurfaceNormal.GetSafeNormal().Dot(FVector::UpVector);
	const float MinimumPuddleDot = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(Settings.MaximumPuddleSlopeDegrees, 0.0f, 80.0f)));
	if (UpDot >= MinimumPuddleDot)
	{
		return EBotanicusWaterSurfaceClass::Horizontal;
	}
	if (UpDot <= FMath::Clamp(
		Settings.VerticalNormalDotThreshold, 0.0f, 0.95f))
	{
		return EBotanicusWaterSurfaceClass::Vertical;
	}
	return EBotanicusWaterSurfaceClass::Inclined;
}

FBotanicusWaterReceiveResult BotanicusWaterSurface::DistributeImpactWater(
	float EmittedAmount,
	const FVector& SurfaceNormal,
	const UBotanicusWaterSettings& Settings)
{
	FBotanicusWaterReceiveResult Result;
	Result.Emitted = FMath::Max(0.0f, EmittedAmount);
	Result.SurfaceClass = ClassifySurface(SurfaceNormal, Settings);
	Result.bCanCreatePuddle =
		Result.SurfaceClass == EBotanicusWaterSurfaceClass::Horizontal;

	float RetentionFraction = Settings.HorizontalRetentionFraction;
	switch (Result.SurfaceClass)
	{
	case EBotanicusWaterSurfaceClass::Inclined:
		RetentionFraction = Settings.InclinedRetentionFraction;
		break;
	case EBotanicusWaterSurfaceClass::Vertical:
		RetentionFraction = Settings.VerticalRetentionFraction;
		break;
	default:
		break;
	}
	RetentionFraction = FMath::Clamp(RetentionFraction, 0.0f, 1.0f);
	Result.Retained = Result.Emitted * RetentionFraction;
	const float NotRetained = Result.Emitted - Result.Retained;
	Result.Runoff = NotRetained * FMath::Clamp(Settings.RunoffShare, 0.0f, 1.0f);
	Result.LostOrSplash = Result.Emitted - Result.Retained - Result.Runoff;
	return Result;
}

void UBotanicusWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UBotanicusWaterSubsystem::Deinitialize()
{
	DestroyVisualPools();
	Zones.Reset();
	Super::Deinitialize();
}

bool UBotanicusWaterSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld() && Zones.Num() > 0;
}

TStatId UBotanicusWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UBotanicusWaterSubsystem,
		STATGROUP_Tickables);
}

void UBotanicusWaterSubsystem::Tick(float DeltaTime)
{
	const UBotanicusWaterSettings* Settings =
		GetDefault<UBotanicusWaterSettings>();
	if (!Settings || DeltaTime <= 0.0f)
	{
		return;
	}

	for (int32 ZoneIndex = Zones.Num() - 1; ZoneIndex >= 0; --ZoneIndex)
	{
		FWetZone& Zone = Zones[ZoneIndex];
		RefreshZoneWorldTransform(Zone);
		const bool bHasPuddleWater = Zone.bCanCreatePuddle &&
			Zone.AccumulatedWater > Settings->PuddleThreshold;
		const float EvaporationRate = bHasPuddleWater
			? Settings->PuddleEvaporationRate
			: Settings->WetnessEvaporationRate;
		Zone.AccumulatedWater = FMath::Max(
			0.0f,
			Zone.AccumulatedWater - FMath::Max(0.0f, EvaporationRate) * DeltaTime);
		Zone.ImpactPulse = FMath::Max(0.0f, Zone.ImpactPulse - DeltaTime * 2.5f);

		UpdateWetnessVisual(Zone, *Settings);
		UpdatePuddleVisual(Zone, DeltaTime, *Settings);
		if (Zone.AccumulatedWater <= KINDA_SMALL_NUMBER &&
			Zone.CurrentPuddleRadius <= 0.25f)
		{
			ReleaseZoneVisuals(Zone);
			Zones.RemoveAtSwap(ZoneIndex, 1, EAllowShrinking::No);
		}
	}
}

FBotanicusWaterReceiveResult UBotanicusWaterSubsystem::ApplyWaterHit(
	const FBotanicusWaterHit& WaterHit)
{
	const UBotanicusWaterSettings* Settings =
		GetDefault<UBotanicusWaterSettings>();
	FBotanicusWaterReceiveResult Result;
	if (!Settings || WaterHit.Amount <= KINDA_SMALL_NUMBER ||
		WaterHit.Normal.IsNearlyZero())
	{
		return Result;
	}

	EnsureVisualPools(*Settings);
	Result = BotanicusWaterSurface::DistributeImpactWater(
		WaterHit.Amount,
		WaterHit.Normal,
		*Settings);
	const int32 ExistingZoneIndex = FindZone(
		WaterHit,
		FMath::Max(1.0f, Settings->ZoneMergeRadius));
	const int32 ZoneIndex = ExistingZoneIndex != INDEX_NONE
		? ExistingZoneIndex
		: AllocateZone(WaterHit, Result, *Settings);
	if (!Zones.IsValidIndex(ZoneIndex))
	{
		Result.Runoff += Result.Retained;
		Result.Retained = 0.0f;
		return Result;
	}

	FWetZone& Zone = Zones[ZoneIndex];
	const float MaximumWater = FMath::Max(
		Settings->PuddleThreshold,
		Settings->MaximumAccumulatedWater);
	const float AvailableCapacity = FMath::Max(
		0.0f,
		MaximumWater - Zone.AccumulatedWater);
	const float AcceptedRetention = FMath::Min(
		Result.Retained,
		AvailableCapacity);
	const float Overflow = Result.Retained - AcceptedRetention;
	Result.Retained = AcceptedRetention;
	Result.Runoff += Overflow;

	if (AcceptedRetention > KINDA_SMALL_NUMBER)
	{
		const float PreviousWater = Zone.AccumulatedWater;
		const float CombinedWater = PreviousWater + AcceptedRetention;
		Zone.Location = (
			Zone.Location * PreviousWater +
			WaterHit.Location * AcceptedRetention) /
			FMath::Max(KINDA_SMALL_NUMBER, CombinedWater);
		Zone.Normal = (
			Zone.Normal * PreviousWater +
			WaterHit.Normal.GetSafeNormal() * AcceptedRetention).
			GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
		Zone.AccumulatedWater = CombinedWater;
		Zone.SurfaceClass = BotanicusWaterSurface::ClassifySurface(
			Zone.Normal,
			*Settings);
		Zone.bCanCreatePuddle =
			Zone.SurfaceClass == EBotanicusWaterSurfaceClass::Horizontal;
		if (UPrimitiveComponent* Component = Zone.SurfaceComponent.Get())
		{
			Zone.LocalLocation = Component->GetComponentTransform().
				InverseTransformPosition(Zone.Location);
			Zone.LocalNormal = Component->GetComponentTransform().
				InverseTransformVectorNoScale(Zone.Normal).GetSafeNormal();
		}
	}
	Zone.LastImpactTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Zone.ImpactPulse = 1.0f;
	UpdateWetnessVisual(Zone, *Settings);
	UpdatePuddleVisual(Zone, 0.0f, *Settings);
	TriggerImpactEffect(WaterHit.Location, WaterHit.Normal);
	if (Result.Runoff > KINDA_SMALL_NUMBER &&
		Result.SurfaceClass != EBotanicusWaterSurfaceClass::Horizontal)
	{
		TriggerRunoffEffect(WaterHit.Location, WaterHit.Normal);
	}

	if (BotanicusWaterTrajectory::IsDebugEnabled() && GetWorld())
	{
		DrawDebugString(
			GetWorld(),
			WaterHit.Location + WaterHit.Normal * 15.0f,
			FString::Printf(
				TEXT("%s  Wet=%.2f  Puddle=%s\nRetained %.3f / Runoff %.3f / Lost %.3f"),
				SurfaceClassName(Result.SurfaceClass),
				Zone.AccumulatedWater,
				Zone.bCanCreatePuddle &&
					Zone.AccumulatedWater > Settings->PuddleThreshold
						? TEXT("yes") : TEXT("no"),
				Result.Retained,
				Result.Runoff,
				Result.LostOrSplash),
			nullptr,
			FColor::White,
			0.15f,
			false);
	}
	return Result;
}

int32 UBotanicusWaterSubsystem::FindZone(
	const FBotanicusWaterHit& WaterHit,
	float MergeRadius) const
{
	const float MaximumDistanceSquared = FMath::Square(MergeRadius);
	int32 BestIndex = INDEX_NONE;
	float BestDistanceSquared = MaximumDistanceSquared;
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		const FWetZone& Zone = Zones[Index];
		const bool bSameSurface = WaterHit.HitComponent
			? Zone.SurfaceComponent.Get() == WaterHit.HitComponent
			: Zone.SurfaceActor.Get() == WaterHit.HitActor;
		const float DistanceSquared = FVector::DistSquared(
			Zone.Location,
			WaterHit.Location);
		if (bSameSurface && DistanceSquared <= BestDistanceSquared &&
			Zone.Normal.Dot(WaterHit.Normal.GetSafeNormal()) >= 0.70f)
		{
			BestIndex = Index;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestIndex;
}

int32 UBotanicusWaterSubsystem::AllocateZone(
	const FBotanicusWaterHit& WaterHit,
	const FBotanicusWaterReceiveResult& Result,
	const UBotanicusWaterSettings& Settings)
{
	const int32 MaximumZones = FMath::Clamp(
		Settings.MaximumTrackedZones,
		1,
		512);
	int32 ZoneIndex = INDEX_NONE;
	if (Zones.Num() < MaximumZones)
	{
		ZoneIndex = Zones.AddDefaulted();
	}
	else
	{
		// Preserve already-retained water. The caller converts the new amount
		// to runoff when the bounded zone budget is exhausted.
		return INDEX_NONE;
	}

	FWetZone& Zone = Zones[ZoneIndex];
	Zone.Location = WaterHit.Location;
	Zone.Normal = WaterHit.Normal.GetSafeNormal(
		SMALL_NUMBER,
		FVector::UpVector);
	Zone.SurfaceComponent = WaterHit.HitComponent;
	Zone.SurfaceActor = WaterHit.HitActor;
	Zone.SurfaceClass = Result.SurfaceClass;
	Zone.bCanCreatePuddle = Result.bCanCreatePuddle;
	if (UPrimitiveComponent* Component = Zone.SurfaceComponent.Get())
	{
		Zone.LocalLocation = Component->GetComponentTransform().
			InverseTransformPosition(Zone.Location);
		Zone.LocalNormal = Component->GetComponentTransform().
				InverseTransformVectorNoScale(Zone.Normal).GetSafeNormal();
	}
	const FIntVector SeedCell(
		FMath::FloorToInt(Zone.Location.X / FMath::Max(1.0f, Settings.ZoneMergeRadius)),
		FMath::FloorToInt(Zone.Location.Y / FMath::Max(1.0f, Settings.ZoneMergeRadius)),
		FMath::FloorToInt(Zone.Location.Z / FMath::Max(1.0f, Settings.ZoneMergeRadius)));
	const uint32 Seed = GetTypeHash(SeedCell);
	const int32 ShapeCount = FMath::Clamp(
		Settings.PuddleShapeVariants,
		1,
		MaximumPuddleShapeVariants);
	Zone.ShapeVariant = static_cast<int32>(Seed % static_cast<uint32>(ShapeCount));
	Zone.ShapeRotationDegrees = static_cast<float>(
		(Seed / static_cast<uint32>(ShapeCount)) % 360u);
	const float MinimumAspect = FMath::Min(
		Settings.MinimumPuddleAspectScale,
		Settings.MaximumPuddleAspectScale);
	const float MaximumAspect = FMath::Max(
		Settings.MinimumPuddleAspectScale,
		Settings.MaximumPuddleAspectScale);
	Zone.ShapeScale.X = FMath::Lerp(
		MinimumAspect,
		MaximumAspect,
		static_cast<float>((Seed / 2160u) % 101u) / 100.0f);
	Zone.ShapeScale.Y = FMath::Lerp(
		MinimumAspect,
		MaximumAspect,
		static_cast<float>((Seed / 218160u) % 101u) / 100.0f);
	return ZoneIndex;
}

void UBotanicusWaterSubsystem::RefreshZoneWorldTransform(FWetZone& Zone) const
{
	if (UPrimitiveComponent* Component = Zone.SurfaceComponent.Get())
	{
		Zone.Location = Component->GetComponentTransform().TransformPosition(
			Zone.LocalLocation);
		Zone.Normal = Component->GetComponentTransform().TransformVectorNoScale(
			Zone.LocalNormal).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
	}
}

void UBotanicusWaterSubsystem::EnsureVisualPools(
	const UBotanicusWaterSettings& Settings)
{
	if (bVisualPoolsInitialized)
	{
		return;
	}
	bVisualPoolsInitialized = true;
	UWorld* World = GetWorld();
	AWorldSettings* Owner = World ? World->GetWorldSettings() : nullptr;
	if (!Owner || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (UMaterialInterface* WetnessMaterial =
		Settings.WetnessDecalMaterial.LoadSynchronous())
	{
		const int32 Count = FMath::Clamp(Settings.MaximumWetnessVisuals, 1, 256);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UDecalComponent* Decal = NewObject<UDecalComponent>(Owner);
			Owner->AddInstanceComponent(Decal);
			Decal->SetMobility(EComponentMobility::Movable);
			Decal->SetVisibility(false);
			Decal->SetHiddenInGame(true);
			Decal->RegisterComponentWithWorld(World);
			UMaterialInstanceDynamic* Material =
				UMaterialInstanceDynamic::Create(WetnessMaterial, Decal);
			Decal->SetDecalMaterial(Material);
			WetnessVisualPool.Add(Decal);
			WetnessMaterials.Add(Material);
			FreeWetnessVisuals.Add(Index);
		}
	}

	UStaticMesh* PuddleMesh = Settings.PuddleMesh.LoadSynchronous();
	UMaterialInterface* PuddleMaterial = Settings.PuddleMaterial.LoadSynchronous();
	if (PuddleMesh && PuddleMaterial && Settings.MaximumVisiblePuddles > 0)
	{
		PuddleVisualPool = NewObject<UInstancedStaticMeshComponent>(Owner);
		Owner->AddInstanceComponent(PuddleVisualPool);
		PuddleVisualPool->SetMobility(EComponentMobility::Movable);
		PuddleVisualPool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PuddleVisualPool->SetCanEverAffectNavigation(false);
		PuddleVisualPool->SetCastShadow(false);
		PuddleVisualPool->SetStaticMesh(PuddleMesh);
		PuddleVisualPool->SetMaterial(0, PuddleMaterial);
		PuddleVisualPool->NumCustomDataFloats = PuddleCustomDataCount;
		PuddleVisualPool->RegisterComponentWithWorld(World);
		const int32 Count = FMath::Clamp(Settings.MaximumVisiblePuddles, 1, 128);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 InstanceIndex = PuddleVisualPool->AddInstance(
				FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector),
				true);
			FreePuddleVisuals.Add(InstanceIndex);
		}
	}

	auto CreateNiagaraPool = [Owner, World](
		UNiagaraSystem* System,
		int32 Count,
		TArray<TObjectPtr<UNiagaraComponent>>& Pool)
	{
		if (!System)
		{
			return;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Owner);
			Owner->AddInstanceComponent(Component);
			Component->SetAsset(System);
			Component->SetAutoActivate(false);
			Component->SetAutoDestroy(false);
			Component->SetCastShadow(false);
			Component->RegisterComponentWithWorld(World);
			Pool.Add(Component);
		}
	};
	CreateNiagaraPool(
		Settings.ImpactNiagara.LoadSynchronous(),
		FMath::Clamp(Settings.MaximumImpactEffects, 0, 32),
		ImpactEffectPool);
	CreateNiagaraPool(
		Settings.RunoffNiagara.LoadSynchronous(),
		FMath::Clamp(Settings.MaximumRunoffEffects, 0, 32),
		RunoffEffectPool);
}

void UBotanicusWaterSubsystem::DestroyVisualPools()
{
	for (UDecalComponent* Component : WetnessVisualPool)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	for (UNiagaraComponent* Component : ImpactEffectPool)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	for (UNiagaraComponent* Component : RunoffEffectPool)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	if (PuddleVisualPool)
	{
		PuddleVisualPool->DestroyComponent();
	}
	WetnessVisualPool.Reset();
	WetnessMaterials.Reset();
	ImpactEffectPool.Reset();
	RunoffEffectPool.Reset();
	PuddleVisualPool = nullptr;
	FreeWetnessVisuals.Reset();
	FreePuddleVisuals.Reset();
	bVisualPoolsInitialized = false;
}

void UBotanicusWaterSubsystem::ReleaseZoneVisuals(FWetZone& Zone)
{
	if (WetnessVisualPool.IsValidIndex(Zone.WetnessVisualIndex))
	{
		UDecalComponent* Decal = WetnessVisualPool[Zone.WetnessVisualIndex];
		Decal->SetVisibility(false);
		Decal->SetHiddenInGame(true);
		FreeWetnessVisuals.AddUnique(Zone.WetnessVisualIndex);
		Zone.WetnessVisualIndex = INDEX_NONE;
	}
	if (PuddleVisualPool && Zone.PuddleVisualIndex != INDEX_NONE)
	{
		PuddleVisualPool->UpdateInstanceTransform(
			Zone.PuddleVisualIndex,
			FTransform(FQuat::Identity, Zone.Location, FVector::ZeroVector),
			true,
			true,
			true);
		FreePuddleVisuals.AddUnique(Zone.PuddleVisualIndex);
		Zone.PuddleVisualIndex = INDEX_NONE;
	}
}

void UBotanicusWaterSubsystem::UpdateWetnessVisual(
	FWetZone& Zone,
	const UBotanicusWaterSettings& Settings)
{
	const float Wetness = FMath::Clamp(
		Zone.AccumulatedWater / FMath::Max(0.01f, Settings.WetnessSaturationAmount),
		0.0f,
		1.0f);
	if (Wetness <= KINDA_SMALL_NUMBER)
	{
		if (WetnessVisualPool.IsValidIndex(Zone.WetnessVisualIndex))
		{
			WetnessVisualPool[Zone.WetnessVisualIndex]->SetVisibility(false);
			WetnessVisualPool[Zone.WetnessVisualIndex]->SetHiddenInGame(true);
			FreeWetnessVisuals.AddUnique(Zone.WetnessVisualIndex);
			Zone.WetnessVisualIndex = INDEX_NONE;
		}
		return;
	}
	if (Zone.WetnessVisualIndex == INDEX_NONE && FreeWetnessVisuals.Num() > 0)
	{
		Zone.WetnessVisualIndex = FreeWetnessVisuals.Pop(EAllowShrinking::No);
	}
	if (!WetnessVisualPool.IsValidIndex(Zone.WetnessVisualIndex))
	{
		return;
	}

	UDecalComponent* Decal = WetnessVisualPool[Zone.WetnessVisualIndex];
	const float Radius = FMath::Lerp(
		Settings.MinimumWetnessRadius,
		Settings.MaximumWetnessRadius,
		FMath::Sqrt(Wetness));
	Decal->SetWorldLocation(Zone.Location + Zone.Normal * 1.0f);
	Decal->SetWorldRotation((-Zone.Normal).Rotation());
	Decal->DecalSize = FVector(5.0f, Radius, Radius);
	Decal->SetVisibility(true);
	Decal->SetHiddenInGame(false);
	if (WetnessMaterials.IsValidIndex(Zone.WetnessVisualIndex))
	{
		WetnessMaterials[Zone.WetnessVisualIndex]->SetScalarParameterValue(
			TEXT("WetnessAmount"),
			Wetness);
		WetnessMaterials[Zone.WetnessVisualIndex]->SetScalarParameterValue(
			TEXT("ShapeVariant"),
			static_cast<float>(Zone.ShapeVariant));
	}
}

void UBotanicusWaterSubsystem::UpdatePuddleVisual(
	FWetZone& Zone,
	float DeltaTime,
	const UBotanicusWaterSettings& Settings)
{
	const float MaximumWater = FMath::Max(
		KINDA_SMALL_NUMBER,
		Settings.MaximumAccumulatedWater);
	const float VisualWaterAlpha = Zone.bCanCreatePuddle
		? FMath::Clamp(Zone.AccumulatedWater / MaximumWater, 0.0f, 1.0f)
		: 0.0f;
	const float FunctionalThreshold = FMath::Clamp(
		Settings.PuddleThreshold,
		KINDA_SMALL_NUMBER,
		MaximumWater);
	const float MinimumRadius = FMath::Max(0.5f, Settings.MinimumPuddleRadius);
	const float MaximumRadius = FMath::Max(
		MinimumRadius,
		Settings.MaximumPuddleRadius);
	const float InitialRadius = FMath::Clamp(
		Settings.InitialPuddleRadius,
		0.5f,
		MinimumRadius);
	float DesiredRadius = 0.0f;
	if (VisualWaterAlpha > 0.0f)
	{
		if (Zone.AccumulatedWater < FunctionalThreshold)
		{
			const float FormationAlpha = FMath::Sqrt(FMath::Clamp(
				Zone.AccumulatedWater / FunctionalThreshold,
				0.0f,
				1.0f));
			DesiredRadius = FMath::Lerp(
				InitialRadius,
				MinimumRadius,
				FormationAlpha);
		}
		else
		{
			const float PuddleRange = FMath::Max(
				KINDA_SMALL_NUMBER,
				MaximumWater - FunctionalThreshold);
			const float AccumulatedPuddleAlpha = FMath::Clamp(
				(Zone.AccumulatedWater - FunctionalThreshold) / PuddleRange,
				0.0f,
				1.0f);
			DesiredRadius = FMath::Lerp(
				MinimumRadius,
				MaximumRadius,
				FMath::Sqrt(AccumulatedPuddleAlpha));
		}
	}
	const float InterpolationSpeed = DesiredRadius > Zone.CurrentPuddleRadius
		? Settings.PuddleGrowthSpeed
		: Settings.PuddleShrinkSpeed;
	Zone.CurrentPuddleRadius = DeltaTime > 0.0f
		? FMath::FInterpTo(
			Zone.CurrentPuddleRadius,
			DesiredRadius,
			DeltaTime,
			FMath::Max(0.01f, InterpolationSpeed))
		: Zone.CurrentPuddleRadius;

	if (DesiredRadius > 0.0f && Zone.PuddleVisualIndex == INDEX_NONE &&
		FreePuddleVisuals.Num() > 0)
	{
		Zone.PuddleVisualIndex = FreePuddleVisuals.Pop(EAllowShrinking::No);
		Zone.CurrentPuddleRadius = FMath::Min(DesiredRadius, InitialRadius);
	}
	if (!PuddleVisualPool || Zone.PuddleVisualIndex == INDEX_NONE)
	{
		return;
	}
	if (DesiredRadius <= 0.0f && Zone.CurrentPuddleRadius <= 0.25f)
	{
		PuddleVisualPool->UpdateInstanceTransform(
			Zone.PuddleVisualIndex,
			FTransform(FQuat::Identity, Zone.Location, FVector::ZeroVector),
			true,
			true,
			true);
		FreePuddleVisuals.AddUnique(Zone.PuddleVisualIndex);
		Zone.PuddleVisualIndex = INDEX_NONE;
		return;
	}

	const float DiameterScale = Zone.CurrentPuddleRadius * 2.0f / 100.0f;
	const FQuat AlignToSurface = FRotationMatrix::MakeFromZ(Zone.Normal).ToQuat();
	const FQuat ShapeRotation(
		Zone.Normal,
		FMath::DegreesToRadians(Zone.ShapeRotationDegrees));
	const FTransform Transform(
		ShapeRotation * AlignToSurface,
		Zone.Location + Zone.Normal * 0.8f,
		FVector(
			DiameterScale * Zone.ShapeScale.X,
			DiameterScale * Zone.ShapeScale.Y,
			1.0f));
	PuddleVisualPool->UpdateInstanceTransform(
		Zone.PuddleVisualIndex,
		Transform,
		true,
		false,
		true);
	PuddleVisualPool->SetCustomDataValue(
		Zone.PuddleVisualIndex,
		PuddleCustomDataWater,
		VisualWaterAlpha,
		false);
	PuddleVisualPool->SetCustomDataValue(
		Zone.PuddleVisualIndex,
		PuddleCustomDataImpactPulse,
		Zone.ImpactPulse,
		false);
	PuddleVisualPool->SetCustomDataValue(
		Zone.PuddleVisualIndex,
		PuddleCustomDataShape,
		static_cast<float>(Zone.ShapeVariant) /
			static_cast<float>(MaximumPuddleShapeVariants - 1),
		true);
}

void UBotanicusWaterSubsystem::TriggerImpactEffect(
	const FVector& Location,
	const FVector& Normal)
{
	if (ImpactEffectPool.Num() == 0)
	{
		return;
	}
	UNiagaraComponent* Effect = ImpactEffectPool[
		NextImpactEffect++ % ImpactEffectPool.Num()];
	Effect->SetWorldLocationAndRotation(
		Location,
		FRotationMatrix::MakeFromZ(Normal.GetSafeNormal()).Rotator());
	Effect->Activate(true);
}

void UBotanicusWaterSubsystem::TriggerRunoffEffect(
	const FVector& Location,
	const FVector& Normal)
{
	if (RunoffEffectPool.Num() == 0)
	{
		return;
	}
	UNiagaraComponent* Effect = RunoffEffectPool[
		NextRunoffEffect++ % RunoffEffectPool.Num()];
	const FVector GravityDirection = FVector::DownVector;
	const FVector AlongSurface = FVector::VectorPlaneProject(
		GravityDirection,
		Normal).GetSafeNormal(SMALL_NUMBER, GravityDirection);
	Effect->SetWorldLocationAndRotation(
		Location + Normal * 1.0f,
		FRotationMatrix::MakeFromZ(AlongSurface).Rotator());
	Effect->Activate(true);
}
