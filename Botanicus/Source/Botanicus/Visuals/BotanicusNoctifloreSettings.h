#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusNoctifloreSettings.generated.h"
class UBotanicusPlantMotionCatalog;
class UNiagaraSystem;
/** Local presentation only; never read by cultivation, economy or climate. */
UCLASS(Config=Game,DefaultConfig,meta=(DisplayName="Botanicus Noctiflores"))
class BOTANICUS_API UBotanicusNoctifloreSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UBotanicusNoctifloreSettings();
    UPROPERTY(EditAnywhere,Config,Category="Quality") bool bEnabled=true;
    UPROPERTY(EditAnywhere,Config,Category="Quality") bool bDisableOnLowEffectsQuality=true;
    UPROPERTY(EditAnywhere,Config,Category="Bioluminescence") bool bBioluminescence=true;
    UPROPERTY(EditAnywhere,Config,Category="Bioluminescence",meta=(ClampMin="0",ClampMax="3")) float GlowIntensity=.65f;
    UPROPERTY(EditAnywhere,Config,Category="Bioluminescence") FLinearColor GlowTint=FLinearColor(.42f,.28f,.65f);
    UPROPERTY(EditAnywhere,Config,Category="Bioluminescence",meta=(ClampMin="0",ClampMax="2")) float PulseSpeed=.65f;
    UPROPERTY(EditAnywhere,Config,Category="Distance",meta=(ClampMin="100",ClampMax="5000")) float GlowDisplayDistance=1800.f;
    UPROPERTY(EditAnywhere,Config,Category="Spores") bool bSpores=true;
    UPROPERTY(EditAnywhere,Config,Category="Spores") bool bSporesOnlyDuringTrade=true;
    UPROPERTY(EditAnywhere,Config,Category="Spores",meta=(ClampMin="0",ClampMax="6")) int32 SporeCount=3;
    UPROPERTY(EditAnywhere,Config,Category="Spores",meta=(ClampMin="0.5",ClampMax="15")) float SporeInterval=2.4f;
    UPROPERTY(EditAnywhere,Config,Category="Spores",meta=(ClampMin="0.2",ClampMax="3")) float SporeSize=1.f;
    UPROPERTY(EditAnywhere,Config,Category="Spores",meta=(ClampMin="0",ClampMax="15")) float SporeSpeed=4.f;
    UPROPERTY(EditAnywhere,Config,Category="Spores",meta=(ClampMin="0.5",ClampMax="5")) float SporeLifetime=3.2f;
    UPROPERTY(EditAnywhere,Config,Category="Spores") FLinearColor SporeTint=FLinearColor(.40f,.34f,.68f,.40f);
    UPROPERTY(EditAnywhere,Config,Category="Distance",meta=(ClampMin="100",ClampMax="2500")) float SporeDisplayDistance=1000.f;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="0",ClampMax="4")) int32 MaximumSporeComponents=2;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="0",ClampMax="24")) int32 MaximumSporeParticles=12;
    UPROPERTY(EditAnywhere,Config,Category="Lighting") FLinearColor LightTint=FLinearColor(.36f,.42f,.78f);
    UPROPERTY(EditAnywhere,Config,Category="Lighting",meta=(ClampMin="0.5",ClampMax="1.5")) float LightIntensityMultiplier=1.f;
    UPROPERTY(EditAnywhere,Config,Category="Lighting",meta=(ClampMin="0",ClampMax="5")) float LightTransitionSeconds=1.6f;
    UPROPERTY(EditAnywhere,Config,Category="Lighting",meta=(ClampMin="0",ClampMax="0.05")) float LightVariation=.018f;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UBotanicusPlantMotionCatalog> GlowCatalog;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UNiagaraSystem> SporeSystem;
};
