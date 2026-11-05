#pragma once

#include "CoreMinimal.h"

/** Presentation only. None of these values feed the horticultural simulation. */
namespace BotanicusDayNight
{
	struct FPhase
	{
		float SolarAngle = 0.0f;
		float Daylight = 0.0f;
		float SkyBlend = 0.0f;
	};

	inline float WrapMinutes(float Minutes)
	{
		return FMath::Fmod(FMath::Fmod(Minutes, 1440.0f) + 1440.0f, 1440.0f);
	}

	inline float SignedMinuteDelta(float From, float To)
	{
		return WrapMinutes(To - From + 720.0f) - 720.0f;
	}

	inline float SmoothMinutes(float Current, float Target, float DeltaSeconds,
		float ResponseSeconds, float SnapThreshold)
	{
		const float Difference = SignedMinuteDelta(Current, Target);
		if (ResponseSeconds <= 0.0f || FMath::Abs(Difference) >= SnapThreshold)
		{
			return WrapMinutes(Target);
		}
		const float Alpha = 1.0f - FMath::Exp(-FMath::Max(0.0f, DeltaSeconds) / ResponseSeconds);
		return WrapMinutes(Current + Difference * Alpha);
	}

	inline FPhase Evaluate(float Minutes, float Sunrise, float Sunset)
	{
		Sunrise = FMath::Clamp(Sunrise, 0.0f, 1438.0f);
		Sunset = FMath::Clamp(Sunset, Sunrise + 1.0f, 1439.0f);
		Minutes = WrapMinutes(Minutes);
		FPhase Result;
		if (Minutes >= Sunrise && Minutes <= Sunset)
		{
			Result.SolarAngle = PI * (Minutes - Sunrise) / (Sunset - Sunrise);
		}
		else
		{
			const float SinceSunset = WrapMinutes(Minutes - Sunset);
			Result.SolarAngle = PI + PI * SinceSunset / (1440.0f - Sunset + Sunrise);
		}
		const float Height = FMath::Sin(Result.SolarAngle);
		Result.Daylight = FMath::Pow(FMath::Max(0.0f, Height), 0.7f);
		Result.SkyBlend = FMath::SmoothStep(-0.12f, 0.2f, Height);
		return Result;
	}
}
