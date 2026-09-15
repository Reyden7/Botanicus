// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BotanicusWaterTypes.generated.h"

class AActor;
class UPhysicalMaterial;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EBotanicusWaterSurfaceClass : uint8
{
	Horizontal,
	Inclined,
	Vertical
};

/** Complete description of one lightweight ballistic water stream. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusWaterStreamParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Water")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm/s"))
	float InitialSpeed = 700.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm/s^2"))
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm"))
	float Radius = 7.0f;

	/** Normalized reservoir units emitted per second. */
	UPROPERTY(BlueprintReadOnly, Category="Water")
	float FlowRate = 0.12f;

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="s"))
	float MaxSimulationTime = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm"))
	float MaxDistance = 1600.0f;

	/** Preferred chord length. Curvature can force shorter segments. */
	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm"))
	float TargetSegmentLength = 70.0f;

	/** Maximum ballistic sag accepted between a chord and the curve. */
	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm"))
	float MaxCurveDeviation = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	int32 MaxTraceSegments = 24;
};

/** Gameplay result produced by the first blocking collision of a water stream. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusWaterHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Water")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	FVector Normal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm/s"))
	FVector ImpactVelocity = FVector::ZeroVector;

	/** Ballistic time at which the first blocking hit occurred. */
	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="s"))
	float TimeOfImpact = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water", meta=(Units="cm"))
	float DistanceToImpact = 0.0f;

	/** Amount emitted during the simulation step that produced this hit. */
	UPROPERTY(BlueprintReadOnly, Category="Water")
	float Amount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	TObjectPtr<AActor> HitActor;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	TObjectPtr<UPrimitiveComponent> HitComponent;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	TObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	TObjectPtr<AActor> SourceActor;
};

/** Conserved distribution of one emitted amount after a surface impact. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusWaterReceiveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Water")
	float Emitted = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	float Absorbed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	float Retained = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	float Runoff = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	float LostOrSplash = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	EBotanicusWaterSurfaceClass SurfaceClass =
		EBotanicusWaterSurfaceClass::Horizontal;

	UPROPERTY(BlueprintReadOnly, Category="Water")
	bool bCanCreatePuddle = false;

	float GetDistributedTotal() const
	{
		return Absorbed + Retained + Runoff + LostOrSplash;
	}
};
