#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusPlantContextValidation.generated.h"

/** Small isolated PIE fixture. Never changes the player's save or asset data. */
UCLASS()
class UBotanicusPlantContextValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool Step(UObject* Context,FName Action);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static FString Snapshot(UObject* Context);
};
