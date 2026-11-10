#pragma once
#include "CoreMinimal.h"
#include "BotanicusSalesShelfTypes.generated.h"
/** One existing sales lot, serialized with its parent furniture rather than as independent furniture. */
USTRUCT()
struct FBotanicusSavedSalesShelfSlot
{
    GENERATED_BODY()
    UPROPERTY() FName PlantKey;
    UPROPERTY() FName SoilKey;
    UPROPERTY() FName PotKey=TEXT("SalePot");
    UPROPERTY() FGuid SaleLotId;
    UPROPERTY() FGuid PotInstanceId;
};
