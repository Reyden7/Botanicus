#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusCheckoutSettings.generated.h"
class ABotanicusCheckoutScanner;
class ABotanicusPaymentCoins;
class UMaterialInterface;
UCLASS(Config=Game,DefaultConfig,meta=(DisplayName="Botanicus Cash Register Juice"))
class BOTANICUS_API UBotanicusCheckoutSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UBotanicusCheckoutSettings();
    UPROPERTY(EditAnywhere,Config,Category="Presentation") bool bAnimationsEnabled=true;
    UPROPERTY(EditAnywhere,Config,Category="Presentation") bool bScanBeamEnabled=true;
    UPROPERTY(EditAnywhere,Config,Category="Presentation") bool bDisableMotionOnLowQuality=true;
    UPROPERTY(EditAnywhere,Config,Category="Presentation",meta=(ClampMin="0.15",ClampMax="0.25")) float ScanSeconds=.20f;
    UPROPERTY(EditAnywhere,Config,Category="Presentation",meta=(ClampMin="0.2",ClampMax="1.5")) float BeamWidth=.6f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0.1",ClampMax="1")) float DrawerOpenSeconds=.45f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0.1",ClampMax="1")) float DrawerCloseSeconds=.35f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0.03",ClampMax="0.2")) float CoinInterval=.08f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0.1",ClampMax="0.5")) float CoinFlight=.26f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0",ClampMax="15")) float CoinArcHeight=8.f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0.1",ClampMax="0.6")) float CoinDepositSeconds=.25f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0",ClampMax="15")) float CoinDepositArcHeight=5.f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0",ClampMax="90")) float CoinRotationDegrees=30.f;
    UPROPERTY(EditAnywhere,Config,Category="Timing",meta=(ClampMin="0",ClampMax="1")) float AfterLastCoinPause=.08f;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="10",ClampMax="256")) int32 MaximumVisibleGoldCoins=96;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="1",ClampMax="5")) float MaximumCollectionSeconds=2.6f;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="100",ClampMax="4000")) float MaximumEffectDistance=1800.f;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftClassPtr<ABotanicusCheckoutScanner> ScannerClass;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftClassPtr<ABotanicusPaymentCoins> PaymentCoinsClass;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UMaterialInterface> BodyMaterial;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UMaterialInterface> BeamMaterial;
};
