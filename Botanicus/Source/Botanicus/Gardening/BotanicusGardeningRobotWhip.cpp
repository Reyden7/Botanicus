#include "Gardening/BotanicusGardeningRobot.h"
#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Gardening/BotanicusRobotFaceComponent.h"
#include "Gardening/BotanicusRobotAudioComponent.h"
#include "Gardening/BotanicusRobotContactSurface.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusAutomaticWatering.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

bool ABotanicusGardeningRobot::CanReceiveWhip() const
{
    if(Carrier || IsActorBeingDestroyed() || IsDocked() || State==EBotanicusGardeningState::Charging ||
        State==EBotanicusGardeningState::RefillingWater)return false;
    const auto A=Presentation.Activity;
    return A!=EBotanicusRobotActivity::Docked && A!=EBotanicusRobotActivity::StoppingAtDock &&
        A!=EBotanicusRobotActivity::TurningAtDock && A!=EBotanicusRobotActivity::ReversingToDock && A!=EBotanicusRobotActivity::LeavingDock;
}

bool ABotanicusGardeningRobot::ApplyElectricWhip(AActor* InstigatorActor)
{
    if(!HasAuthority() || !IsValid(InstigatorActor) || !CanReceiveWhip() ||
        Presentation.Activity==EBotanicusRobotActivity::WhipBump || GetWorld()->GetTimeSeconds()<NextWhipAllowed)return false;
    // Capture the same validated server view ray; no client-supplied cosmetic transform.
    const auto* Pawn=Cast<APawn>(InstigatorActor);const auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
    if(!PC)return false;
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);FHitResult Contact;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RobotWhipContact),false,InstigatorActor);
    if(!GetWorld()->LineTraceSingleByChannel(Contact,Eye,Eye+View.Vector()*500,ECC_Visibility,Q) || Contact.GetActor()!=this)return false;
    FVector ContactPoint=Contact.ImpactPoint,ContactNormal=Contact.ImpactNormal;
    if(auto* Surface=WhipContactSurface.LoadSynchronous())Surface->Trace(GetMesh(),Body,Contact,ContactPoint,ContactNormal);
    WhipDirection=(GetActorLocation()-InstigatorActor->GetActorLocation()).GetSafeNormal2D();
    if(WhipDirection.IsNearlyZero())return false;
    NextWhipAllowed=GetWorld()->GetTimeSeconds()+FMath::Max(.1f,WhipCooldown);
    WhipResumeState=State;
    if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    const auto A=Presentation.Activity;
    if(State==EBotanicusGardeningState::Watering || A==EBotanicusRobotActivity::StoppingForWater ||
        A==EBotanicusRobotActivity::AimingAtPlant || A==EBotanicusRobotActivity::PreparingWater || A==EBotanicusRobotActivity::FinishingWater)
    {
        FinishWatering(); // Releases the existing reservation and immediately stops water/audio.
        WhipResumeState=EBotanicusGardeningState::SearchingPlant;
    }
    WhipApplied=0;WhipDistance=FMath::Max(0.f,WhipBumpDistance);
    SetActivity(EBotanicusRobotActivity::WhipBump,FMath::Max(.01f,WhipBumpDuration));
    MulticastWhipImpact(ContactPoint+ContactNormal*.3f,ContactNormal);return true;
}

void ABotanicusGardeningRobot::UpdateWhipBump()
{
    if(Carrier){FinishWhipBump();return;}
    const float Alpha=FMath::Clamp(float((PresentationNow()-Presentation.StartedAt)/FMath::Max(.01f,Presentation.Duration)),0.f,1.f);
    const float Wanted=WhipDistance*(1-FMath::Square(1-Alpha));
    FHitResult Hit;
    // The Character capsule performs a real sweep using its collision responses.
    // Stop at the first blocking hit; never slide through a closed door, another robot or a wall.
    GetCharacterMovement()->SafeMoveUpdatedComponent(WhipDirection*(Wanted-WhipApplied),GetActorQuat(),true,Hit);
    WhipApplied=Wanted;
    if(Alpha>=1 || Hit.IsValidBlockingHit())FinishWhipBump();
}

