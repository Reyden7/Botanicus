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
struct FBotanicusAutomaticWateringNeed;
class UBotanicusReservoirVisualComponent;
UCLASS()
class BOTANICUS_API ABotanicusGardeningRobot : public ACharacter, public IBotanicusInteractable
{
    GENERATED_BODY()
public:
    ABotanicusGardeningRobot();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Reservoir") TObjectPtr<UBotanicusReservoirVisualComponent> ReservoirVisual;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    void InitializeRobot(ABotanicusGardeningRobotBase* Base,const FBotanicusGardeningSave& Saved);
    FBotanicusGardeningSave Capture() const;
    void SetEnabled(bool bValue);
    bool IsDocked() const;
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
    void Search();
    void ReturnToBase(bool bLowBattery=false);
    void ReleaseTarget();
    bool Navigate(const FVector& Point,EBotanicusGardeningState NextState);
    bool Path(const FVector& Start,const FVector& End,float& Length) const;
    bool Approach(ABotanicusPlantPotActor* Pot,int32 Slot,FVector& Point,float& Length) const;
    bool HasWaterLine(const FVector& Soil) const;
    bool CanAfford(float Distance,float Water) const;
    void AccountResources(float Minutes,float Seconds);
    void FailMove();
    void UpdateCosmetics();
    UFUNCTION() void OnRep_Water();
    bool DropCarriedRobot();
    UFUNCTION() void OnRep_Carrier();
    UPROPERTY(Replicated) TObjectPtr<ABotanicusPlantPotActor> Target;
    UPROPERTY(Replicated) int32 TargetSlot=0;
    UPROPERTY(Replicated) FVector WaterTarget=FVector::ZeroVector;
    UPROPERTY(ReplicatedUsing=OnRep_Carrier) TObjectPtr<ACharacter> Carrier;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Stream;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Splash;
    TMap<TWeakObjectPtr<ABotanicusPlantPotActor>,double> RetryAfter;
    FVector Destination=FVector::ZeroVector,LastProgressLocation=FVector::ZeroVector;
    double LastUpdate=-1,NextSearch=0,MoveStarted=0,LastProgress=0;
    float LastClock=-1,ReturnPathLength=0;
    double NextReturnPathCheck=0;
    bool bAwaitFullCharge=false,bFirstWaterPulse=true,bDropping=false;
    int32 MoveFailures=0;
    FTimerHandle UpdateTimer;
};
