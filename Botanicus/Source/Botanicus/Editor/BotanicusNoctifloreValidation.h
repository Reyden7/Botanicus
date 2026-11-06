#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusNoctifloreValidation.generated.h"
/** Mutations restricted to opted-in PIE with isolated copied saves. */
UCLASS()
class UBotanicusNoctifloreValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Noctiflore|Diagnostics") static bool Step(UObject* Context,FName Action);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Noctiflore|Diagnostics") static FString Snapshot(UObject* Context);
};
