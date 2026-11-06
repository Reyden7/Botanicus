#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusEquipmentValidation.generated.h"
/** Inert outside explicitly opted-in PIE with isolated saves. No shipping instrumentation. */
UCLASS()
class UBotanicusEquipmentValidation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context",DevelopmentOnly)) static bool Begin(UObject* Context);
	UFUNCTION(BlueprintCallable,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context",DevelopmentOnly)) static void Finish(UObject* Context);
	UFUNCTION(BlueprintCallable,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context",DevelopmentOnly)) static bool SetLegacy(UObject* Context,bool bLegacy);
	UFUNCTION(BlueprintCallable,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context",DevelopmentOnly)) static void ResetCounters(UObject* Context);
	UFUNCTION(BlueprintPure,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context")) static TMap<FString,int64> ReadCounters(UObject* Context);
	UFUNCTION(BlueprintPure,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context")) static FString Snapshot(UObject* Context);
	UFUNCTION(BlueprintCallable,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context",DevelopmentOnly)) static bool Step(UObject* Context,FName Action);
	UFUNCTION(BlueprintCallable,Category="Botanicus|Equipment|Diagnostics",meta=(WorldContext="Context",DevelopmentOnly)) static bool Select(UObject* Context,int32 Slot);
#if WITH_EDITOR
	static bool UseLegacy(UWorld* World);
	static bool IsTracing(UWorld* World);
	static void RecordDuration(UWorld* World,double StartSeconds);
	static void Record(UWorld* World,const TCHAR* Event,AActor* Actor=nullptr);
#endif
};

#if WITH_EDITOR
/** CPU self-time only; no timer or per-frame log outside the opted-in harness. */
struct FBotanicusEquipmentUpdateTimer
{
	UWorld* World;
	double Start;
	explicit FBotanicusEquipmentUpdateTimer(UWorld* InWorld)
		: World(UBotanicusEquipmentValidation::IsTracing(InWorld) ? InWorld : nullptr), Start(World ? FPlatformTime::Seconds() : 0) {}
	~FBotanicusEquipmentUpdateTimer() {if(World) UBotanicusEquipmentValidation::RecordDuration(World,Start);}
};
#endif
