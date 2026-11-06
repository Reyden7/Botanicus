#include "Environment/BotanicusAmbienceDirector.h"

#include "Botanicus.h"
#include "Visuals/BotanicusNoctifloreSettings.h"
#include "BotanicusGameState.h"
#include "Environment/BotanicusDayNightMath.h"
#include "Environment/BotanicusSeasonSettings.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

namespace
{
	template<typename TProperty, typename TValue>
	void SetSkyProperty(AActor* Sky, const TCHAR* Name, TValue Value)
	{
		if (TProperty* Property = FindFProperty<TProperty>(Sky->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Sky, Value);
		}
	}

	void SetSkyNumber(AActor* Sky, const TCHAR* Name, double Value)
	{
		// UE5 Blueprint real variables may be doubles, including this legacy sky.
		if (FNumericProperty* Property = FindFProperty<FNumericProperty>(Sky->GetClass(), Name))
		{
			if (Property->IsFloatingPoint())
			{
				Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Sky), Value);
			}
		}
	}
}

ABotanicusAmbienceDirector::ABotanicusAmbienceDirector()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AmbienceRoot")));
}

void ABotanicusAmbienceDirector::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	InitializeBindings();
	RefreshFromGameState(0.0f);
}

void ABotanicusAmbienceDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshFromGameState(DeltaSeconds);
}

void ABotanicusAmbienceDirector::InitializeBindings()
{
	if (bBindingsInitialized) return;
	bBindingsInitialized = true;
	if (IsValid(Sun)) Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	if (IsValid(LegacySkySphere))
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(LegacySkySphere->GetClass(), TEXT("Directional light actor")))
		{
			Property->SetObjectPropertyValue_InContainer(LegacySkySphere, Sun.Get());
		}
		SetSkyProperty<FBoolProperty>(LegacySkySphere, TEXT("Colors determined by sun position"), true);
		UFunction* Function = LegacySkySphere->FindFunction(TEXT("UpdateSunDirection"));
		if (Function && Function->ParmsSize == 0) SkyUpdateFunction = Function;
		else UE_LOG(LogBotanicus, Warning, TEXT("Ambience: bound sky has no parameterless UpdateSunDirection; inspect its adapter."));
	}
	ApplyExposureSettings();
}

void ABotanicusAmbienceDirector::ApplyExposureSettings()
{
	if (!bManageExposure || !IsValid(ExposureVolume)) return;
	FPostProcessSettings& Settings = ExposureVolume->Settings;
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Histogram;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMinBrightness = FMath::Min(MinimumEV100, MaximumEV100);
	Settings.AutoExposureMaxBrightness = FMath::Max(Settings.AutoExposureMinBrightness + 0.01f, FMath::Max(MinimumEV100, MaximumEV100));
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = ExposureCompensation;
	Settings.bOverride_AutoExposureSpeedUp = true;
	Settings.bOverride_AutoExposureSpeedDown = true;
	Settings.AutoExposureSpeedUp = FMath::Max(0.1f, ExposureSpeedUp);
	Settings.AutoExposureSpeedDown = FMath::Max(0.1f, ExposureSpeedDown);
	Settings.bOverride_LocalExposureHighlightContrastScale = true;
	Settings.bOverride_LocalExposureShadowContrastScale = true;
	Settings.LocalExposureHighlightContrastScale = LocalHighlightContrast;
	Settings.LocalExposureShadowContrastScale = LocalShadowContrast;
}

void ABotanicusAmbienceDirector::RefreshFromGameState(float DeltaSeconds)
{
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return;
	const ABotanicusGameState* State = GetWorld()->GetGameState<ABotanicusGameState>();
	if (!State) return; // Clients may not have their initial GameState yet.
	InitializeBindings();
	const float Target = State->GetDayTimeMinutes();
	const bool bFirstSample = !bHasTimeSample;
	const bool bJump = bHasTimeSample && FMath::Abs(BotanicusDayNight::SignedMinuteDelta(VisualMinutes, Target)) >= SnapThresholdMinutes;
	VisualMinutes = bFirstSample ? Target : BotanicusDayNight::SmoothMinutes(
		VisualMinutes, Target, DeltaSeconds, VisualResponseSeconds, SnapThresholdMinutes);
	bHasTimeSample = true;
    const auto* Noct=GetDefault<UBotanicusNoctifloreSettings>();
    const float TargetWeight=State->IsIllegalTradeActive() ? 1.f : 0.f;
    // First sample / late join is correct immediately; subsequent activation fades.
    const float Response=FMath::Clamp(Noct->LightTransitionSeconds,0.f,5.f);
    ClandestineWeight=!bClandestineSample || !Noct->bEnabled || Response<=0.f ? TargetWeight :
        FMath::Lerp(ClandestineWeight,TargetWeight,1.f-FMath::Exp(-3.f*FMath::Max(0.f,DeltaSeconds)/Response));
    bClandestineSample=true;
    ApplyPresentation(VisualMinutes, State->GetCurrentDayNumber(), State->IsIllegalTradeActive(), bFirstSample || bJump);
}

