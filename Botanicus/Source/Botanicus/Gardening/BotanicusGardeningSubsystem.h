#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BotanicusGardeningSubsystem.generated.h"
class ABotanicusPlantPotActor;
class ABotanicusGardeningRobot;
/** Server plant registry and per-slot reservations, independent of local plant widgets. */
UCLASS()
class BOTANICUS_API UBotanicusGardeningSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Register(ABotanicusPlantPotActor* Pot);
    const TArray<TWeakObjectPtr<ABotanicusPlantPotActor>>& GetPots() const {return Pots;}
    bool IsAvailable(ABotanicusPlantPotActor* Pot,int32 Slot,ABotanicusGardeningRobot* Robot) const;
    bool Reserve(ABotanicusPlantPotActor* Pot,int32 Slot,ABotanicusGardeningRobot* Robot);
    void Release(ABotanicusGardeningRobot* Robot);
private:
    struct FReservation {TWeakObjectPtr<ABotanicusPlantPotActor> Pot; int32 Slot=0; TWeakObjectPtr<ABotanicusGardeningRobot> Robot;};
    TArray<TWeakObjectPtr<ABotanicusPlantPotActor>> Pots;
    TArray<FReservation> Reservations;
};
