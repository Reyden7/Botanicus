#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusCheckoutSettings.generated.h"
class ABotanicusCheckoutScanner;
class ABotanicusPaymentCoins;
class UMaterialInterface;
class UStaticMesh;
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
    UPROPERTY(EditAnywhere,Config,Category="Coins",meta=(ClampMin="0.25",ClampMax="4")) float CoinVisualScale=2.f;
    UPROPERTY(EditAnywhere,Config,Category="Coins",meta=(ClampMin="30",ClampMax="160",Units="cm")) float CoinFloatHeight=90.f;
    UPROPERTY(EditAnywhere,Config,Category="Coins",meta=(ClampMin="0",ClampMax="5",Units="cm")) float CoinFloatAmplitude=2.f;
    UPROPERTY(EditAnywhere,Config,Category="Coins",meta=(ClampMin="0.5",ClampMax="10",Units="s")) float CoinFloatPeriod=2.f;
    UPROPERTY(EditAnywhere,Config,Category="Coins",meta=(ClampMin="0",ClampMax="30")) float CoinFloatRotationSpeed=16.f;
    UPROPERTY(EditAnywhere,Config,Category="Register") TSoftObjectPtr<UStaticMesh> RegisterBodyMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/itemsMesh/caisse/caisse.caisse")));
    UPROPERTY(EditAnywhere,Config,Category="Register") TSoftObjectPtr<UStaticMesh> RegisterDrawerMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/itemsMesh/caisse/tiroir.tiroir")));
    // Common, fixed authoring calibration for the two 2m Blender exports.
    UPROPERTY(EditAnywhere,Config,Category="Register") FVector RegisterVisualOffset=FVector(-8,0,75.755725);
    UPROPERTY(EditAnywhere,Config,Category="Register") FRotator RegisterVisualRotation=FRotator(0,90,0);
    UPROPERTY(EditAnywhere,Config,Category="Register",meta=(ClampMin="0.01",ClampMax="2")) float RegisterVisualScale=.2f;
    UPROPERTY(EditAnywhere,Config,Category="Register") FVector DrawerOpenOffset=FVector(0,140,0);
    UPROPERTY(EditAnywhere,Config,Category="Register") FVector DrawerInteriorTarget=FVector(55,55,-8);
    UPROPERTY(EditAnywhere,Config,Category="Register") FVector ScannerDockOffset=FVector(-5,26.5,77);
    UPROPERTY(EditAnywhere,Config,Category="Register") FRotator ScannerDockRotation=FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="10",ClampMax="256")) int32 MaximumVisibleGoldCoins=96;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="1",ClampMax="5")) float MaximumCollectionSeconds=2.6f;
    UPROPERTY(EditAnywhere,Config,Category="Budget",meta=(ClampMin="100",ClampMax="4000")) float MaximumEffectDistance=1800.f;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftClassPtr<ABotanicusCheckoutScanner> ScannerClass;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftClassPtr<ABotanicusPaymentCoins> PaymentCoinsClass;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UMaterialInterface> BodyMaterial;
    UPROPERTY(EditAnywhere,Config,Category="Assets") TSoftObjectPtr<UMaterialInterface> BeamMaterial;
};
