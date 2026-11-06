#include "Visuals/BotanicusNoctifloreSettings.h"
UBotanicusNoctifloreSettings::UBotanicusNoctifloreSettings()
{
    GlowCatalog=TSoftObjectPtr<UBotanicusPlantMotionCatalog>(FSoftObjectPath(TEXT("/Game/Botanicus/Materials/Juice/Noctiflore/DA_NoctifloreGlow.DA_NoctifloreGlow")));
    SporeSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/VFX/Noctiflore/NS_NoctifloreSpores.NS_NoctifloreSpores")));
}
