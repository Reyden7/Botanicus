#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BotanicusJuiceWorldSubsystem.generated.h"
class UNiagaraComponent;
class UNiagaraSystem;
class ABotanicusJuiceRelay;
class ABotanicusAudioRelay;
class ABotanicusWateringCanActor;
class UMaterialInstanceDynamic;

USTRUCT()
struct FBotanicusWaterJuice
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UNiagaraComponent> Drops;
	UPROPERTY() TObjectPtr<ABotanicusWateringCanActor> RemoteCan;
	TWeakObjectPtr<AActor> LastHit;
	bool bDesired=false;
	double StopAt=0;
	double NextSplash=0;
};
USTRUCT()
struct FBotanicusJuiceBurst
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UNiagaraComponent> Component;
	TWeakObjectPtr<AActor> Surface;
	TWeakObjectPtr<AActor> Source;
	double ExpireAt=0;
};
struct FBotanicusWetnessJuice
{
	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<UMaterialInstanceDynamic> Material;
	float Current=0;
	float Target=0;
};

/** Local cosmetic rendering; cultivation and audio remain independent. */
UCLASS()
class BOTANICUS_API UBotanicusJuiceWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(UBotanicusJuiceWorldSubsystem,STATGROUP_Tickables);}
	virtual bool IsTickable() const override;
	static void PlantSucceeded(AActor* Pot,const FVector& Position);
	static void SetSoilWetness(UObject* Owner,UMaterialInstanceDynamic* Material,float Wetness,float Exponent=.5f);
	static FVector SoilSurface(AActor* Pot,const FVector& Hint);
	void ReceivePlant(uint32 Sequence,AActor* Pot,const FVector& Position);
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetPlantBurstCount() const {return PlantBursts;}
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetWaterStartCount() const {return WaterStarts;}
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetWaterComponentCount() const {return Water.Num();}
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetActiveWaterCount() const;
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetActiveBurstCount() const {return Bursts.Num();}
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetParticleCount() const;
	UFUNCTION(BlueprintPure,Category="Botanicus|Juice|Diagnostics") int32 GetWetnessTransitionCount() const {return WetnessTransitions.Num();}
private:
	void ReconcileWater();
	void UpdateWater(float DeltaTime);
	void SpawnBurst(AActor* Surface,const FVector& Position,const FLinearColor& Tint,int32 Count,float Size,float Lifetime,float Speed,AActor* Source=nullptr);
	UNiagaraComponent* CreateEffect(UNiagaraSystem* System);
	ABotanicusJuiceRelay* GetRelay();
	FVector ListenerPosition() const;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> WaterSystem;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> BurstSystem;
	UPROPERTY(Transient) TMap<TWeakObjectPtr<AActor>,FBotanicusWaterJuice> Water;
	UPROPERTY(Transient) TArray<FBotanicusJuiceBurst> Bursts;
	TMap<TWeakObjectPtr<UMaterialInstanceDynamic>,FBotanicusWetnessJuice> WetnessTransitions;
	TWeakObjectPtr<ABotanicusJuiceRelay> Relay;
	TWeakObjectPtr<ABotanicusAudioRelay> AudioRelay;
	FTimerHandle ReconcileTimer;
	uint32 LastPlantSequence=0;
	int32 PlantBursts=0;
	int32 WaterStarts=0;
};
