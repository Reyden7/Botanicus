// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/BotanicusWaterTrajectory.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBotanicusWaterDebug(
		TEXT("botanicus.Water.Debug"),
		0,
		TEXT("Draw authoritative Botanicus water trajectories and impacts."),
		ECVF_Cheat);

	constexpr float DebugLifetimeSeconds = 0.12f;
	constexpr float MinimumTimeStep = 0.0025f;
}

FVector BotanicusWaterTrajectory::PositionAtTime(
	const FBotanicusWaterStreamParams& Params,
	float Time)
{
	const float SafeTime = FMath::Max(0.0f, Time);
	const FVector InitialVelocity =
		Params.Direction.GetSafeNormal() * FMath::Max(0.0f, Params.InitialSpeed);
	return Params.Origin + InitialVelocity * SafeTime +
		0.5f * Params.Gravity * SafeTime * SafeTime;
}

FVector BotanicusWaterTrajectory::VelocityAtTime(
	const FBotanicusWaterStreamParams& Params,
	float Time)
{
	const float SafeTime = FMath::Max(0.0f, Time);
	return Params.Direction.GetSafeNormal() * FMath::Max(0.0f, Params.InitialSpeed) +
		Params.Gravity * SafeTime;
}

void BotanicusWaterTrajectory::BuildSamples(
	const FBotanicusWaterStreamParams& Params,
	TArray<FBotanicusWaterTrajectorySample>& OutSamples)
{
	OutSamples.Reset();
	FBotanicusWaterTrajectorySample& Initial = OutSamples.AddDefaulted_GetRef();
	Initial.Position = Params.Origin;
	Initial.Velocity = VelocityAtTime(Params, 0.0f);

	const float MaximumTime = FMath::Max(0.0f, Params.MaxSimulationTime);
	const float MaximumDistance = FMath::Max(0.0f, Params.MaxDistance);
	const int32 MaximumSegments = FMath::Clamp(Params.MaxTraceSegments, 1, 64);
	if (MaximumTime <= KINDA_SMALL_NUMBER ||
		MaximumDistance <= KINDA_SMALL_NUMBER ||
		Params.InitialSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float TargetLength = FMath::Max(5.0f, Params.TargetSegmentLength);
	const float GravityMagnitude = Params.Gravity.Size();
	const float CurvatureTimeStep = GravityMagnitude > KINDA_SMALL_NUMBER
		? FMath::Sqrt(
			8.0f * FMath::Max(0.1f, Params.MaxCurveDeviation) /
			GravityMagnitude)
		: MaximumTime;

	float CurrentTime = 0.0f;
	float TravelledDistance = 0.0f;
	for (int32 SegmentIndex = 0;
		 SegmentIndex < MaximumSegments && CurrentTime < MaximumTime;
		 ++SegmentIndex)
	{
		const float CurrentSpeed = FMath::Max(
			1.0f,
			VelocityAtTime(Params, CurrentTime).Size());
		const float LengthTimeStep = TargetLength / CurrentSpeed;
		const float TimeStep = FMath::Max(
			MinimumTimeStep,
			FMath::Min3(
				LengthTimeStep,
				CurvatureTimeStep,
				MaximumTime - CurrentTime));
		const float NextTime = FMath::Min(CurrentTime + TimeStep, MaximumTime);
		float EffectiveNextTime = NextTime;
		const FVector PreviousPosition = OutSamples.Last().Position;
		FVector NextPosition = PositionAtTime(Params, NextTime);
		float SegmentLength = FVector::Distance(PreviousPosition, NextPosition);
		const float RemainingDistance = MaximumDistance - TravelledDistance;
		if (SegmentLength > RemainingDistance)
		{
			const float Alpha = RemainingDistance /
				FMath::Max(KINDA_SMALL_NUMBER, SegmentLength);
			NextPosition = FMath::Lerp(PreviousPosition, NextPosition, Alpha);
			EffectiveNextTime = FMath::Lerp(CurrentTime, NextTime, Alpha);
			SegmentLength = RemainingDistance;
		}

		FBotanicusWaterTrajectorySample& Sample =
			OutSamples.AddDefaulted_GetRef();
		Sample.Time = EffectiveNextTime;
		Sample.Position = NextPosition;
		Sample.Velocity = VelocityAtTime(Params, EffectiveNextTime);
		CurrentTime = EffectiveNextTime;
		TravelledDistance += SegmentLength;
		if (TravelledDistance >= MaximumDistance - KINDA_SMALL_NUMBER)
		{
			break;
		}
	}
}

bool BotanicusWaterTrajectory::TraceFirstBlockingHit(
	UWorld* World,
	const FBotanicusWaterStreamParams& Params,
	AActor* SourceActor,
	const TArray<const AActor*>& IgnoredActors,
	float EmittedAmount,
	FBotanicusWaterHit& OutWaterHit,
	TArray<FBotanicusWaterTrajectorySample>* OutTrajectorySamples)
{
	OutWaterHit = FBotanicusWaterHit();
	if (OutTrajectorySamples)
	{
		OutTrajectorySamples->Reset();
	}
	if (!World)
	{
		return false;
	}

	TArray<FBotanicusWaterTrajectorySample> LocalSamples;
	TArray<FBotanicusWaterTrajectorySample>& Samples = OutTrajectorySamples
		? *OutTrajectorySamples
		: LocalSamples;
	BuildSamples(Params, Samples);
	if (Samples.Num() < 2)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusWaterTrajectory),
		false);
	QueryParams.bReturnPhysicalMaterial = true;
	if (IsValid(SourceActor))
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}
	for (const AActor* IgnoredActor : IgnoredActors)
	{
		if (IsValid(IgnoredActor))
		{
			QueryParams.AddIgnoredActor(IgnoredActor);
		}
	}

	const bool bDebug = IsDebugEnabled();
	const float Radius = FMath::Max(0.1f, Params.Radius);
	if (bDebug)
	{
		DrawDebugPoint(
			World,
			Params.Origin,
			14.0f,
			FColor::Cyan,
			false,
			DebugLifetimeSeconds);
	}

	float DistanceToImpact = 0.0f;
	for (int32 Index = 1; Index < Samples.Num(); ++Index)
	{
		const FBotanicusWaterTrajectorySample& Previous = Samples[Index - 1];
		const FBotanicusWaterTrajectorySample& Current = Samples[Index];
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(
			Hit,
			Previous.Position,
			Current.Position,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(Radius),
			QueryParams);

		if (bDebug)
		{
			DrawDebugLine(
				World,
				Previous.Position,
				bHit ? Hit.ImpactPoint : Current.Position,
				bHit ? FColor::Red : FColor::Cyan,
				false,
				DebugLifetimeSeconds,
				0,
				2.0f);
			DrawDebugSphere(
				World,
				Previous.Position,
				Radius,
				8,
				FColor::Blue,
				false,
				DebugLifetimeSeconds,
				0,
				0.75f);
		}

		if (!bHit)
		{
			DistanceToImpact += FVector::Distance(
				Previous.Position,
				Current.Position);
			continue;
		}

		const float SegmentLength = FVector::Distance(
			Previous.Position,
			Current.Position);
		const float HitAlpha = SegmentLength > KINDA_SMALL_NUMBER
			? FMath::Clamp(
				FVector::Distance(Previous.Position, Hit.ImpactPoint) /
				SegmentLength,
				0.0f,
				1.0f)
			: 0.0f;
		const float ImpactTime = FMath::Lerp(Previous.Time, Current.Time, HitAlpha);
		DistanceToImpact += FVector::Distance(
			Previous.Position,
			Hit.ImpactPoint);

		OutWaterHit.Location = Hit.ImpactPoint;
		OutWaterHit.Normal = Hit.ImpactNormal.GetSafeNormal();
		OutWaterHit.ImpactVelocity = VelocityAtTime(Params, ImpactTime);
		OutWaterHit.TimeOfImpact = ImpactTime;
		OutWaterHit.DistanceToImpact = DistanceToImpact;
		OutWaterHit.Amount = FMath::Max(0.0f, EmittedAmount);
		OutWaterHit.HitActor = Hit.GetActor();
		OutWaterHit.HitComponent = Hit.GetComponent();
		OutWaterHit.PhysicalMaterial = Hit.PhysMaterial.Get();
		OutWaterHit.SourceActor = SourceActor;

		if (bDebug)
		{
			DrawDebugPoint(
				World,
				OutWaterHit.Location,
				18.0f,
				FColor::Red,
				false,
				DebugLifetimeSeconds);
			DrawDebugDirectionalArrow(
				World,
				OutWaterHit.Location,
				OutWaterHit.Location + OutWaterHit.Normal * 55.0f,
				14.0f,
				FColor::Yellow,
				false,
				DebugLifetimeSeconds,
				0,
				2.0f);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					static_cast<uint64>(-7610),
					0.15f,
					FColor::Cyan,
					FString::Printf(
						TEXT("Water Source=%s  Flow=%.3f/s  Speed=%.0f cm/s\nHit=%s  Distance=%.0f cm  ImpactVelocity=%.0f cm/s"),
						*GetNameSafe(SourceActor),
						Params.FlowRate,
						Params.InitialSpeed,
						*GetNameSafe(OutWaterHit.HitActor),
						DistanceToImpact,
						OutWaterHit.ImpactVelocity.Size()));
			}
		}
		return true;
	}

	if (bDebug)
	{
		const FBotanicusWaterTrajectorySample& Last = Samples.Last();
		DrawDebugSphere(
			World,
			Last.Position,
			Radius,
			8,
			FColor::Blue,
			false,
			DebugLifetimeSeconds,
			0,
			0.75f);
	}
	return false;
}

bool BotanicusWaterTrajectory::IsDebugEnabled()
{
	return CVarBotanicusWaterDebug.GetValueOnAnyThread() != 0;
}
