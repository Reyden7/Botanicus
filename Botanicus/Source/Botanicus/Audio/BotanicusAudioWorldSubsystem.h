#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BotanicusAudioWorldSubsystem.generated.h"
class ABotanicusAudioRelay;
class UBotanicusAudioCatalog;
class UAudioComponent;
class USoundBase;

USTRUCT()
struct FBotanicusWaterVoice
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UAudioComponent> Component;
	bool bDesired = false;
	double StopAt = 0.0;
};

/** Audio presentation only, with one local timer, no climate dependencies. */
UCLASS()
class BOTANICUS_API UBotanicusAudioWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	static void Emit(UObject* Context,FName Id,const FVector& Position);
	static void Growth(UObject* Context,float Before,float After,const FVector& Position);
	static void WaterPulse(AActor* Source);
	static void WaterEnd(AActor* Source);
	static FName PlacementEvent(AActor* Actor);
	static FName GrowthEvent(float Before,float After);
	void ReceiveFeedback(uint32 Sequence,FName Id,const FVector& Position);
	void PlayPrivate(FName Id);
	void RefreshWaterLoops(ABotanicusAudioRelay* InRelay);
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") int32 GetEventPlayCount(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") int32 GetWaterStartCount() const {return WaterStarts;}
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") int32 GetActiveWaterVoiceCount() const;
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") int32 GetAmbienceStartCount() const {return AmbienceStarts;}
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") bool HasAudioDevice() const;
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") float GetNightAmbienceWeight() const {return NightWeight;}
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") float GetAmbienceMixVolume(bool bNight) const {return bNight ? InsectVolume : WindVolume;}
	UFUNCTION(BlueprintPure, Category="Botanicus|Audio|Diagnostics") int32 GetActiveAmbienceVoiceCount() const;
	ABotanicusAudioRelay* GetRelay();
private:
	void UpdateLocalAudio();
	void LoadCatalog();
	void PlayFeedback(FName Id,const FVector& Position,bool bSpatial);
	UAudioComponent* CreateLoop(USoundBase* Sound,bool bSpatial);
	FVector ListenerPosition() const;
	UPROPERTY(Transient) TObjectPtr<UBotanicusAudioCatalog> Catalog;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Wind;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Insects;
	UPROPERTY(Transient) TMap<TWeakObjectPtr<AActor>,FBotanicusWaterVoice> WaterVoices;
	TWeakObjectPtr<ABotanicusAudioRelay> Relay;
	TMap<uint32,double> Cooldowns;
	TMap<FName,int32> PlayCounts;
	FTimerHandle LocalTimer;
	uint32 LastSequence = 0;
	int32 WaterStarts = 0;
	int32 AmbienceStarts = 0;
	float NightWeight = 0.0f;
	float WindVolume = 0.0f;
	float InsectVolume = 0.0f;
	double LastLocalUpdate = 0.0;
	bool bCatalogAttempted = false;
};
