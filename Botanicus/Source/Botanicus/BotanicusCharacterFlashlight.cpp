#include "BotanicusCharacter.h"

#include "BotanicusGameState.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/BotanicusAmbienceDirector.h"
#include "Environment/BotanicusDayNightMath.h"
#include "Environment/BotanicusSeasonSettings.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	constexpr float FlashlightSampleInterval = 0.15f;
	constexpr float CoveredDaylight = 0.28f;
	constexpr float NearbyIndoorLight = 0.85f;

	/** A presentation estimate. It has no influence on cultivation or eye adaptation. */
	float EstimateAmbientBrightness(
		const UWorld& World, const AActor& Player, const FVector& Sample, const ABotanicusGameState& State,
		const ABotanicusAmbienceDirector* Ambience)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(HeadFlashlightAmbient), false, &Player);
		const FVector SkyRays[] = {{0, 0, 1000}, {140, 80, 1000}, {-140, -80, 1000}};
		int32 OpenRays = 0;
		for (const FVector& Ray : SkyRays)
		{
			OpenRays += !World.LineTraceTestByChannel(Sample, Sample + Ray, ECC_Visibility, Query);
		}
		const float SkyAccess = float(OpenRays) / UE_ARRAY_COUNT(SkyRays);
		const float Outdoor = FMath::Clamp(State.GetOutdoorLuminosityPercent() / 100.f, 0.f, 1.f);
		float Brightness = Outdoor * FMath::Lerp(CoveredDaylight, 1.f, SkyAccess);
		if (!Ambience || SkyAccess >= 1.f)
			return Brightness;

		// The ambience director is presentation-only on dedicated servers.
		// Use its authored indoor group and the shared shop state on every peer.
		const float ShopWeight = State.IsMainShopOpen() ? 1.f :
			FMath::Clamp(Ambience->StandbyIntensity / FMath::Max(1.f, Ambience->InteriorDayIntensity), 0.f, 1.f);
		for (const ALight* Actor : Ambience->InteriorLights)
		{
			if (!IsValid(Actor)) continue;
			const auto* Light = Cast<ULocalLightComponent>(Actor->GetLightComponent());
			if (!Light || !Light->IsVisible() || Light->AttenuationRadius <= 0.f) continue;

			const FVector FromLight = Sample - Light->GetComponentLocation();
			const double DistanceSq = FromLight.SizeSquared();
			const double RangeSq = FMath::Square(Light->AttenuationRadius);
			if (DistanceSq >= RangeSq) continue;
			float ConeWeight = 1.f;
			if (const auto* Spot = Cast<USpotLightComponent>(Light))
			{
				const double CosAngle = FVector::DotProduct(FromLight.GetSafeNormal(), Spot->GetForwardVector());
				const double Outer = FMath::Cos(FMath::DegreesToRadians(double(Spot->OuterConeAngle)));
				const double Inner = FMath::Cos(FMath::DegreesToRadians(double(Spot->InnerConeAngle)));
				ConeWeight = FMath::SmoothStep(Outer, FMath::Max(Outer + .001, Inner), CosAngle);
			}
			const float Contribution = NearbyIndoorLight * ShopWeight * (1.f - DistanceSq / RangeSq) * ConeWeight;
			if (Contribution <= Brightness) continue;
			FCollisionQueryParams LightQuery = Query;
			LightQuery.AddIgnoredActor(Actor);
			if (!World.LineTraceTestByChannel(Sample, Light->GetComponentLocation(), ECC_Visibility, LightQuery))
			{
				Brightness = Contribution;
			}
		}
		return Brightness;
	}
}

void ABotanicusCharacter::InitializeHeadFlashlight()
{
	// Preserve the component's authored intensity, units, cone, range, color and relative transform.
	FlashlightFullIntensity = HeadFlashlight->Intensity;
	HeadFlashlight->SetIntensity(0.f);
	HeadFlashlight->SetVisibility(false);
	ConfigureFlashlightSampling();
}

void ABotanicusCharacter::ToggleHeadFlashlight()
{
	if (!IsLocallyControlled() || !Cast<APlayerController>(GetController()))
		return;

	const bool bWasOn = HasAuthority() ? bHeadFlashlightOn : bPredictedHeadFlashlightOn;
	ManualFlashlightOverride = !bWasOn;
	SampleHeadFlashlight();
	if (!HasAuthority())
		ServerSetManualFlashlightOverride(!bWasOn);
}

void ABotanicusCharacter::ServerSetManualFlashlightOverride_Implementation(bool bOn)
{
	if (!Cast<APlayerController>(GetController()))
		return;

	ManualFlashlightOverride = bOn;
	SampleHeadFlashlight();
}

