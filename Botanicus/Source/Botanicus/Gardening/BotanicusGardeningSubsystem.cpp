#include "Gardening/BotanicusGardeningSubsystem.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Growing/BotanicusPlantPotActor.h"
void UBotanicusGardeningSubsystem::Register(AActor* Pot)
{
    if(!Pot || !Pot->HasAuthority() || !GetWorld()->IsGameWorld())return;
    Pots.RemoveAll([](auto& P){return !P.IsValid();}); Pots.AddUnique(Pot);
}
bool UBotanicusGardeningSubsystem::IsAvailable(AActor* P,int32 Slot,ABotanicusGardeningRobot* R) const
{
    return !Reservations.ContainsByPredicate([=](const auto& V){return V.Pot==P && V.Slot==Slot && V.Robot.IsValid() && V.Robot!=R;});
}
bool UBotanicusGardeningSubsystem::Reserve(AActor* P,int32 Slot,ABotanicusGardeningRobot* R)
{
    if(!IsAvailable(P,Slot,R))return false; Release(R); Reservations.Add({P,Slot,R});return true;
}
void UBotanicusGardeningSubsystem::Release(ABotanicusGardeningRobot* R)
{
    Reservations.RemoveAll([R](const auto& V){return !V.Pot.IsValid() || !V.Robot.IsValid() || V.Robot==R;});
}
