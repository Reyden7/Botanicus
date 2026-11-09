#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusRefillValidation.generated.h"
/** Opt-in controls, inert outside isolated PIE; never changes production saves. */
UCLASS()
class UBotanicusRefillValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(DevelopmentOnly)) static bool Action(UObject* Context,FName Command,float Value=0);
    UFUNCTION(BlueprintPure) static FString Snapshot(UObject* Context);
};
