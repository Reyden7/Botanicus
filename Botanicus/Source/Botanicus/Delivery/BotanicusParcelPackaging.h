#pragma once

#include "CoreMinimal.h"
#include "BotanicusParcelPackaging.generated.h"

UENUM(BlueprintType)
enum class EBotanicusParcelShape : uint8
{
	Automatic, Square, Rectangular
};

/** Physical properties resolved at dispatch time, in cm and kg. */
USTRUCT()
struct FBotanicusParcelPackaging
{
	GENERATED_BODY()

	UPROPERTY()
	EBotanicusParcelShape Shape = EBotanicusParcelShape::Automatic;
	UPROPERTY()
	FVector HalfExtent = FVector::ZeroVector;
	UPROPERTY()
	float MassKg = 0.f;

	bool IsValid() const
	{
		return (Shape == EBotanicusParcelShape::Square || Shape == EBotanicusParcelShape::Rectangular) &&
			!HalfExtent.ContainsNaN() && HalfExtent.GetMin() > 0.0 && FMath::IsFinite(MassKg) && MassKg > 0.f;
	}
};
