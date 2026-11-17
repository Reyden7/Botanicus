#pragma once
#include "CoreMinimal.h"
#include "Growing/BotanicusPlantCatalog.h"

/** Read-only per-slot request. UnitsPerMoisture converts normalized moisture to existing reservoir units. */
struct FBotanicusAutomaticWateringNeed
{
    float Current=0, Minimum=0, Target=0, Maximum=0, UnitsPerMoisture=0;
    FVector SoilPoint=FVector::ZeroVector;
    float UnitsNeeded() const { return FMath::Max(0.f,Target-Current)*UnitsPerMoisture; }
    static bool Read(const FBotanicusPlantDefinition* D,float Water,float Ratio,FBotanicusAutomaticWateringNeed& Out)
    {
        if(!D || D->WaterAddedPerUse<=KINDA_SMALL_NUMBER) return false;
        Out.Current=Water;
        Out.Minimum=FMath::Clamp(D->MinimumHealthyWater,0.f,1.f);
        Out.Maximum=FMath::Clamp(D->MaximumHealthyWater,Out.Minimum,1.f);
        Out.Target=FMath::Lerp(Out.Minimum,Out.Maximum,FMath::Clamp(Ratio,0.f,1.f));
        Out.UnitsPerMoisture=.12f/D->WaterAddedPerUse;
        return true;
    }
};

/** Dispatch only: normal pots retain their existing virtual watering implementation. */
namespace BotanicusAutomaticWatering
{
    int32 SlotCount(const AActor* Target);
    bool IsMature(const AActor* Target,int32 Slot);
    bool CanAccess(const AActor* Target);
    bool Read(const AActor* Target,int32 Slot,float Ratio,FBotanicusAutomaticWateringNeed& Out);
    float Apply(AActor* Target,int32 Slot,float Ratio,float AvailableUnits,bool bFirstPulse);
}
