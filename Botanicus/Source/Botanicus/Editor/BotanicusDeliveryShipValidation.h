#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusDeliveryShipValidation.generated.h"
UCLASS()
class UBotanicusDeliveryShipValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) static bool Step(UObject* Context,FName Action,int32 Index=0);
    UFUNCTION(BlueprintCallable) static FString Snapshot(UObject* Context);
    UFUNCTION(BlueprintCallable) static FString SavedSnapshot();
};
