#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusJuiceValidation.generated.h"
class UBotanicusJuiceWorldSubsystem;
UCLASS()
class UBotanicusJuiceValidation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Juice|Diagnostics")
	static UBotanicusJuiceWorldSubsystem* GetJuice(UObject* Context);
	UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Juice|Diagnostics")
	static float ReadWetness(UObject* Context);
	UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Juice|Diagnostics")
	static TArray<float> ReadFrameMetrics(UObject* Context);
	UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Juice|Diagnostics")
	static bool Step(UObject* Context,FName Action);
	UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Juice|Diagnostics")
	static void Capture(UObject* Context,const FString& Filename);
	UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Juice|Diagnostics")
	static bool CaptureFrame(UObject* Context,const FString& Filename);
	UFUNCTION(BlueprintCallable,meta=(WorldContext="Context",DevelopmentOnly),Category="Botanicus|Juice|Diagnostics")
	static bool RestoreGrowth(UObject* Context,float Progress);
	UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Juice|Diagnostics")
	static AActor* GetTarget(UObject* Context);
	UFUNCTION(BlueprintPure,meta=(WorldContext="Context"),Category="Botanicus|Juice|Diagnostics")
	static TArray<float> ReadGrowth(UObject* Context);

};
