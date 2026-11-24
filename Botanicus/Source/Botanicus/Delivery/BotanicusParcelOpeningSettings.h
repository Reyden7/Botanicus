#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusParcelOpeningSettings.generated.h"
class UNiagaraSystem;
class USoundBase;
class USoundAttenuation;
class UMaterialInterface;

/** Cosmetics only. Contents, inventory validation and delivery rules stay authoritative. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Parcel Opening"))
class BOTANICUS_API UBotanicusParcelOpeningSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Botanicus"); }
	UPROPERTY(EditAnywhere, Config, Category="Opening", meta=(ClampMin="0.2", ClampMax="1", Units="s"))
	float OpeningDuration = 0.5f;
	UPROPERTY(EditAnywhere, Config, Category="Confetti", meta=(ClampMin="1", ClampMax="50"))
	int32 ConfettiCount = 40;
	UPROPERTY(EditAnywhere, Config, Category="Confetti", meta=(ClampMin="0.8", ClampMax="1.2", Units="s"))
	float ConfettiLifetime = 1.0f;
	UPROPERTY(EditAnywhere, Config, Category="Confetti", meta=(ClampMin="1", ClampMax="12"))
	int32 MaximumSimultaneousBursts = 6;
	UPROPERTY(EditAnywhere, Config, Category="Confetti", meta=(ClampMin="100", ClampMax="5000", Units="cm"))
	float DisplayDistance = 2000.0f;
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UNiagaraSystem> ConfettiSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/Botanicus/VFX/Delivery/NS_ParcelConfetti.NS_ParcelConfetti")));
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> ConfettiMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Botanicus/VFX/Delivery/M_ParcelConfetti.M_ParcelConfetti")));
	/** Optional legacy layer for the confetti burst. */
	UPROPERTY(EditAnywhere, Config, Category="Future Audio")
	TSoftObjectPtr<USoundBase> OptionalPopSound;

	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio")
	TSoftObjectPtr<USoundBase> CuttingTapeSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Audio/Delivery/Cartons/S_Carton_CuttingTape.S_Carton_CuttingTape")));
	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio", meta=(ClampMin="0", ClampMax="2"))
	float CuttingSoundVolume = 1.f;
	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio", meta=(ClampMin="0.1", ClampMax="4"))
	float CuttingSoundMinPlayRate = .5f;
	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio", meta=(ClampMin="0.1", ClampMax="4"))
	float CuttingSoundMaxPlayRate = 2.f;
	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio", meta=(ClampMin="0.1", ClampMax="4"))
	float CuttingSoundReferencePlayRate = 1.f;
	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio", meta=(ClampMin="0.1"))
	float CuttingSoundSpeedSmoothing = 10.f;
	UPROPERTY(EditAnywhere, Config, Category="Cutting Audio", meta=(ClampMin="0", Units="s"))
	float CuttingSoundFadeDuration = .08f;

	UPROPERTY(EditAnywhere, Config, Category="Opening Audio")
	TSoftObjectPtr<USoundBase> BoxOpenedSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Audio/Delivery/Cartons/S_Carton_Opened.S_Carton_Opened")));
	UPROPERTY(EditAnywhere, Config, Category="Opening Audio", meta=(ClampMin="0", ClampMax="2"))
	float BoxOpenedSoundVolume = .8f;
	UPROPERTY(EditAnywhere, Config, Category="Opening Audio")
	TSoftObjectPtr<USoundAttenuation> CartonAudioAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Audio/Delivery/SA_DeliveryImpact.SA_DeliveryImpact")));
};
