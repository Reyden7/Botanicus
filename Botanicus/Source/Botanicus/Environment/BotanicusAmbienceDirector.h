#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusAmbienceDirector.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ALight;
class UCurveFloat;
class ABotanicusClandestineDoor;

/** One placed, non-replicated presentation controller per level, on every peer. */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusAmbienceDirector : public AActor
{
	GENERATED_BODY()
public:
	ABotanicusAmbienceDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Bindings")
	TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Bindings")
	TObjectPtr<ASkyLight> SkyLight;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Bindings")
	TObjectPtr<AActor> LegacySkySphere;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Bindings")
	TObjectPtr<AExponentialHeightFog> HeightFog;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Bindings")
	TObjectPtr<APostProcessVolume> ExposureVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Time", meta=(ClampMin="0.0", ClampMax="1.0"))
	float VisualResponseSeconds = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Time", meta=(ClampMin="1.0", ClampMax="120.0"))
	float SnapThresholdMinutes = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sun")
	float SolarAzimuth = 38.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sun", meta=(ClampMin="5.0", ClampMax="85.0"))
	float MaximumSunElevation = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sun", meta=(ClampMin="0.0"))
	float DaySunIntensity = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sun")
	FLinearColor NoonSunColor = FLinearColor(1.0f, 0.95f, 0.82f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sun")
	FLinearColor HorizonSunColor = FLinearColor(1.0f, 0.48f, 0.22f);
	/** Optional normalized daylight -> intensity multiplier, does not change clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sun")
	TObjectPtr<UCurveFloat> SunIntensityCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sky", meta=(ClampMin="0.0"))
	float DaySkyIntensity = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sky", meta=(ClampMin="0.0"))
	float NightSkyIntensity = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sky")
	FLinearColor NightSkyTint = FLinearColor(0.32f, 0.46f, 0.78f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sky", meta=(ClampMin="0.0"))
	float SkySunBrightness = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sky", meta=(ClampMin="0.0"))
	float StarsBrightness = 0.25f;
	/** Blueprint sky material updates, real seconds, never scene recaptures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Sky", meta=(ClampMin="0.05"))
	float SkyMaterialUpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Fog", meta=(ClampMin="0.0", ClampMax="0.1"))
	float DayFogDensity = 0.008f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Fog", meta=(ClampMin="0.0", ClampMax="0.1"))
	float NightFogDensity = 0.012f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Fog")
	FLinearColor DayFogColor = FLinearColor(0.45f, 0.63f, 0.85f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Fog")
	FLinearColor NightFogColor = FLinearColor(0.008f, 0.014f, 0.028f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure")
	bool bManageExposure = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure", meta=(ClampMin="-10.0", ClampMax="20.0"))
	float MinimumEV100 = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure", meta=(ClampMin="-10.0", ClampMax="20.0"))
	float MaximumEV100 = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure")
	float ExposureCompensation = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure", meta=(ClampMin="0.1"))
	float ExposureSpeedUp = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure", meta=(ClampMin="0.1"))
	float ExposureSpeedDown = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LocalHighlightContrast = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Exposure", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LocalShadowContrast = 0.8f;

	/** Explicit opt-in decorative lights only. Never bind climate gameplay lights. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Interior")
	TArray<TObjectPtr<ALight>> InteriorLights;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Interior", meta=(ClampMin="0.0"))
	float InteriorDayIntensity = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Interior", meta=(ClampMin="0.0"))
	float InteriorNightIntensity = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Interior")
	FLinearColor InteriorTint = FLinearColor(1.0f, 0.74f, 0.46f);
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambience|Clandestine")
	TArray<TObjectPtr<ALight>> ClandestineLights;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Clandestine", meta=(ClampMin="0.0"))
	float ClandestineIntensity = 350.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience|Clandestine")
	FLinearColor ClandestineTint = FLinearColor(0.35f, 0.58f, 1.0f);

    /** Optional sectional door supplies synchronized public/neon presentation phases. */
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite,Category="Ambience|Night reveal") TObjectPtr<ABotanicusClandestineDoor> ClandestineDoor;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ambience|Night reveal",meta=(ClampMin="0")) float StandbyIntensity=200.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ambience|Night reveal",meta=(ClampMin=".1")) float ShopLightTransitionSeconds=3.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ambience|Night reveal") FLinearColor StandbyTint=FLinearColor(1.f,.64f,.32f);
	/** Refresh from replicated GameState; also used by integration automation. */
	void RefreshFromGameState(float DeltaSeconds);
	float GetVisualMinutes() const { return VisualMinutes; }
	/** Editor preview only. Never sets game time or touches the save game. */
	UFUNCTION(BlueprintCallable, Category="Ambience|Preview")
	void PreviewAtHour(float Hour, int32 DayNumber = 1, bool bClandestine = false);
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Ambience|Preview")
	void ApplyExposureSettings();
private:
	void InitializeBindings();
	void ApplyPresentation(float Minutes, int32 DayNumber, bool bClandestine, bool bForceSky);
	float ClandestineWeight = 0.f;
	bool bClandestineSample = false;
	bool bBindingsInitialized = false;
	bool bHasTimeSample = false;
	float VisualMinutes = 0.0f;
	double LastSkyUpdateRealSeconds = -1.0;
	TWeakObjectPtr<UFunction> SkyUpdateFunction;
};
