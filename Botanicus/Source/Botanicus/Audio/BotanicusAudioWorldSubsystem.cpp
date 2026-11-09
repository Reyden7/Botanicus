#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Audio/BotanicusAudioCatalog.h"
#include "Audio/BotanicusAudioRelay.h"
#include "AudioDevice.h"
#include "BotanicusGameState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Environment/BotanicusSeasonSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "TimerManager.h"

bool UBotanicusAudioWorldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type==EWorldType::Game || Type==EWorldType::PIE;
}
void UBotanicusAudioWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (World.GetNetMode()!=NM_Client) GetRelay();
	if (World.GetNetMode()!=NM_DedicatedServer)
	{
		LoadCatalog();
		World.GetTimerManager().SetTimer(LocalTimer,this,&UBotanicusAudioWorldSubsystem::UpdateLocalAudio,0.1f,true);
	}
}
void UBotanicusAudioWorldSubsystem::LoadCatalog()
{
	if (bCatalogAttempted) return;
	bCatalogAttempted=true;
	Catalog=GetDefault<UBotanicusAudioSettings>()->Catalog.LoadSynchronous();
}
ABotanicusAudioRelay* UBotanicusAudioWorldSubsystem::GetRelay()
{
	if (Relay.IsValid()) return Relay.Get();
	for (TActorIterator<ABotanicusAudioRelay> It(GetWorld());It;++It) {Relay=*It;return *It;}
	if (GetWorld()->GetNetMode()!=NM_Client) Relay=GetWorld()->SpawnActor<ABotanicusAudioRelay>();
	return Relay.Get();
}
void UBotanicusAudioWorldSubsystem::Emit(UObject* Context,FName Id,const FVector& Position)
{
	if (UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull))
		if (World->GetNetMode()!=NM_Client)
			if (auto* Audio=World->GetSubsystem<UBotanicusAudioWorldSubsystem>())
				if (auto* Events=Audio->GetRelay()) Events->Emit(Id,Position);
}
FName UBotanicusAudioWorldSubsystem::GrowthEvent(float Before,float After)
{
	if (Before<1.0f-KINDA_SMALL_NUMBER && After>=1.0f-KINDA_SMALL_NUMBER) return TEXT("Mature");
	if (Before<0.70f && After>=0.70f) return TEXT("GrowthLarge");
	if (Before<0.30f && After>=0.30f) return TEXT("GrowthYoung");
	return NAME_None;
}
void UBotanicusAudioWorldSubsystem::Growth(UObject* Context,float Before,float After,const FVector& Position)
{
	const FName Id=GrowthEvent(Before,After); if(!Id.IsNone()) Emit(Context,Id,Position);
}
FName UBotanicusAudioWorldSubsystem::PlacementEvent(AActor* Actor)
{
	return Actor && (Actor->IsA<ABotanicusPlantPotActor>() || Actor->IsA<ABotanicusSalePotActor>() || Actor->IsA<ABotanicusIllegalPlanterActor>()) ? FName(TEXT("PotPlace")) : FName(TEXT("EquipmentPlace"));
}
void UBotanicusAudioWorldSubsystem::WaterPulse(AActor* Source)
{
	if (IsValid(Source) && Source->HasAuthority())
		if (auto* Audio=Source->GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>())
			if (auto* Events=Audio->GetRelay()) Events->PulseWater(Source);
}
void UBotanicusAudioWorldSubsystem::WaterEnd(AActor* Source)
{
	if (IsValid(Source) && Source->HasAuthority())
		if (auto* Audio=Source->GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>())
			if (auto* Events=Audio->GetRelay()) Events->EndWater(Source);
}
FVector UBotanicusAudioWorldSubsystem::ListenerPosition() const
{
	if (const APlayerController* PC=GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager) return PC->PlayerCameraManager->GetCameraLocation();
		if (PC->GetPawn()) return PC->GetPawn()->GetActorLocation();
	}
	return FVector::ZeroVector;
}
void UBotanicusAudioWorldSubsystem::ReceiveFeedback(uint32 Sequence,FName Id,const FVector& Position)
{
	if (Sequence==0 || static_cast<int32>(Sequence-LastSequence)<=0) return;
	LastSequence=Sequence; PlayFeedback(Id,Position,true);
}
void UBotanicusAudioWorldSubsystem::PlayPlacementContact(FName Id,const FVector& Position)
{
    if(Id==TEXT("PotPlace") || Id==TEXT("EquipmentPlace")) PlayFeedback(Id,Position,true);
}
void UBotanicusAudioWorldSubsystem::PlayPrivate(FName Id) {PlayFeedback(Id,FVector::ZeroVector,false);}
void UBotanicusAudioWorldSubsystem::PlayCheckoutContact(FName Id,const FVector& Position,float PitchMultiplier)
{
    if(Id==TEXT("CoinPickup")) PlayFeedback(Id,Position,true,FMath::Clamp(PitchMultiplier,.9f,1.1f));
}
void UBotanicusAudioWorldSubsystem::PlayFeedback(FName Id,const FVector& Position,bool bSpatial,float PitchMultiplier)
{
	if (GetWorld()->GetNetMode()==NM_DedicatedServer) return;
	LoadCatalog(); if (!Catalog) return;
	const auto* Entry=Catalog->Feedback.Find(Id); if (!Entry || !Entry->Sound) return;
	if (bSpatial && FVector::DistSquared(ListenerPosition(),Position)>FMath::Square(1800.0f)) return;
	const FVector Cell=Position/40.0f;
	const uint32 Key=HashCombine(GetTypeHash(Id),bSpatial ? GetTypeHash(FIntVector(FMath::RoundToInt(Cell.X),FMath::RoundToInt(Cell.Y),FMath::RoundToInt(Cell.Z))) : 0);
	const double Now=FPlatformTime::Seconds();
	if (const double* Last=Cooldowns.Find(Key); Last && Now-*Last<Entry->MinimumInterval) return;
	if (Cooldowns.Num()>256) Cooldowns.Reset();
	Cooldowns.FindOrAdd(Key)=Now;
	UAudioComponent* Voice=bSpatial
		? UGameplayStatics::SpawnSoundAtLocation(GetWorld(),Entry->Sound,Position,FRotator::ZeroRotator,Entry->Volume,Entry->Pitch*PitchMultiplier,0,Id==TEXT("FaucetOpen") && Catalog->RefillAttenuation ? Catalog->RefillAttenuation.Get():Catalog->InteractionAttenuation.Get(),Entry->Concurrency)
		: UGameplayStatics::SpawnSound2D(GetWorld(),Entry->Sound,Entry->Volume,Entry->Pitch*PitchMultiplier,0,Entry->Concurrency);
	if (Voice && Voice->IsPlaying()) ++PlayCounts.FindOrAdd(Id);
}
UAudioComponent* UBotanicusAudioWorldSubsystem::CreateLoop(USoundBase* Sound,bool bSpatial)
{
	if (!Sound) return nullptr;
	UAudioComponent* Voice=NewObject<UAudioComponent>(GetWorld());
	Voice->bAutoActivate=false;Voice->bAutoDestroy=false;Voice->bIsUISound=!bSpatial;
	Voice->SetSound(Sound);Voice->SetVolumeMultiplier(1.0f);
	if (bSpatial) Voice->AttenuationSettings=Catalog->InteractionAttenuation;
	if (USoundConcurrency* Concurrency=bSpatial ? Catalog->WaterConcurrency.Get() : Catalog->AmbienceConcurrency.Get()) Voice->ConcurrencySet.Add(Concurrency);
	Voice->RegisterComponentWithWorld(GetWorld());
	return Voice;
}
void UBotanicusAudioWorldSubsystem::RefreshWaterLoops(ABotanicusAudioRelay* InRelay)
{
	if (!InRelay || GetWorld()->GetNetMode()==NM_DedicatedServer) return;
	LoadCatalog(); if (!Catalog) return;
	for (auto& Pair:WaterVoices) Pair.Value.bDesired=false;
	for (const auto& State:InRelay->GetWaterStates())
	{
		if (!IsValid(State.Source)) continue;
		auto& Voice=WaterVoices.FindOrAdd(State.Source.Get());
		if (!Voice.Component) Voice.Component=CreateLoop(Catalog->WaterLoop,true);
		Voice.bDesired=true;
		if (Voice.Component)
		{
			Voice.Component->SetWorldLocation(State.Source->GetActorLocation()+State.Source->GetActorForwardVector()*40.0f);
			if (!Voice.Component->IsPlaying()) { Voice.Component->FadeIn(0.06f,Catalog->WaterVolume);++WaterStarts; }
			else if (Voice.StopAt!=0.0) Voice.Component->AdjustVolume(0.06f,Catalog->WaterVolume);
			Voice.StopAt=0.0;
		}
	}
	for (auto It=WaterVoices.CreateIterator();It;++It)
	{
		auto& Voice=It.Value();
		if (!It.Key().IsValid())
		{
			if (Voice.Component) {Voice.Component->Stop();Voice.Component->DestroyComponent();} It.RemoveCurrent();continue;
		}
		if (!Voice.bDesired && Voice.Component && Voice.Component->IsPlaying())
		{
			if (Voice.StopAt==0.0) { Voice.Component->AdjustVolume(0.12f,0.0f);Voice.StopAt=FPlatformTime::Seconds()+0.15; }
			else if(FPlatformTime::Seconds()>=Voice.StopAt) {Voice.Component->Stop();Voice.StopAt=0.0;}
		}
	}
}
void UBotanicusAudioWorldSubsystem::UpdateLocalAudio()
{
	if (!Catalog) return;
	const double Now=FPlatformTime::Seconds();
	const float Delta=LastLocalUpdate>0 ? FMath::Clamp(static_cast<float>(Now-LastLocalUpdate),0.0f,0.5f) : 0.1f;
	LastLocalUpdate=Now;
	if (auto* Events=GetRelay()) RefreshWaterLoops(Events);
	const auto* State=GetWorld()->GetGameState<ABotanicusGameState>(); if (!State) return;
	const auto* Seasons=GetDefault<UBotanicusSeasonSettings>();
	const auto& Profile=Seasons->GetProfile(State->GetCurrentSeason());
	const float Minute=State->GetDayTimeMinutes();
	const float Morning=FMath::SmoothStep(Profile.SunriseTimeMinutes-30.0f,Profile.SunriseTimeMinutes+30.0f,Minute);
	const float Evening=1.0f-FMath::SmoothStep(Profile.SunsetTimeMinutes-30.0f,Profile.SunsetTimeMinutes+30.0f,Minute);
	NightWeight=1.0f-Morning*Evening;
	float Interior=0,Clandestine=0;
	for(const auto& Region:Catalog->Regions)
	{
		const float Weight=Region.WeightAt(ListenerPosition());
		if(Region.bClandestine) Clandestine=FMath::Max(Clandestine,Weight);
		Interior=FMath::Max(Interior,Weight);
	}
	if (!State->IsIllegalTradeActive()) Clandestine=0;
	if(!Wind) {Wind=CreateLoop(Catalog->WindLoop,false);if(Wind){Wind->SetVolumeMultiplier(0);Wind->Play();++AmbienceStarts;}}
	if(!Insects) {Insects=CreateLoop(Catalog->NightInsectsLoop,false);if(Insects){Insects->SetVolumeMultiplier(0);Insects->Play();++AmbienceStarts;}}
	if(Wind)
	{
		WindVolume=FMath::FInterpTo(WindVolume,FMath::Lerp(Catalog->OutdoorWindVolume,Catalog->IndoorWindVolume,Interior),Delta,3.0f/FMath::Max(0.05f,Catalog->TransitionSeconds));
		Wind->SetVolumeMultiplier(WindVolume);
		Wind->SetLowPassFilterEnabled(true);Wind->SetLowPassFilterFrequency(FMath::Lerp(18000.0f,2800.0f,Interior));
	}
	if(Insects)
	{
		const float Volume=FMath::Lerp(Catalog->OutdoorNightVolume,Catalog->IndoorNightVolume,Interior)+Clandestine*Catalog->ClandestineNightVolume;
		InsectVolume=FMath::FInterpTo(InsectVolume,NightWeight*Volume,Delta,3.0f/FMath::Max(0.05f,Catalog->TransitionSeconds));
		Insects->SetVolumeMultiplier(InsectVolume);
		Insects->SetLowPassFilterEnabled(true);Insects->SetLowPassFilterFrequency(FMath::Lerp(16000.0f,3500.0f,Interior));
	}
}
int32 UBotanicusAudioWorldSubsystem::GetEventPlayCount(FName Id) const
{
	const int32* Count=PlayCounts.Find(Id); return Count ? *Count : 0;
}
int32 UBotanicusAudioWorldSubsystem::GetActiveWaterVoiceCount() const
{
	int32 Count=0;for(const auto& Pair:WaterVoices) if(Pair.Value.bDesired && Pair.Value.Component && Pair.Value.Component->IsPlaying()) ++Count;return Count;
}
bool UBotanicusAudioWorldSubsystem::HasAudioDevice() const {return GetWorld()->GetAudioDevice().IsValid();}
int32 UBotanicusAudioWorldSubsystem::GetActiveAmbienceVoiceCount() const {return (Wind && Wind->IsPlaying() ? 1 : 0)+(Insects && Insects->IsPlaying() ? 1 : 0);}
void UBotanicusAudioWorldSubsystem::Deinitialize()
{
	GetWorld()->GetTimerManager().ClearTimer(LocalTimer);
	if(Wind){Wind->Stop();Wind->DestroyComponent();} if(Insects){Insects->Stop();Insects->DestroyComponent();}
	for(auto& Pair:WaterVoices) if(Pair.Value.Component){Pair.Value.Component->Stop();Pair.Value.Component->DestroyComponent();}
	WaterVoices.Reset();Super::Deinitialize();
}

