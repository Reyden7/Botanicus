#pragma once
#include "CoreMinimal.h"
#include "Growing/BotanicusPlantCatalog.h"

/** Read-only species-specific request. Units use the existing .12 reservoir/second conversion. */
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
