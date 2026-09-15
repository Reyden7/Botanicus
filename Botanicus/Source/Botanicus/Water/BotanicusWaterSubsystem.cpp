// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/BotanicusWaterSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Water/BotanicusWaterSettings.h"
#include "Water/BotanicusWaterTrajectory.h"

namespace
{
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
		if (Zone.AccumulatedWater <= KINDA_SMALL_NUMBER)
		{
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
