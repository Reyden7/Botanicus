#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusAudioRelay.generated.h"

USTRUCT()
struct FBotanicusWaterAudioState
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Source;
};

/** Cosmetic events only. Persistent loop state supports loss, join and disconnect. */
UCLASS(NotBlueprintable)
class BOTANICUS_API ABotanicusAudioRelay : public AActor
{
	GENERATED_BODY()
public:
	ABotanicusAudioRelay();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const override;
	void Emit(FName EventId,const FVector& Location);
	void PulseWater(AActor* Source);
	void EndWater(AActor* Source);
	const TArray<FBotanicusWaterAudioState>& GetWaterStates() const { return WaterStates; }
	UFUNCTION(NetMulticast, Unreliable) void MulticastFeedback(uint32 Sequence,FName EventId,FVector_NetQuantize Location);
private:
	UFUNCTION() void OnRep_WaterStates();
	void ExpireWater();
	UPROPERTY(ReplicatedUsing=OnRep_WaterStates) TArray<FBotanicusWaterAudioState> WaterStates;
	TMap<TWeakObjectPtr<AActor>,double> LastWaterPulse;
	FTimerHandle ExpiryTimer;
	uint32 NextSequence = 0;
};
