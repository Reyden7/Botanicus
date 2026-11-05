#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusJuiceAssetTools.generated.h"

UCLASS()
class UBotanicusJuiceAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Botanicus|Editor|Juice")
	static FString InspectNiagara(const FString& AssetPath);
	UFUNCTION(BlueprintCallable, Category="Botanicus|Editor|Juice")
	static bool BuildSpriteSystems();
};
