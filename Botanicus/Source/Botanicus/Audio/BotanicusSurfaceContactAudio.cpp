#include "Audio/BotanicusSurfaceContactAudio.h"
#include "BotanicusCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundConcurrency.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"

UBotanicusSurfaceContactAudio::UBotanicusSurfaceContactAudio()
{
    PrimaryComponentTick.bCanEverTick=false;
    SetIsReplicatedByDefault(false);
}
void UBotanicusSurfaceContactAudio::BeginPlay()
{
    Super::BeginPlay();
    auto* Player=Cast<ABotanicusCharacter>(GetOwner());
    if(!Player || GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    Concurrency=NewObject<USoundConcurrency>(this);
    Concurrency->Concurrency.MaxCount=4;Concurrency->Concurrency.bLimitToOwner=true;
    Concurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopQuietest;
    Player->OnCharacterMovementUpdated.AddDynamic(this,&ThisClass::MovementUpdated);
}
void UBotanicusSurfaceContactAudio::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* Player=Cast<ABotanicusCharacter>(GetOwner()))
    {
        Player->OnCharacterMovementUpdated.RemoveDynamic(this,&ThisClass::MovementUpdated);
    }
    for(const auto& Voice:Voices)if(Voice.IsValid())Voice->Stop();
    Voices.Empty();Super::EndPlay(Reason);
}
void UBotanicusSurfaceContactAudio::AnimationContact(bool bRightFoot)
{
    if(bUseAnimationContacts)Contact(bRightFoot ? TEXT("foot_r") : TEXT("foot_l"));
}
void UBotanicusSurfaceContactAudio::MovementUpdated(float DeltaSeconds,FVector OldLocation,FVector OldVelocity)
{
    if(bUseAnimationContacts)
    {
        // State reset only: notification mode NEVER emits audio from movement.
        if(auto* Player=Cast<ABotanicusCharacter>(GetOwner()))
            if(Player->GetVelocity().Size2D()<15.f || !Player->GetCharacterMovement()->IsMovingOnGround())LastFoot=NAME_None;
        return;
    }
    if(LastMovementFrame==GFrameCounter)return;
    LastMovementFrame=GFrameCounter;
    auto* Player=Cast<ABotanicusCharacter>(GetOwner());if(!Player)return;
    const FVector Position=Player->GetActorLocation();const float Distance=FVector::Dist2D(Position,PreviousPosition);PreviousPosition=Position;
    const float Speed=Player->GetVelocity().Size2D();auto* Movement=Player->GetCharacterMovement();
    if(!bInitialized || !Movement->IsMovingOnGround() || Movement->bJustTeleported || Speed<15.f || DeltaSeconds<=0 || DeltaSeconds>.25f || Distance>FMath::Max(100.f,Speed*DeltaSeconds*2.5f+10.f))
    {bInitialized=true;Travel=0;return;}
    Travel+=Distance;
    const float Step=FMath::Max(20.f,Speed<=600 ? FMath::GetMappedRangeValueClamped(FVector2D(150,600),FVector2D(SlowStepDistance,RunStepDistance),Speed) : FMath::GetMappedRangeValueClamped(FVector2D(600,900),FVector2D(RunStepDistance,SprintStepDistance),Speed));
    if(Travel<Step)return;
    Travel=FMath::Fmod(Travel,Step);bNextRight=!bNextRight;
    Contact(bNextRight ? TEXT("foot_r") : TEXT("foot_l"));
}
void UBotanicusSurfaceContactAudio::Contact(FName Foot)
{
    auto* Player=Cast<ABotanicusCharacter>(GetOwner());UWorld* World=GetWorld();
    if(!Player || !World || World->GetNetMode()==NM_DedicatedServer || LastContactFrame==GFrameCounter || !Player->GetCharacterMovement()->IsMovingOnGround() || Player->GetVelocity().Size2D()<15.f)return;
    // Named contacts from cross-faded copies of the same animation cannot repeat a foot.
    if(bUseAnimationContacts && Foot==LastFoot)return;
    LastContactFrame=GFrameCounter;LastFoot=Foot;++Contacts;
    const bool bLocal=Player->IsLocallyControlled();
    const auto* PC=World->GetFirstPlayerController();
    if(!bLocal && (!PC || !PC->PlayerCameraManager || FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(),Player->GetActorLocation())>FMath::Square(1500.f)))return;
    FVector Point=Player->GetActorLocation()-FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if(Player->GetMesh()->DoesSocketExist(Foot))
    {
        const FVector Socket=Player->GetMesh()->GetSocketLocation(Foot);
        if(FVector::DistSquared2D(Socket,Point)<FMath::Square(80.f)){Point.X=Socket.X;Point.Y=Socket.Y;}
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusSurfaceContact),false,Player);Query.bReturnPhysicalMaterial=true;
    FHitResult Hit;
    if(!World->LineTraceSingleByChannel(Hit,Point+FVector(0,0,40),Point-FVector(0,0,55),ECC_Visibility,Query))
    {
        Query.bTraceComplex=true;
        if(!World->LineTraceSingleByChannel(Hit,Point+FVector(0,0,40),Point-FVector(0,0,55),ECC_Visibility,Query))return;
    }
    if(Hit.ImpactNormal.Z<.4f)return;
    LastPhysicalMaterial=Hit.PhysMaterial.Get();USoundCue* Cue=DefaultSound;
    if(USoundCue** Found=SurfaceSounds.Find(Hit.PhysMaterial.Get()))Cue=*Found;
    if(!Cue)return;LastCue=Cue;
    Voices.RemoveAll([](const auto& Voice){return !Voice.IsValid();});
    auto* Voice=NewObject<UAudioComponent>(Player);Voice->bAutoActivate=false;Voice->bAutoDestroy=true;Voice->bStopWhenOwnerDestroyed=true;
    Voice->SetSound(Cue);Voice->bAllowSpatialization=!bLocal;Voice->ConcurrencySet.Add(Concurrency);
    if(!bLocal)
    {
        FSoundAttenuationSettings Attenuation;Attenuation.bAttenuate=true;Attenuation.bSpatialize=true;Attenuation.AttenuationShapeExtents=FVector(150,0,0);Attenuation.FalloffDistance=1250;
        Voice->AdjustAttenuation(Attenuation);
    }
    Voice->SetVolumeMultiplier(Volume);Voice->SetPitchMultiplier(FMath::FRandRange(.98f,1.02f));
    Voice->RegisterComponentWithWorld(World);Voice->SetWorldLocation(Hit.ImpactPoint);Voice->Play();Voices.Add(Voice);
    if(Voice->IsPlaying())++Plays;
#if WITH_EDITOR
    static const bool bTrace=FParse::Param(FCommandLine::Get(),TEXT("BotanicusContactValidation"));
    if(bTrace)UE_LOG(LogTemp,Display,TEXT("SurfaceContact t=%.9f world=%s local=%d foot=%s mode=%s speed=%.2f pm=%s cue=%s contacts=%d plays=%d"),FPlatformTime::Seconds(),*World->GetPathName(),bLocal ? 1:0,*Foot.ToString(),bUseAnimationContacts ? TEXT("notify"):TEXT("movement"),Player->GetVelocity().Size2D(),*GetNameSafe(Hit.PhysMaterial.Get()),*GetNameSafe(Cue),Contacts,Plays);
#endif
}
