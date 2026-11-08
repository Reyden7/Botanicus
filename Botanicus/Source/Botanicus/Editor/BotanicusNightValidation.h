#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusNightValidation.generated.h"
/** Opt-in integration controls, inert outside isolated PIE. */
UCLASS()
class UBotanicusNightValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) static bool Action(UObject* Context,FName Command,float Value=0.f);
    UFUNCTION(BlueprintPure) static FString Snapshot(UObject* Context);
};
