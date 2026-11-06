#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusPlacementJuiceSettings.generated.h"
class UMaterialInterface;
class UCurveFloat;
/** Presentation only. Placement rules, inventory and physical transforms never read these values. */
UCLASS(Config=Game,DefaultConfig,meta=(DisplayName="Botanicus Placement Juice"))
class BOTANICUS_API UBotanicusPlacementJuiceSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UBotanicusPlacementJuiceSettings();
    UPROPERTY(EditAnywhere,Config,Category="Quality") bool bPlacementJuiceEnabled=true;
    UPROPERTY(EditAnywhere,Config,Category="Quality") bool bPlacementHologramEnabled=true;
    UPROPERTY(EditAnywhere,Config,Category="Quality") bool bDisableAnimationOnLowEffectsQuality=true;
    UPROPERTY(EditAnywhere,Config,Category="Hologram") FLinearColor ValidColor=FLinearColor(FColor(70,184,255));
    UPROPERTY(EditAnywhere,Config,Category="Hologram") FLinearColor InvalidColor=FLinearColor(FColor(255,79,101));
    UPROPERTY(EditAnywhere,Config,Category="Hologram") FLinearColor ValidRim=FLinearColor(FColor(140,230,255));
    UPROPERTY(EditAnywhere,Config,Category="Hologram") FLinearColor InvalidRim=FLinearColor(FColor(255,154,165));
    UPROPERTY(EditAnywhere,Config,Category="Hologram",meta=(ClampMin="0.1",ClampMax="0.8")) float Opacity=.42f;
    UPROPERTY(EditAnywhere,Config,Category="Hologram",meta=(ClampMin="0",ClampMax="2")) float FresnelIntensity=.65f;
    UPROPERTY(EditAnywhere,Config,Category="Hologram",meta=(ClampMin="0",ClampMax="3")) float EmissiveIntensity=1.1f;
    UPROPERTY(EditAnywhere,Config,Category="Hologram",meta=(ClampMin="0",ClampMax="0.25")) float ColorTransitionSeconds=.10f;
    UPROPERTY(EditAnywhere,Config,Category="Animation") bool bFlyFromHand=true;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="0.03",ClampMax="0.3")) float FlyDuration=.10f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="0.06",ClampMax="0.3")) float GrowDuration=.15f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="0.04",ClampMax="0.3")) float BounceDuration=.12f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="0.1",ClampMax="0.6")) float InitialVisualScale=.30f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="1",ClampMax="1.2")) float OvershootScale=1.08f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="0",ClampMax="8")) float BounceHeight=2.5f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="0",ClampMax="20")) float FlightArcHeight=7.f;
    UPROPERTY(EditAnywhere,Config,Category="Animation",meta=(ClampMin="10",ClampMax="100")) float MaximumHandVisualSpan=55.f;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="0",ClampMax="8")) int32 MaximumSimultaneousAnimations=4;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="1",ClampMax="96")) int32 MaximumMeshesPerAnimation=48;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="1",ClampMax="192")) int32 MaximumVisualComponents=128;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="100",ClampMax="4000")) float MaximumDisplayDistance=1800.f;
    UPROPERTY(EditAnywhere,Config,Category="Policy") TArray<FName> NoFlyItems;
    /** Explicit art override for meshes with misleading below-foot render bounds. */
    UPROPERTY(EditAnywhere,Config,Category="Policy") TArray<FName> GroundPivotItems;
    UPROPERTY(EditAnywhere,Config,Category="Policy") TArray<FName> NoAnimationItems;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UMaterialInterface> HologramMaterial;
    /** Optional normalized 0..1 phase curves; final pose is always exact. */
    UPROPERTY(EditAnywhere,Config,Category="Curves") TSoftObjectPtr<UCurveFloat> FlightCurve;
    UPROPERTY(EditAnywhere,Config,Category="Curves") TSoftObjectPtr<UCurveFloat> GrowthCurve;
    UPROPERTY(EditAnywhere,Config,Category="Curves") TSoftObjectPtr<UCurveFloat> BounceCurve;
};
