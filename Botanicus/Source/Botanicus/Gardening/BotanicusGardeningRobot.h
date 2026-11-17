#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/BotanicusInteractable.h"
#include "Gardening/BotanicusGardeningTypes.h"
#include "BotanicusGardeningRobot.generated.h"
class ABotanicusGardeningRobotBase;
class ABotanicusPlantPotActor;
class UNiagaraComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UAnimInstance;
class UAnimSequence;
struct FBotanicusAutomaticWateringNeed;
class UBotanicusReservoirVisualComponent;
class UBotanicusRobotAudioComponent;
UCLASS()
class BOTANICUS_API ABotanicusGardeningRobot : public ACharacter, public IBotanicusInteractable
{
    GENERATED_BODY()
public:
    ABotanicusGardeningRobot();
    /** The calibrated bone mount is internal; edit WaterTankPivot with the Blueprint transform gizmo. */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Reservoir") TObjectPtr<USceneComponent> WaterTankMount;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Reservoir") TObjectPtr<USceneComponent> WaterTankPivot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Audio") TObjectPtr<UBotanicusRobotAudioComponent> RobotAudio;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Reservoir") TObjectPtr<UBotanicusReservoirVisualComponent> ReservoirVisual;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    const FBotanicusRobotPresentation& GetPresentation() const{return Presentation;}
    double PresentationNow() const;
    UPROPERTY(EditAnywhere,Category="Robot|Appearance") TSoftObjectPtr<USkeletalMesh> RobotSkeletalMesh;
    UPROPERTY(EditAnywhere,Category="Robot|Animation") TSoftClassPtr<UAnimInstance> RobotAnimationBlueprint;
    UPROPERTY(EditAnywhere,Category="Robot|Animation") TSoftObjectPtr<UAnimSequence> StartRunAnimation;
    UPROPERTY(EditAnywhere,Category="Robot|Animation") TSoftObjectPtr<UAnimSequence> StopRunAnimation;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Replicated,Category="Robot|Animation",meta=(ClampMin="0.01")) float StopRunPlayRate=1.75f;
    UPROPERTY(EditAnywhere,Category="Robot|Animation") TSoftObjectPtr<UAnimSequence> ArmAnimation;
    UPROPERTY(EditAnywhere,Category="Robot|Animation") TSoftObjectPtr<UAnimSequence> WaterAnimation;
    UPROPERTY(EditAnywhere,Category="Robot|Animation") TSoftObjectPtr<UAnimSequence> RunAnimation;
    UPROPERTY() FVector VisualScale=FVector(.11f);
    UPROPERTY() FRotator VisualRotation=FRotator(0,-90,0);
    UPROPERTY() FVector VisualOffset=FVector(0,0,-41.4888f);
    UPROPERTY(EditAnywhere,Category="Robot|Reservoir") FName ReservoirSocket=TEXT("corps");
    UPROPERTY(EditAnywhere,Category="Robot|Reservoir") FVector ReservoirMeshSpaceCenter=FVector(0,-290,470);
    UPROPERTY(EditAnywhere,Category="Robot|Appearance") FVector OutletMeshSpacePosition=FVector(-275,414,340);
    UPROPERTY(EditAnywhere,Category="Robot|Reservoir") FVector ReservoirScale=FVector(1.7f,1.7f,2.5f);
    UPROPERTY(EditAnywhere,Category="Robot|Appearance") FName OutletSocket=TEXT("poigné");
    UPROPERTY(EditAnywhere,Category="Robot|Dock",meta=(ClampMin="5",Units="cm/s")) float DockSpeed=35;
    UPROPERTY(EditAnywhere,Category="Robot|Dock",meta=(ClampMin="10",Units="deg/s")) float DockTurnSpeed=90;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    void InitializeRobot(ABotanicusGardeningRobotBase* Base,const FBotanicusGardeningSave& Saved);
    FBotanicusGardeningSave Capture() const;
    void SetEnabled(bool bValue);
    bool IsDocked() const;
    bool CanLaunchRound() const;
    bool LaunchRound();
    bool HasPowerForTask() const;
    FText Describe() const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Replicated,Category="Robot") FBotanicusGardeningConfig Config;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot") TObjectPtr<USceneComponent> WaterOutlet;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") FGuid RobotId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") TObjectPtr<ABotanicusGardeningRobotBase> Base;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") float Battery=1;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,ReplicatedUsing=OnRep_Water,Category="Robot") float CurrentWater=1;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") bool bEnabled=true;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") EBotanicusGardeningState State=EBotanicusGardeningState::Inactive;
private:
    void UpdateRobot();
    void SetupAnimatedVisual();
    void SetActivity(EBotanicusRobotActivity Activity,float Duration=0,float AnimationStart=0);
    UFUNCTION() void OnRep_Activity();
    bool UpdateActivity(double Now);
    void BeginWaterPreparation();
    void FinishWatering(bool bReturn=false,bool bLowBattery=false);
    bool GetDockApproach(FVector& Point) const;
    bool DockLaneClear(const FVector& To) const;
    void FailDocking();
    float AnimationDuration(const TSoftObjectPtr<UAnimSequence>& Animation) const;
    float StopRunDuration() const {return AnimationDuration(StopRunAnimation)/FMath::Max(.01f,StopRunPlayRate);}
    UPROPERTY(ReplicatedUsing=OnRep_Activity) FBotanicusRobotPresentation Presentation;
    FVector PendingDestination=FVector::ZeroVector,DockDestination=FVector::ZeroVector;
    EBotanicusGardeningState PendingState=EBotanicusGardeningState::SearchingPlant;
    bool bReturnAfterFinish=false,bLowBatteryAfterFinish=false;
    double DockLastProgress=0;
    FVector DockLastPosition=FVector::ZeroVector;
    void Search();
    void ReturnToBase(bool bLowBattery=false);
    void ReleaseTarget();
    bool Navigate(const FVector& Point,EBotanicusGardeningState NextState);
    bool Path(const FVector& Start,const FVector& End,float& Length) const;
    bool Approach(AActor* Pot,int32 Slot,FVector& Point,float& Length) const;
    bool HasWaterLine(const FVector& Soil) const;
    float WateringApproachDistance() const;
    bool CanAfford(float Distance,float Water) const;
    void AccountResources(float Minutes,float Seconds);
    void FailMove();
    void UpdateCosmetics();
    UFUNCTION() void OnRep_Water();
    bool DropCarriedRobot();
    UFUNCTION() void OnRep_Carrier();
    UPROPERTY(Replicated) TObjectPtr<AActor> Target;
    UPROPERTY(Replicated) int32 TargetSlot=0;
    UPROPERTY(Replicated) FVector WaterTarget=FVector::ZeroVector;
    UPROPERTY(ReplicatedUsing=OnRep_Carrier) TObjectPtr<ACharacter> Carrier;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Stream;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Splash;
    TMap<TWeakObjectPtr<AActor>,double> RetryAfter;
    FVector Destination=FVector::ZeroVector,LastProgressLocation=FVector::ZeroVector;
    double LastUpdate=-1,NextSearch=0,MoveStarted=0,LastProgress=0;
    float LastClock=-1,ReturnPathLength=0;
    double NextReturnPathCheck=0;
    UPROPERTY(Replicated) bool bAwaitFullCharge=false;
    UPROPERTY(Replicated) bool bTaskPowerLimited=false;
    bool bFirstWaterPulse=true,bDropping=false;
    int32 MoveFailures=0;
    FTimerHandle UpdateTimer;
};