UAudioComponent* UBotanicusAudioWorldSubsystem::PlaySpatialTransition(AActor* Owner,USoundBase* Asset,const FVector& Position,float Volume,float StartAt,USoundAttenuation* OverrideAttenuation)
{
    if(!Owner || !Asset || GetWorld()->GetNetMode()==NM_DedicatedServer || FVector::DistSquared(ListenerPosition(),Position)>FMath::Square(1800.f))return nullptr;
    LoadCatalog();
    if(!TransitionConcurrency)
    {
        TransitionConcurrency=NewObject<USoundConcurrency>(this);TransitionConcurrency->Concurrency.MaxCount=2;
        TransitionConcurrency->Concurrency.bLimitToOwner=true;TransitionConcurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopQuietest;
    }
    auto* Voice=NewObject<UAudioComponent>(Owner);Voice->bAutoActivate=false;Voice->bAutoDestroy=true;Voice->bStopWhenOwnerDestroyed=true;
    Voice->SetSound(Asset);Voice->bAllowSpatialization=true;if(OverrideAttenuation)Voice->AttenuationSettings=OverrideAttenuation;else if(Catalog)Voice->AttenuationSettings=Catalog->InteractionAttenuation;
    Voice->ConcurrencySet.Add(TransitionConcurrency);Voice->SetVolumeMultiplier(FMath::Clamp(Volume,0.f,1.f));
    Voice->RegisterComponentWithWorld(GetWorld());Voice->SetWorldLocation(Position);Voice->Play(FMath::Max(0.f,StartAt));
    if(Voice->IsPlaying())++PlayCounts.FindOrAdd(Asset->GetFName());return Voice;
}

UAudioComponent* UBotanicusAudioWorldSubsystem::StartRefillLoop(AActor* Owner,const FVector& Position)
{
    LoadCatalog();return Catalog ? PlaySpatialTransition(Owner,Catalog->RefillLoop,Position,Catalog->RefillVolume,0.f,Catalog->RefillAttenuation) : nullptr;
}
