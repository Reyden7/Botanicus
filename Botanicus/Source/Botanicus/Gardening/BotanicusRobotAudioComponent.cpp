#include "Gardening/BotanicusRobotAudioComponent.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Audio/BotanicusAudioCatalog.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"
UBotanicusRobotAudioComponent::UBotanicusRobotAudioComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    const FString Root=TEXT("/Game/Botanicus/Items/robot_aroseur/Sounds/");
    auto Ref=[&](const TCHAR* Name){return TSoftObjectPtr<USoundBase>(FSoftObjectPath(Root+Name+TEXT(".")+Name));};
    PowerOnSound=Ref(TEXT("S_Robot_PowerOn"));PowerDownSound=Ref(TEXT("S_Robot_PowerDown"));RunSound=Ref(TEXT("S_Robot_Run"));DeploySound=Ref(TEXT("S_Robot_Deploy"));RetractSound=Ref(TEXT("S_Robot_Retract"));
}
UAudioComponent* UBotanicusRobotAudioComponent::MakeVoice(FName Name)
{
    auto* Voice=NewObject<UAudioComponent>(GetOwner(),Name);Voice->bAutoActivate=false;Voice->bAutoDestroy=false;Voice->bStopWhenOwnerDestroyed=true;
    Voice->SetupAttachment(GetOwner()->GetRootComponent());Voice->bAllowSpatialization=true;
    FSoundAttenuationSettings Settings;Settings.bAttenuate=true;Settings.bSpatialize=true;Settings.AttenuationShapeExtents=FVector(InnerRadius,0,0);Settings.FalloffDistance=FalloffDistance;
    Voice->AdjustAttenuation(Settings);Voice->RegisterComponentWithWorld(GetWorld());return Voice;
}
void UBotanicusRobotAudioComponent::BeginPlay()
{
    Super::BeginPlay();if(GetNetMode()==NM_DedicatedServer)return;
    LoadedSounds={PowerOnSound.LoadSynchronous(),PowerDownSound.LoadSynchronous(),RunSound.LoadSynchronous(),DeploySound.LoadSynchronous(),RetractSound.LoadSynchronous()};
    Motor=MakeVoice(TEXT("RobotMotorAudio"));Arm=MakeVoice(TEXT("RobotArmAudio"));Power=MakeVoice(TEXT("RobotPowerAudio"));Motor->SetSound(LoadedSounds[2]);
    WaterCatalog=GetDefault<UBotanicusAudioSettings>()->Catalog.LoadSynchronous();
    if(auto* R=Cast<ABotanicusGardeningRobot>(GetOwner()); R && WaterCatalog && WaterCatalog->WaterLoop)
    {
        Water=MakeVoice(TEXT("RobotWaterAudio"));
        Water->AttachToComponent(R->WaterOutlet,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        Water->bOverrideAttenuation=false;Water->AttenuationSettings=WaterCatalog->InteractionAttenuation;
        // One local voice per robot; do not consume the four-player manual watering concurrency group.
        Water->SetSound(WaterCatalog->WaterLoop);
    }
    // Initial replication/restore never replays an old departure or docking cue.
    if(auto* R=Cast<ABotanicusGardeningRobot>(GetOwner())){LastActivity=R->GetPresentation().Activity;LastStartedAt=R->GetPresentation().StartedAt;bInitialized=true;}
}
void UBotanicusRobotAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    for(auto* Voice:{Motor.Get(),Arm.Get(),Power.Get(),Water.Get()})if(Voice){Voice->Stop();Voice->DestroyComponent();}
    Motor=nullptr;Arm=nullptr;Power=nullptr;Water=nullptr;bWaterWanted=false;Super::EndPlay(Reason);
}
void UBotanicusRobotAudioComponent::PlayAction(UAudioComponent* Voice,USoundBase* Sound,float Volume,float Age)
{
    if(!Voice || !Sound || Volume<=0 || Age>=Sound->GetDuration())return;
    Voice->Stop();Voice->SetSound(Sound);Voice->FadeIn(.01f,FMath::Clamp(Volume,0.f,4.f),FMath::Max(0.f,Age));
}
void UBotanicusRobotAudioComponent::UpdatePresentation()
{
    auto* R=Cast<ABotanicusGardeningRobot>(GetOwner());if(!R || !Motor || !Arm || !Power || LoadedSounds.Num()!=5)return;
    const auto& P=R->GetPresentation();const auto A=P.Activity;const double Now=R->PresentationNow();
    const float Age=FMath::Max(0.f,float(Now-P.StartedAt));
    if(bInitialized && (LastActivity!=A || LastStartedAt!=P.StartedAt))
    {
        const auto Previous=LastActivity;LastActivity=A;LastStartedAt=P.StartedAt;
        if(A==EBotanicusRobotActivity::LeavingDock)PlayAction(Power,LoadedSounds[0],PowerOnVolume,Age);
        else if(A==EBotanicusRobotActivity::Docked && Previous==EBotanicusRobotActivity::ReversingToDock)
        {Motor->Stop();bMotorWanted=false;PlayAction(Power,LoadedSounds[1],PowerDownVolume,Age);}
        if(Previous==EBotanicusRobotActivity::PreparingWater || Previous==EBotanicusRobotActivity::FinishingWater)
            if(Arm->IsPlaying())Arm->FadeOut(.03f,0);
        bArmPending=A==EBotanicusRobotActivity::PreparingWater || A==EBotanicusRobotActivity::FinishingWater;
        bArmBaseline=false;ArmMotionAt=-1;
    }
    const bool bAllowed=R->bEnabled && !R->GetAttachParentActor() &&
        (A==EBotanicusRobotActivity::None || A==EBotanicusRobotActivity::ApproachingDock || A==EBotanicusRobotActivity::LeavingDock || A==EBotanicusRobotActivity::ReversingToDock);
    const float Speed=R->GetVelocity().Size2D();
    bool bWant=bMotorWanted?bAllowed && Speed>3:bAllowed && Speed>8;
    if(!bWant && bMotorWanted && bAllowed){if(QuietSince<0)QuietSince=Now;if(Now-QuietSince<.12)bWant=true;}else QuietSince=-1;
    if(bWant!=bMotorWanted)
    {
        bMotorWanted=bWant;
        if(bWant)Motor->FadeIn(FMath::Max(.01f,MotorFadeSeconds),FMath::Clamp(RunVolume,0.f,4.f));
        else if(Motor->IsPlaying())Motor->FadeOut(FMath::Max(.01f,MotorFadeSeconds),0);
    }
}

