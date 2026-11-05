#include "Visuals/BotanicusJuiceRelay.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Engine/World.h"
ABotanicusJuiceRelay::ABotanicusJuiceRelay()
{
	bReplicates=true;bAlwaysRelevant=true;PrimaryActorTick.bCanEverTick=false;
}
void ABotanicusJuiceRelay::PlantSucceeded(AActor* Pot,const FVector& Position)
{
	if(HasAuthority()) MulticastPlant(++NextSequence,Pot,Position);
}
void ABotanicusJuiceRelay::MulticastPlant_Implementation(uint32 Sequence,AActor* Pot,FVector_NetQuantize Position)
{
	if(GetNetMode()!=NM_DedicatedServer)
		if(auto* Juice=GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>()) Juice->ReceivePlant(Sequence,Pot,Position);
}