void ABotanicusAmbienceDirector::PreviewAtHour(float Hour, int32 DayNumber, bool bClandestine)
{
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		InitializeBindings();
		ApplyExposureSettings();
		ClandestineWeight=bClandestine ? 1.f : 0.f;
		ApplyPresentation(BotanicusDayNight::WrapMinutes(Hour * 60.0f), DayNumber, bClandestine, true);
	}
#endif
}

void ABotanicusAmbienceDirector::ApplyPresentation(float Minutes, int32 DayNumber, bool bClandestine, bool bForceSky)
{
	const UBotanicusSeasonSettings* Seasons = GetDefault<UBotanicusSeasonSettings>();
	const EBotanicusSeason Season = Seasons->EvaluateEnvironment(DayNumber, Minutes).Season;
	const FBotanicusSeasonEnvironmentProfile& Profile = Seasons->GetProfile(Season);
	const BotanicusDayNight::FPhase Phase = BotanicusDayNight::Evaluate(Minutes, Profile.SunriseTimeMinutes, Profile.SunsetTimeMinutes);
	if (IsValid(Sun))
	{
		const float Tilt = FMath::DegreesToRadians(FMath::Clamp(MaximumSunElevation, 5.0f, 85.0f));
		const FVector Direction = FRotator(0.0f, SolarAzimuth, 0.0f).RotateVector(FVector(
			FMath::Cos(Phase.SolarAngle), FMath::Cos(Tilt) * FMath::Sin(Phase.SolarAngle),
			-FMath::Sin(Tilt) * FMath::Sin(Phase.SolarAngle)));
		Sun->SetActorRotation(Direction.Rotation());
		ULightComponent* Light = Sun->GetLightComponent();
		const float IntensityFactor = SunIntensityCurve ? FMath::Max(0.0f, SunIntensityCurve->GetFloatValue(Phase.Daylight)) : Phase.Daylight;
		// Even an accidentally nonzero curve endpoint cannot create a nocturnal sun.
		Light->SetIntensity(Phase.Daylight > KINDA_SMALL_NUMBER ? DaySunIntensity * IntensityFactor : 0.0f);
		Light->SetLightColor(FMath::Lerp(HorizonSunColor, NoonSunColor, FMath::Clamp(Phase.Daylight * 2.0f, 0.0f, 1.0f)));
	}
	if (IsValid(SkyLight))
	{
		USkyLightComponent* Light = SkyLight->GetLightComponent();
		Light->SetIntensity(FMath::Lerp(NightSkyIntensity, DaySkyIntensity, Phase.SkyBlend));
		Light->SetLightColor(FMath::Lerp(NightSkyTint, FLinearColor::White, Phase.SkyBlend));
		// No RecaptureSky, cubemap changes or real-time captures in this update.
	}
	if (IsValid(HeightFog))
	{
		UExponentialHeightFogComponent* Fog = HeightFog->GetComponent();
		Fog->SetFogDensity(FMath::Lerp(NightFogDensity, DayFogDensity, Phase.SkyBlend));
		Fog->SetFogInscatteringColor(FMath::Lerp(NightFogColor, DayFogColor, Phase.SkyBlend));
	}
	for (ALight* Actor : InteriorLights)
	{
		if (!IsValid(Actor)) continue;
		Actor->GetLightComponent()->SetIntensity(FMath::Lerp(InteriorNightIntensity, InteriorDayIntensity, Phase.SkyBlend));
		Actor->GetLightComponent()->SetLightColor(InteriorTint);
	}
	for (ALight* Actor : ClandestineLights)
	{
		if (!IsValid(Actor)) continue;
		const auto* Noct=GetDefault<UBotanicusNoctifloreSettings>();
        const float Variation=1.f+FMath::Clamp(Noct->LightVariation,0.f,.05f)*FMath::Sin(GetWorld()->GetTimeSeconds()*.45f);
        const float Weight=Noct->bEnabled ? ClandestineWeight : (bClandestine ? 1.f : 0.f);
        Actor->GetLightComponent()->SetIntensity(ClandestineIntensity * (1.f-Phase.SkyBlend) * Weight *
            (Noct->bEnabled ? FMath::Clamp(Noct->LightIntensityMultiplier,.5f,1.5f)*Variation : 1.f));
        Actor->GetLightComponent()->SetLightColor(Noct->bEnabled ? Noct->LightTint : ClandestineTint);
	}
	const double Now = FPlatformTime::Seconds();
	if (IsValid(LegacySkySphere) && SkyUpdateFunction.IsValid() &&
		(bForceSky || Now - LastSkyUpdateRealSeconds >= FMath::Max(0.05f, SkyMaterialUpdateInterval)))
	{
		SetSkyNumber(LegacySkySphere, TEXT("Sun brightness"), SkySunBrightness * Phase.Daylight);
		SetSkyNumber(LegacySkySphere, TEXT("Stars brightness"), StarsBrightness);
		FEditorScriptExecutionGuard ScriptGuard;
		LegacySkySphere->ProcessEvent(SkyUpdateFunction.Get(), nullptr);
		LastSkyUpdateRealSeconds = Now;
	}
}
