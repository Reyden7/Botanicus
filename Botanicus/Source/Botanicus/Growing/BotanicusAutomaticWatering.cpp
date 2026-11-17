#include "Growing/BotanicusAutomaticWatering.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"

int32 BotanicusAutomaticWatering::SlotCount(const AActor* Target)
{
    if(const auto* Pot=Cast<ABotanicusPlantPotActor>(Target))return Pot->GetAutomaticWateringSlotCount();
    if(const auto* Planter=Cast<ABotanicusIllegalPlanterActor>(Target))return Planter->GetAutomaticWateringSlotCount();
    return 0;
}
bool BotanicusAutomaticWatering::IsMature(const AActor* Target,int32 Slot)
{
    if(const auto* Pot=Cast<ABotanicusPlantPotActor>(Target))return Pot->IsAutomaticWateringMature(Slot);
    if(const auto* Planter=Cast<ABotanicusIllegalPlanterActor>(Target))return Planter->IsAutomaticWateringMature(Slot);
    return true;
}
bool BotanicusAutomaticWatering::CanAccess(const AActor* Target)
{
    if(const auto* Planter=Cast<ABotanicusIllegalPlanterActor>(Target))return Planter->IsAutomaticWateringAllowed();
    return IsValid(Target);
}
bool BotanicusAutomaticWatering::Read(const AActor* Target,int32 Slot,float Ratio,FBotanicusAutomaticWateringNeed& Out)
{
    if(!IsValid(Target))return false;
    if(const auto* Pot=Cast<ABotanicusPlantPotActor>(Target))return Pot->GetAutomaticWateringNeed(Slot,Ratio,Out);
    if(const auto* Planter=Cast<ABotanicusIllegalPlanterActor>(Target))return Planter->GetAutomaticWateringNeed(Slot,Ratio,Out);
    return false;
}
float BotanicusAutomaticWatering::Apply(AActor* Target,int32 Slot,float Ratio,float AvailableUnits,bool bFirstPulse)
{
    if(!IsValid(Target))return 0;
    if(auto* Pot=Cast<ABotanicusPlantPotActor>(Target))return Pot->ApplyAutomaticWater(Slot,Ratio,AvailableUnits,bFirstPulse);
    if(auto* Planter=Cast<ABotanicusIllegalPlanterActor>(Target))return Planter->ApplyAutomaticWater(Slot,Ratio,AvailableUnits,bFirstPulse);
    return 0;
}
