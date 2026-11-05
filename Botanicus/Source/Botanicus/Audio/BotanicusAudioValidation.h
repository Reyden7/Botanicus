#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusAudioValidation.generated.h"
class UBotanicusAudioWorldSubsystem;
/** Isolated editor test harness. Mutations require PIE, an opt-in flag and an isolated save directory. */
UCLASS()
class UBotanicusAudioValidation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Audio|Diagnostics")
	static UBotanicusAudioWorldSubsystem* GetAudio(UObject* Context);
	UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Audio|Diagnostics")
	static bool Step(UObject* Context,FName Action);
};
