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

    /** Offset added in the shared UI root: +X towards the player, +Y brings the right column inward, +Z up. */
    UPROPERTY(EditAnywhere,Config,Category="Presentation|Icones laterales",meta=(Units="cm"))
    FVector SideIconsLocalOffset=FVector(0,8,0);
    UPROPERTY(EditAnywhere,Config,Category="Presentation|Icones laterales",meta=(ClampMin="0.05",ClampMax="0.30"))
    float SideIconsScale=.16f;
    UPROPERTY(EditAnywhere,Config,Category="Presentation|Panneau",meta=(ClampMin="-30",ClampMax="60",Units="cm"))
    float InfoPanelVerticalOffset=8.f;
    /** Positive local pitch tilts the panel normal upward; its top leans back towards the pot. */
    UPROPERTY(EditAnywhere,Config,Category="Presentation|Panneau",meta=(ClampMin="-30",ClampMax="30",Units="deg"))
    float InfoPanelPitchOffset=18.f;
};