void ABotanicusGardeningRobot::FinishWhipBump()
{
    SetActivity(EBotanicusRobotActivity::None);GetCharacterMovement()->StopMovementImmediately();
    if(!bEnabled || Battery<=0 || !Base || !Base->IsStationAvailable())
    {ReleaseTarget();State=Battery<=0?EBotanicusGardeningState::Blocked:EBotanicusGardeningState::Inactive;return;}
    if(WhipResumeState==EBotanicusGardeningState::ReturningToBase){ReturnToBase();return;}
    FBotanicusAutomaticWateringNeed Need;
    if(WhipResumeState==EBotanicusGardeningState::MovingToPlant && IsValid(Target) &&
        BotanicusAutomaticWatering::Read(Target,TargetSlot,1.f,Need) && Need.Current<Need.Target-KINDA_SMALL_NUMBER)
    {
        FVector Point;float Length;
        if(Approach(Target,TargetSlot,Point,Length) && Navigate(Point,EBotanicusGardeningState::MovingToPlant))return;
        FailMove();return; // Existing unreachable backoff/recovery, not an endless retry loop.
    }
    if(WhipResumeState==EBotanicusGardeningState::Blocked)RetryAfter.Reset();
    ReleaseTarget();MoveFailures=0;State=EBotanicusGardeningState::SearchingPlant;NextSearch=GetWorld()->GetTimeSeconds();
}

void ABotanicusGardeningRobot::MulticastWhipImpact_Implementation(FVector_NetQuantize10 Position,FVector_NetQuantizeNormal Normal)
{
    if(GetNetMode()==NM_DedicatedServer)return;
    if(RobotFace)RobotFace->ShowWhipSurprise(.5f);
    if(RobotAudio)RobotAudio->PlayRobotReaction(EBotanicusRobotReaction::Ouch);
    if(auto* System=WhipSparks.LoadSynchronous())
        if(auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,Position,FRotator::ZeroRotator,FVector::OneVector,false,false,ENCPoolMethod::None))
        {
            FX->SetVariableInt(TEXT("User.CyanCount"),FMath::Clamp(WhipCyanSparks,1,32));
            FX->SetVariableInt(TEXT("User.YellowCount"),FMath::Clamp(WhipYellowSparks,0,12));
            FX->SetVariableFloat(TEXT("User.Lifetime"),FMath::Clamp(WhipSparkDuration,.05f,.4f));
            FX->SetVariableFloat(TEXT("User.Size"),FMath::Clamp(WhipSparkSize,.1f,4.f));
            FX->SetVariableVec3(TEXT("User.VelocityMin"),FVector(Normal)*70-FVector(110));
            FX->SetVariableVec3(TEXT("User.VelocityMax"),FVector(Normal)*70+FVector(110));FX->Activate(true);
            // The shared burst system does not self-complete. Bound its lifetime explicitly.
            TWeakObjectPtr<UNiagaraComponent> WeakFX=FX;FTimerHandle Cleanup;
            GetWorldTimerManager().SetTimer(Cleanup,[WeakFX](){if(auto* Component=WeakFX.Get()){Component->SetAutoDestroy(false);Component->DeactivateImmediate();Component->DestroyComponent();}},FMath::Clamp(WhipSparkDuration,.05f,.4f)+.05f,false);
        }
    if(auto* Sound=WhipImpactSound.LoadSynchronous())
    {
        // Reuse the existing interaction attenuation; one multicast, no local prediction.
        auto* Attenuation=LoadObject<USoundAttenuation>(nullptr,TEXT("/Game/Botanicus/Audio/Settings/ATT_Interactions.ATT_Interactions"));
        UGameplayStatics::PlaySoundAtLocation(this,Sound,Position,WhipVolume,1,0,Attenuation);
    }
}
