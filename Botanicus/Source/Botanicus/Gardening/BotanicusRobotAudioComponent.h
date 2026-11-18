#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gardening/BotanicusGardeningTypes.h"
#include "BotanicusRobotAudioComponent.generated.h"
UENUM(BlueprintType)
enum class EBotanicusRobotReaction:uint8 {Start,PlantFound,Watering,Success,NoWater,LowBattery,Charging,Charged,Blocked,Ouch};
USTRUCT(BlueprintType)
struct FBotanicusRobotVoiceSet
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<TSoftObjectPtr<class USoundBase>> Sounds;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="2")) float VolumeMultiplier=1;
};
class UAudioComponent;
class USoundBase;
class UBotanicusAudioCatalog;
/** Local audio driven by existing replicated activity + visible movement; no audio RPC or component tick. */
UCLASS(ClassGroup=(Botanicus),meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusRobotAudioComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBotanicusRobotAudioComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void UpdatePresentation();
    void SetWaterActive(bool bActive);
    bool PlayRobotReaction(EBotanicusRobotReaction Reaction,float Age=0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Voice",meta=(ClampMin="0",ClampMax="4")) float RobotVoiceVolume=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Voice",meta=(ClampMin="0",Units="s")) float OrdinaryVoiceCooldown=1.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Voice") TMap<EBotanicusRobotReaction,FBotanicusRobotVoiceSet> RobotVoices;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",ClampMax="2")) float WaterVolumeMultiplier=1;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0.01",ClampMax="0.15",Units="s")) float WaterFadeSeconds=.06f;
    void ObserveEvaluatedArmMotion();
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",UIMax="0.3",Units="s")) float DeploySoundOffset=0;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",UIMax="0.3",Units="s")) float RetractSoundOffset=0;
    UPROPERTY(EditAnywhere,Category="Robot|Audio") TSoftObjectPtr<USoundBase> PowerOnSound;
    UPROPERTY(EditAnywhere,Category="Robot|Audio") TSoftObjectPtr<USoundBase> PowerDownSound;
    UPROPERTY(EditAnywhere,Category="Robot|Audio") TSoftObjectPtr<USoundBase> RunSound;
    UPROPERTY(EditAnywhere,Category="Robot|Audio") TSoftObjectPtr<USoundBase> DeploySound;
    UPROPERTY(EditAnywhere,Category="Robot|Audio") TSoftObjectPtr<USoundBase> RetractSound;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",ClampMax="4")) float PowerOnVolume=.65f;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",ClampMax="4")) float PowerDownVolume=.65f;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",ClampMax="4")) float RunVolume=1;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",ClampMax="4")) float DeployVolume=1.4f;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0",ClampMax="4")) float RetractVolume=1.4f;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0")) float InnerRadius=100;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="1")) float FalloffDistance=900;
    UPROPERTY(EditAnywhere,Category="Robot|Audio",meta=(ClampMin="0.01")) float MotorFadeSeconds=.08f;
private:
    void InitializeVoice();
    void UpdateVoice();
    void ObserveVoiceWater(bool bActive);
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Expression;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> LoadedVoices;
    TMap<EBotanicusRobotReaction,int32> LastVoiceIndex;
    TWeakObjectPtr<AActor> VoiceTarget;
    int32 VoiceTargetSlot=INDEX_NONE,CurrentVoicePriority=0;
    EBotanicusGardeningState VoiceState=EBotanicusGardeningState::Inactive;
    EBotanicusRobotActivity VoiceActivity=EBotanicusRobotActivity::None;
    double NextOrdinaryVoice=0,VoiceCompleted=-1;
    bool bVoiceInitialized=false,bVoiceNoWater=false,bVoiceLowBattery=false,bVoiceCharging=false,bVoiceFull=false,bVoiceWatering=false;
    UAudioComponent* MakeVoice(FName Name);
    void PlayAction(UAudioComponent* Voice,USoundBase* Sound,float Volume,float Age);
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Motor;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Arm;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Power;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> LoadedSounds;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Water;
    UPROPERTY(Transient) TObjectPtr<UBotanicusAudioCatalog> WaterCatalog;
    bool bWaterWanted=false;
    bool bInitialized=false,bMotorWanted=false;
    EBotanicusRobotActivity LastActivity=EBotanicusRobotActivity::None;
    double LastStartedAt=-1,QuietSince=-1;
    bool bArmPending=false,bArmBaseline=false;
    double ArmMotionAt=-1;
    FVector ArmPositions[4];
    FQuat ArmRotations[4];
    FVector ArmScales[4];
};
