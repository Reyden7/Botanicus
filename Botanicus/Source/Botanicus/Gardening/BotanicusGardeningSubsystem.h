#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BotanicusGardeningSubsystem.generated.h"
class AActor;
class ABotanicusGardeningRobot;
/** Server plant registry and per-slot reservations, independent of local plant widgets. */
UCLASS()
class BOTANICUS_API UBotanicusGardeningSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Register(AActor* Pot);
    const TArray<TWeakObjectPtr<AActor>>& GetPots() const {return Pots;}
    bool IsAvailable(AActor* Pot,int32 Slot,ABotanicusGardeningRobot* Robot) const;
    bool Reserve(AActor* Pot,int32 Slot,ABotanicusGardeningRobot* Robot);
    void Release(ABotanicusGardeningRobot* Robot);
private:
    struct FReservation {TWeakObjectPtr<AActor> Pot; int32 Slot=0; TWeakObjectPtr<ABotanicusGardeningRobot> Robot;};
    TArray<TWeakObjectPtr<AActor>> Pots;
    TArray<FReservation> Reservations;
};
