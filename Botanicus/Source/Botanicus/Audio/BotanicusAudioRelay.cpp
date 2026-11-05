#include "Audio/BotanicusAudioRelay.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
ABotanicusAudioRelay::ABotanicusAudioRelay()
{
	bReplicates=true; bAlwaysRelevant=true; PrimaryActorTick.bCanEverTick=false;
	SetNetUpdateFrequency(10.0f);
}
void ABotanicusAudioRelay::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) GetWorldTimerManager().SetTimer(ExpiryTimer,this,&ABotanicusAudioRelay::ExpireWater,0.1f,true);
}
void ABotanicusAudioRelay::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ExpiryTimer);
	Super::EndPlay(Reason);
}
void ABotanicusAudioRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ABotanicusAudioRelay,WaterStates);
}
void ABotanicusAudioRelay::Emit(FName EventId,const FVector& Location)
{
	if (HasAuthority()) MulticastFeedback(++NextSequence,EventId,Location);
}
void ABotanicusAudioRelay::MulticastFeedback_Implementation(uint32 Sequence,FName EventId,FVector_NetQuantize Location)
{
	if (GetNetMode()!=NM_DedicatedServer)
		if (auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>()) Audio->ReceiveFeedback(Sequence,EventId,Location);
}
void ABotanicusAudioRelay::PulseWater(AActor* Source)
{
	if (!HasAuthority() || !IsValid(Source)) return;
	LastWaterPulse.FindOrAdd(Source)=GetWorld()->GetTimeSeconds();
	if (!WaterStates.ContainsByPredicate([Source](const auto& State){return State.Source==Source;}))
	{
		WaterStates.AddDefaulted_GetRef().Source=Source;
		ForceNetUpdate(); OnRep_WaterStates();
	}
}
void ABotanicusAudioRelay::EndWater(AActor* Source)
{
	if (!HasAuthority()) return;
	LastWaterPulse.Remove(Source);
	if (WaterStates.RemoveAll([Source](const auto& State){return State.Source==Source;})>0)
	{
		ForceNetUpdate(); OnRep_WaterStates();
	}
}
void ABotanicusAudioRelay::ExpireWater()
{
	for(auto It=LastWaterPulse.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
	const double Now=GetWorld()->GetTimeSeconds();
	const int32 Removed=WaterStates.RemoveAll([this,Now](const auto& State)
	{
		const double* Last=LastWaterPulse.Find(State.Source.Get());
		if (!IsValid(State.Source) || !Last || Now-*Last>0.35)
		{
			LastWaterPulse.Remove(State.Source.Get()); return true;
		}
		return false;
	});
	if (Removed) { ForceNetUpdate(); OnRep_WaterStates(); }
}
void ABotanicusAudioRelay::OnRep_WaterStates()
{
	if (auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>()) Audio->RefreshWaterLoops(this);
}
