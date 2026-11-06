#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusPlacementValidation.generated.h"
UCLASS()
class UBotanicusPlacementValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Placement|Diagnostics") static bool Step(UObject* Context,FName Action);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Placement|Diagnostics") static bool Prepare(UObject* Context,FName ItemKey);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Placement|Diagnostics") static bool CaptureRawFrame(UObject* Context,const FString& Filename);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Placement|Diagnostics") static FString Snapshot(UObject* Context);
};
