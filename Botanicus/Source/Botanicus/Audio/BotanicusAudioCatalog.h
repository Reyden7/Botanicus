#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusAudioCatalog.generated.h"
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSoundFeedback
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound") TObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta=(ClampMin="0",ClampMax="2")) float Volume = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta=(ClampMin="0.01",ClampMax="2")) float Pitch = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta=(ClampMin="0")) float MinimumInterval = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound") TObjectPtr<USoundConcurrency> Concurrency;
};

/** Soft audio regions only: no overlap, collision or horticultural modifiers. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusAudioRegion
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region") FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region") FVector Centre = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region") FVector Extent = FVector(500,500,250);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region",meta=(ClampMin="1")) float BlendDistance = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region") bool bClandestine = false;
	float WeightAt(const FVector& Position) const;
};

UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusAudioCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback") TMap<FName,FBotanicusSoundFeedback> Feedback;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback") TObjectPtr<USoundAttenuation> InteractionAttenuation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loops") TObjectPtr<USoundBase> WaterLoop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loops") TObjectPtr<USoundConcurrency> WaterConcurrency;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loops",meta=(ClampMin="0",ClampMax="1")) float WaterVolume = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience") TObjectPtr<USoundBase> WindLoop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience") TObjectPtr<USoundBase> NightInsectsLoop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience") TObjectPtr<USoundConcurrency> AmbienceConcurrency;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience",meta=(ClampMin="0",ClampMax="1")) float OutdoorWindVolume = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience",meta=(ClampMin="0",ClampMax="1")) float IndoorWindVolume = 0.018f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience",meta=(ClampMin="0",ClampMax="1")) float OutdoorNightVolume = 0.055f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience",meta=(ClampMin="0",ClampMax="1")) float IndoorNightVolume = 0.012f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience",meta=(ClampMin="0",ClampMax="1")) float ClandestineNightVolume = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience",meta=(ClampMin="0.05")) float TransitionSeconds = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambience") TArray<FBotanicusAudioRegion> Regions;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Audio"))
class BOTANICUS_API UBotanicusAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UBotanicusAudioSettings();
	UPROPERTY(EditAnywhere, Config, Category="Audio") TSoftObjectPtr<UBotanicusAudioCatalog> Catalog;
};
