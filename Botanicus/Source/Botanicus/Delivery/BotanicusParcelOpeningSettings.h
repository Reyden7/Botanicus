#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusParcelOpeningSettings.generated.h"
class UNiagaraSystem;
class USoundBase;
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
	/** Empty by default: no new sound in this work package. */
	UPROPERTY(EditAnywhere, Config, Category="Future Audio")
	TSoftObjectPtr<USoundBase> OptionalPopSound;
};
