#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusJuiceSettings.generated.h"
class UNiagaraSystem;
class ABotanicusWateringCanActor;

/** Cosmetic controls only. No values in this class influence cultivation. */
UCLASS(Config=Game,DefaultConfig,meta=(DisplayName="Botanicus Planting & Water Juice"))
class BOTANICUS_API UBotanicusJuiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UBotanicusJuiceSettings();
	UPROPERTY(EditAnywhere,Config,Category="Quality") bool bEnabled=true;
	UPROPERTY(EditAnywhere,Config,Category="Quality") bool bWaterDrops=true;
	UPROPERTY(EditAnywhere,Config,Category="Quality") bool bSplashes=true;
	UPROPERTY(EditAnywhere,Config,Category="Quality") bool bPlantingBurst=true;
	UPROPERTY(EditAnywhere,Config,Category="Quality") bool bRemoteHeldCan=true;
	UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UNiagaraSystem> WaterSystem;
	UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UNiagaraSystem> BurstSystem;
	UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftClassPtr<ABotanicusWateringCanActor> WateringCanClass;
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="0",ClampMax="160")) float DropsPerSecond=65;
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="0.2",ClampMax="4")) float DropletSize=1.4f;
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="100",ClampMax="1000")) float DropletSpeed=550;
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="0",ClampMax="980")) float Gravity=580;
	/** Mesh-local rose centre measured on the existing watering-can asset; an authored Water Nozzle overrides it. */
	UPROPERTY(EditAnywhere,Config,Category="Water") FVector NozzleMeshLocalOffset=FVector(-44,0,14);
	UPROPERTY(EditAnywhere,Config,Category="Water") FLinearColor WaterTint=FLinearColor(.16f,.62f,.92f,.75f);
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="0.08",ClampMax="0.5")) float SplashInterval=.16f;
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="0.1",ClampMax="0.6")) float SplashLifetime=.28f;
	UPROPERTY(EditAnywhere,Config,Category="Water",meta=(ClampMin="1",ClampMax="8")) int32 SplashCount=4;
	UPROPERTY(EditAnywhere,Config,Category="Planting",meta=(ClampMin="0",ClampMax="2")) float PlantingIntensity=1;
	UPROPERTY(EditAnywhere,Config,Category="Planting",meta=(ClampMin="1",ClampMax="16")) int32 PlantingCount=8;
	UPROPERTY(EditAnywhere,Config,Category="Planting",meta=(ClampMin="0.1",ClampMax="0.8")) float PlantingLifetime=.4f;
	UPROPERTY(EditAnywhere,Config,Category="Planting") FLinearColor SoilTint=FLinearColor(.24f,.12f,.035f,.9f);
	UPROPERTY(EditAnywhere,Config,Category="Soil",meta=(ClampMin="0",ClampMax="1.5")) float WetnessIntensity=1;
	UPROPERTY(EditAnywhere,Config,Category="Soil",meta=(ClampMin="0.1",ClampMax="20")) float WetnessInterpolationSpeed=7;
	UPROPERTY(EditAnywhere,Config,Category="Distance",meta=(ClampMin="100",ClampMax="5000")) float WaterDisplayDistance=2500;
	UPROPERTY(EditAnywhere,Config,Category="Distance",meta=(ClampMin="100",ClampMax="5000")) float PlantingDisplayDistance=1800;
	UPROPERTY(EditAnywhere,Config,Category="Quality",meta=(ClampMin="1",ClampMax="24")) int32 MaximumBurstComponents=12;
};