void ABotanicusCharacter::ConfigureFlashlightSampling()
{
	GetWorldTimerManager().ClearTimer(FlashlightSampleTimer);
	if (Cast<APlayerController>(GetController()) && (HasAuthority() || IsLocallyControlled()))
	{
		for (TActorIterator<ABotanicusAmbienceDirector> It(GetWorld()); It; ++It)
		{
			FlashlightAmbience = *It;
			break;
		}
		GetWorldTimerManager().SetTimer(FlashlightSampleTimer, this,
			&ABotanicusCharacter::SampleHeadFlashlight, FlashlightSampleInterval, true);
	}
	SampleHeadFlashlight();
}

void ABotanicusCharacter::SampleHeadFlashlight()
{
	if (!HasAuthority() && !IsLocallyControlled())
		return;

	const bool bHasPlayerController = Cast<APlayerController>(GetController()) != nullptr;
	bool bNextOn = bHasPlayerController && ManualFlashlightOverride.Get(false);
	const auto* State = GetWorld()->GetGameState<ABotanicusGameState>();
	if (bHasPlayerController && !ManualFlashlightOverride.IsSet() && bAutoFlashlightEnabled && State)
	{
		const auto* Seasons = GetDefault<UBotanicusSeasonSettings>();
		const float Minute = State->GetDayTimeMinutes();
		const auto& Profile = Seasons->GetProfile(State->GetCurrentSeason());
		const auto Phase = BotanicusDayNight::Evaluate(Minute, Profile.SunriseTimeMinutes, Profile.SunsetTimeMinutes);
		if (Phase.Daylight <= KINDA_SMALL_NUMBER)
		{
			bNextOn = true;
		}
		else
		{
			const float Brightness = EstimateAmbientBrightness(*GetWorld(), *this,
				FirstPersonCameraComponent->GetComponentLocation(), *State, FlashlightAmbience.Get());
			const bool bWasOn = HasAuthority() ? bHeadFlashlightOn : bPredictedHeadFlashlightOn;
			const float Margin = bWasOn ? BrightnessHysteresis : -BrightnessHysteresis;
			const float Threshold = FMath::Clamp(BrightnessThreshold + Margin, 0.f, 1.f);
			bNextOn = bWasOn ? Brightness <= Threshold : Brightness < Threshold;
		}
	}

	if (HasAuthority())
	{
		const float ViewYaw = GetControlRotation().Yaw;
		const bool bNeedsYawOverride = bNextOn &&
			FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, ViewYaw)) > 1.f;
		// Quantized yaw only while needed, sampled alongside ambience rather than sent each frame.
		FlashlightYawOverride = bNeedsYawOverride ? FRotator::CompressAxisToByte(ViewYaw) : NoFlashlightYawOverride;
		if (bHeadFlashlightOn != bNextOn)
		{
			bHeadFlashlightOn = bNextOn;
			ForceNetUpdate();
		}
	}
	bPredictedHeadFlashlightOn = bNextOn;
}

void ABotanicusCharacter::UpdateHeadFlashlight(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
		return;

	const bool bOn = IsLocallyControlled() && !HasAuthority() ? bPredictedHeadFlashlightOn : bHeadFlashlightOn;
	if (!bOn && FlashlightFadeAlpha <= 0.f)
		return;

	// The pivot shares the first-person camera's position and leaves its settings intact.
	// Observers use the existing replicated pawn yaw and remote view pitch from GetBaseAimRotation.
	FRotator Aim = GetController() ? GetControlRotation() : GetBaseAimRotation();
	if (GetLocalRole() == ROLE_SimulatedProxy && FlashlightYawOverride != NoFlashlightYawOverride)
	{
		Aim.Yaw = FRotator::DecompressAxisFromByte(FlashlightYawOverride);
		if (FlashlightFadeAlpha > 0.f)
			Aim.Yaw = FMath::FixedTurn(HeadFlashlightAim->GetComponentRotation().Yaw, Aim.Yaw, DeltaSeconds * 1080.f);
	}
	HeadFlashlightAim->SetWorldRotation(FRotator(Aim.Pitch, Aim.Yaw, 0.f));
	const float Target = bOn ? 1.f : 0.f;
	const float Previous = FlashlightFadeAlpha;
	FlashlightFadeAlpha = FlashlightFadeDuration > UE_SMALL_NUMBER ?
		FMath::FInterpConstantTo(Previous, Target, DeltaSeconds, 1.f / FlashlightFadeDuration) : Target;
	if (FlashlightFadeAlpha != Previous)
	{
		HeadFlashlight->SetIntensity(FlashlightFullIntensity * FlashlightFadeAlpha);
		HeadFlashlight->SetVisibility(FlashlightFadeAlpha > 0.f);
	}
}
