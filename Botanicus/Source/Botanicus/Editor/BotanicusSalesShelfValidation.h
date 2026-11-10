#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusSalesShelfValidation.generated.h"
UCLASS()
class UBotanicusSalesShelfValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool Step(UObject* Context,FName Action,int32 Index=0);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static FString Snapshot(UObject* Context);
};
