#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusPlantContextSettings.generated.h"

/** Local presentation only; none of these settings change interaction reach or plant needs. */
UCLASS(Config=Game,DefaultConfig,meta=(DisplayName="Informations contextuelles des plantes"))
class BOTANICUS_API UBotanicusPlantContextSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Botanicus"); }
    UPROPERTY(EditAnywhere,Config,Category="Panneau",meta=(ClampMin="25",ClampMax="500",Units="cm"))
    float PanelDistance=230.f;
    UPROPERTY(EditAnywhere,Config,Category="Panneau",meta=(ClampMin="0",ClampMax="1",Units="s"))
    float FadeInSeconds=.15f;
    UPROPERTY(EditAnywhere,Config,Category="Panneau",meta=(ClampMin="0",ClampMax="1",Units="s"))
    float FadeOutSeconds=.15f;
    UPROPERTY(EditAnywhere,Config,Category="Panneau",meta=(ClampMin="0",ClampMax=".5",Units="s"))
    float TargetHoldSeconds=.08f;
    UPROPERTY(EditAnywhere,Config,Category="Alertes")
    bool bAlertsEnabled=true;
    UPROPERTY(EditAnywhere,Config,Category="Alertes",meta=(ClampMin="25",ClampMax="2000",Units="cm"))
    float AlertDistance=800.f;
};
