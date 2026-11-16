#pragma once
#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Gardening/BotanicusGardeningTypes.h"
#include "BotanicusGardeningRobotBase.generated.h"
class ABotanicusGardeningRobot;
class UPointLightComponent;
UCLASS()
class BOTANICUS_API ABotanicusGardeningRobotBase : public ABotanicusLargeEquipmentActor
{
    GENERATED_BODY()
public:
    ABotanicusGardeningRobotBase();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual void ConfirmPlacement() override;
    FBotanicusGardeningSave CaptureRobot() const;
    void RestoreRobot(const FBotanicusGardeningSave& Saved);
    FVector DockLocation() const;
    bool IsStationAvailable() const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot") FBotanicusGardeningConfig Config;
    UPROPERTY(EditAnywhere,Category="Robot") TSubclassOf<ABotanicusGardeningRobot> RobotClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Water") bool bWaterSupplyEnabled=true;
    UPROPERTY(EditAnywhere,Category="Robot|Indicator") FLinearColor ChargingColor=FLinearColor(1,.35f,.03f);
    UPROPERTY(EditAnywhere,Category="Robot|Indicator") FLinearColor FullColor=FLinearColor(.15f,1,.15f);
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot") TObjectPtr<USceneComponent> DockPoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") TObjectPtr<ABotanicusGardeningRobot> Robot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") FGuid BaseId;
private:
    void EnsureRobot();
    void UpdateIndicator();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Indicator;
    FBotanicusGardeningSave PendingSave;
    bool bInstalled=false,bEnding=false,bSpawnedOnce=false;
    double NextSpawnAttempt=0;
    FTimerHandle StationTimer;
};
