#include "Gardening/BotanicusRobotAudioComponent.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

namespace
{
int32 ReactionPriority(EBotanicusRobotReaction Reaction)
{
    switch(Reaction)
    {
    case EBotanicusRobotReaction::Ouch:return 100;
    case EBotanicusRobotReaction::NoWater:return 80;
    case EBotanicusRobotReaction::LowBattery:return 75;
    case EBotanicusRobotReaction::Blocked:return 70;
    case EBotanicusRobotReaction::Success:case EBotanicusRobotReaction::Charged:return 40;
    case EBotanicusRobotReaction::Start:case EBotanicusRobotReaction::Watering:return 20;
    case EBotanicusRobotReaction::PlantFound:return 10;
    default:return 5;
    }
}
}
void UBotanicusRobotAudioComponent::InitializeVoice()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    Expression=MakeVoice(TEXT("RobotExpressionAudio"));
    for(const auto& Pair:RobotVoices)for(const auto& Ref:Pair.Value.Sounds)if(auto* Sound=Ref.LoadSynchronous())LoadedVoices.AddUnique(Sound);
    // First observation is a baseline, including late joins and restored robots.
    UpdateVoice();
}
bool UBotanicusRobotAudioComponent::PlayRobotReaction(EBotanicusRobotReaction Reaction,float Age)
{
    if(!Expression || !GetWorld() || RobotVoiceVolume<=0)return false;
    const auto* Set=RobotVoices.Find(Reaction);if(!Set || Set->Sounds.IsEmpty())return false;
    const double Now=GetWorld()->GetTimeSeconds();const int32 Priority=ReactionPriority(Reaction);
    const bool bMayInterrupt=Expression->IsPlaying() && Priority>CurrentVoicePriority;
    if(Priority<60 && Now<NextOrdinaryVoice && !bMayInterrupt)return false;
    if(Expression->IsPlaying() && Priority<=CurrentVoicePriority)return false;
    int32 Index=FMath::RandHelper(Set->Sounds.Num());const int32* Previous=LastVoiceIndex.Find(Reaction);
    if(Previous && Set->Sounds.Num()>1 && Index==*Previous)Index=(Index+1+FMath::RandHelper(Set->Sounds.Num()-1))%Set->Sounds.Num();
    auto* Sound=Set->Sounds[Index].LoadSynchronous();if(!Sound || Age>=Sound->GetDuration())return false;
    LoadedVoices.AddUnique(Sound);Expression->Stop();Expression->SetSound(Sound);
    Expression->FadeIn(.005f,FMath::Clamp(RobotVoiceVolume*Set->VolumeMultiplier,0.f,4.f),FMath::Max(0.f,Age));
    CurrentVoicePriority=Priority;LastVoiceIndex.Add(Reaction,Index);
    NextOrdinaryVoice=Now+FMath::Max(0.f,OrdinaryVoiceCooldown);return true;
}
void UBotanicusRobotAudioComponent::UpdateVoice()
{
    auto* R=Cast<ABotanicusGardeningRobot>(GetOwner());if(!R || !Expression)return;
    const auto& P=R->GetPresentation();const auto S=R->State;
    const bool bNoWater=R->CurrentWater<=KINDA_SMALL_NUMBER || (S==EBotanicusGardeningState::RefillingWater && R->CurrentWater<R->Config.WaterCapacity-KINDA_SMALL_NUMBER);
    const bool bLow=R->Battery<=R->Config.BatteryReturnThreshold;
    const bool bCharging=S==EBotanicusGardeningState::Charging && R->IsDocked() && R->Battery<1-KINDA_SMALL_NUMBER;
    const bool bFull=R->Battery>=1-KINDA_SMALL_NUMBER;
    const bool bPlant=S==EBotanicusGardeningState::MovingToPlant && IsValid(R->GetWateringTarget());
    const bool bNewPlant=bPlant && (VoiceTarget.Get()!=R->GetWateringTarget() || VoiceTargetSlot!=R->GetWateringTargetSlot());
    if(bVoiceInitialized)
    {
        // Important reactions are evaluated first; one component prevents overlapping voices.
        if(bNoWater && !bVoiceNoWater)PlayRobotReaction(EBotanicusRobotReaction::NoWater);
        if(bLow && !bVoiceLowBattery)PlayRobotReaction(EBotanicusRobotReaction::LowBattery);
        if(S==EBotanicusGardeningState::Blocked && VoiceState!=S)PlayRobotReaction(EBotanicusRobotReaction::Blocked);
        const double CompletedAge=R->PresentationNow()-P.WateringCompletedAt;
        if(P.WateringCompletedAt>=0 && P.WateringCompletedAt>VoiceCompleted && CompletedAge<.75)
            PlayRobotReaction(EBotanicusRobotReaction::Success,FMath::Max(0.f,float(CompletedAge)));
        if(bFull && !bVoiceFull && (bVoiceCharging || R->IsDocked()))PlayRobotReaction(EBotanicusRobotReaction::Charged);
        const float Age=FMath::Max(0.f,float(R->PresentationNow()-P.StartedAt));
        if(P.Activity==EBotanicusRobotActivity::LeavingDock && VoiceActivity!=P.Activity && Age<.75)
            PlayRobotReaction(EBotanicusRobotReaction::Start,Age);
        if(bNewPlant)PlayRobotReaction(EBotanicusRobotReaction::PlantFound);
        if(bCharging && !bVoiceCharging)PlayRobotReaction(EBotanicusRobotReaction::Charging);
    }
    else bVoiceWatering=S==EBotanicusGardeningState::Watering && P.Activity==EBotanicusRobotActivity::Watering;
    bVoiceInitialized=true;bVoiceNoWater=bNoWater;bVoiceLowBattery=bLow;bVoiceCharging=bCharging;bVoiceFull=bFull;
    VoiceState=S;VoiceActivity=P.Activity;VoiceCompleted=P.WateringCompletedAt;
    VoiceTarget=bPlant?R->GetWateringTarget():nullptr;VoiceTargetSlot=bPlant?R->GetWateringTargetSlot():INDEX_NONE;
}
void UBotanicusRobotAudioComponent::ObserveVoiceWater(bool bActive)
{
    if(bVoiceInitialized && bActive && !bVoiceWatering)
    {
        const auto* R=Cast<ABotanicusGardeningRobot>(GetOwner());
        const float Age=R?FMath::Max(0.f,float(R->PresentationNow()-R->GetPresentation().StartedAt)):0;
        if(Age<.75f)PlayRobotReaction(EBotanicusRobotReaction::Watering,Age);
    }
    bVoiceWatering=bActive;
}
