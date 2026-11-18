#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusJuiceAssetTools.generated.h"

UCLASS()
class UBotanicusJuiceAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Botanicus|Editor|Soil") static FString BuildSoilPour();
    UFUNCTION(BlueprintCallable,Category="Botanicus|Editor|Robot") static FString BuildRobotWhipImpact();
	UFUNCTION(BlueprintCallable, Category="Botanicus|Editor|Juice")
	static FString InspectNiagara(const FString& AssetPath);
	UFUNCTION(BlueprintCallable, Category="Botanicus|Editor|Juice")
	static bool BuildSpriteSystems();
    UFUNCTION(BlueprintCallable,Category="Botanicus|Editor|Juice") static bool BuildDeliverySmoke();
	UFUNCTION(BlueprintCallable, Category="Botanicus|Editor|Juice") static bool BuildNoctifloreSpores(bool bResetPreviewDefaults=false);
};
