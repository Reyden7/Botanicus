#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusCashRegisterValidation.generated.h"
UCLASS()
class UBotanicusCashRegisterValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool PrepareVisitorVisit(UObject* Context);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static FString VisitorSnapshot(UObject* Context);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool Prepare(UObject* Context,int32 Price);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool Aim(UObject* Context,FName Target);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool Step(UObject* Context,FName Action);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly)) static bool Capture(UObject* Context,const FString& Path);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static FString Snapshot(UObject* Context);
};
