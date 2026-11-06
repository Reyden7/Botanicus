#include "Visuals/BotanicusPlacementJuiceSettings.h"
UBotanicusPlacementJuiceSettings::UBotanicusPlacementJuiceSettings()
{
    HologramMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Botanicus/Materials/Juice/Placement/M_PlacementHologram.M_PlacementHologram")));
}