void UBotanicusRobotAudioComponent::ObserveEvaluatedArmMotion()
{
    // Runs only while a cue is pending, after the existing skeletal evaluation. No new tick or notify asset.
    if(!bArmPending || !Arm || LoadedSounds.Num()!=5)return;
    auto* R=Cast<ABotanicusGardeningRobot>(GetOwner());if(!R)return;
    const auto& P=R->GetPresentation();const auto A=P.Activity;
    if((A!=EBotanicusRobotActivity::PreparingWater && A!=EBotanicusRobotActivity::FinishingWater) || A!=LastActivity || P.StartedAt!=LastStartedAt)return;
    const double Now=R->PresentationNow();auto* Rig=R->GetMesh();
    if(ArmMotionAt<0)
    {
        const FTransform Torso=Rig->GetSocketTransform(TEXT("corps"),RTS_Component);
        const FVector VisualScale=Rig->GetComponentScale().GetAbs();
        static const FName Bones[]={TEXT("b1"),TEXT("b2"),TEXT("b3"),TEXT("poigné")};bool bMoved=false;
        for(int32 I=0;I<4;++I)
        {
            const FTransform Bone=Rig->GetSocketTransform(Bones[I],RTS_Component);
            // Torso-relative motion ignores braking/actor turns; retain model units then convert to world cm.
            const FVector Position=Torso.InverseTransformPositionNoScale(Bone.GetLocation())*VisualScale;
            const FQuat Rotation=Torso.GetRotation().Inverse()*Bone.GetRotation();
            const FVector Scale=Bone.GetScale3D()/Torso.GetScale3D();
            if(bArmBaseline)
                bMoved|=FVector::DistSquared(Position,ArmPositions[I])>.01f || Rotation.AngularDistance(ArmRotations[I])>FMath::DegreesToRadians(.5f) || FVector::DistSquared(Scale,ArmScales[I])>.000025f;
            else {ArmPositions[I]=Position;ArmRotations[I]=Rotation;ArmScales[I]=Scale;}
        }
        bArmBaseline=true;if(bMoved)ArmMotionAt=Now;
    }
    if(ArmMotionAt<0)return;
    const bool bDeploy=A==EBotanicusRobotActivity::PreparingWater;
    const double CueAt=ArmMotionAt+FMath::Max(0.f,bDeploy?DeploySoundOffset:RetractSoundOffset);
    if(Now<CueAt)return;
    PlayAction(Arm,LoadedSounds[bDeploy?3:4],bDeploy?DeployVolume:RetractVolume,FMath::Max(0.f,float(Now-CueAt)));
    bArmPending=false;
}

void UBotanicusRobotAudioComponent::SetWaterActive(bool bActive)
{
    if(!Water || !WaterCatalog || bWaterWanted==bActive)return;
    bWaterWanted=bActive;
    if(bActive)Water->FadeIn(FMath::Max(.01f,WaterFadeSeconds),WaterCatalog->WaterVolume*FMath::Clamp(WaterVolumeMultiplier,0.f,2.f));
    else if(Water->IsPlaying())Water->FadeOut(FMath::Max(.01f,WaterFadeSeconds),0.f);
}
