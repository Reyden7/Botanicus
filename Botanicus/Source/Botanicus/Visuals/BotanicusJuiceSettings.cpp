#include "Visuals/BotanicusJuiceSettings.h"
UBotanicusJuiceSettings::UBotanicusJuiceSettings()
{
	WaterSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/VFX/Juice/NS_BotanicusWaterDrops.NS_BotanicusWaterDrops")));
	BurstSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Botanicus/VFX/Juice/NS_BotanicusJuiceBurst.NS_BotanicusJuiceBurst")));
	WateringCanClass=TSoftClassPtr<ABotanicusWateringCanActor>(FSoftObjectPath(TEXT("/Game/Botanicus/blueprints/BP_Item_WateringCan.BP_Item_WateringCan_C")));
}
