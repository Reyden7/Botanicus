#pragma once
#include "CoreMinimal.h"
#include "BotanicusCheckoutVisualTypes.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ABotanicusCheckoutScanner;
USTRUCT()
struct FBotanicusCheckoutBeam
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Beam;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Impact;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    TWeakObjectPtr<ABotanicusCheckoutScanner> Scanner;
    FVector Start,End;
    double StartedAt=0;
    float Duration=.2f;
};
