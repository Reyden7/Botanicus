#include "Visuals/BotanicusJuiceRelay.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
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

void ABotanicusJuiceRelay::Lifecycle(AActor* Pot,uint8 Event,const FVector& Position)
{
	if(HasAuthority()) MulticastLifecycle(++NextSequence,Pot,Event,Position);
}
void ABotanicusJuiceRelay::MulticastLifecycle_Implementation(uint32 Sequence,AActor* Pot,uint8 Event,FVector_NetQuantize Position)
{
	if(GetNetMode()!=NM_DedicatedServer)
		if(auto* Juice=GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>()) Juice->ReceiveLifecycle(Sequence,Pot,Event,Position);
}

void ABotanicusJuiceRelay::ReceiveLocalPlacement(const FBotanicusPlacementEvent& Event)
{
    if(GetNetMode()!=NM_DedicatedServer)
        if(auto* Juice=GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
            Juice->ReceivePlacement(Event.Sequence,Event.Item,Event.Placer,Event.ItemKey,Event.Final,Event.Start,Event.AudioId,Event.bContentsOnly);
}
void ABotanicusJuiceRelay::Placement(AActor* Item,APawn* Placer,FName ItemKey,const FTransform& Final,const FVector& Start,FName AudioId,bool bContentsOnly)
{
    if(!HasAuthority()) return;
    FBotanicusPlacementEvent Event;Event.Sequence=++NextSequence;Event.Item=Item;Event.Placer=Placer;
    Event.bContentsOnly=bContentsOnly;Event.ItemKey=ItemKey;Event.Final=Final;Event.Start=Start;Event.AudioId=AudioId;
    ReceiveLocalPlacement(Event); // Listen host starts immediately, once, without waiting for the batch.
    if(GetNetMode()==NM_Standalone) return;
    // UE limits repeated unreliable multicast calls per net update (default 2).
    // Aggregate simultaneous successes, rather than raising a global network CVar.
    if(PendingPlacementEvents.Num()<32) PendingPlacementEvents.Add(MoveTemp(Event));
    if(!bPlacementFlushScheduled)
    {
        bPlacementFlushScheduled=true;GetWorldTimerManager().SetTimerForNextTick(this,&ABotanicusJuiceRelay::FlushPlacementBatch);
    }
}
void ABotanicusJuiceRelay::FlushPlacementBatch()
{
    bPlacementFlushScheduled=false;if(PendingPlacementEvents.IsEmpty()) return;
    TArray<FBotanicusPlacementEvent> Events=MoveTemp(PendingPlacementEvents);PendingPlacementEvents.Reset();
    ForceNetUpdate();MulticastPlacementBatch(Events);
}
void ABotanicusJuiceRelay::MulticastPlacementBatch_Implementation(const TArray<FBotanicusPlacementEvent>& Events)
{
    if(HasAuthority()) return; // Authority already received these individual successes.
    for(const auto& Event:Events) ReceiveLocalPlacement(Event);
}
void ABotanicusJuiceRelay::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);PendingPlacementEvents.Reset();bPlacementFlushScheduled=false;Super::EndPlay(Reason);
}
