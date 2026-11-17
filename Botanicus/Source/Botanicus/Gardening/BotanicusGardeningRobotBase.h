#pragma once
#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Gardening/BotanicusGardeningTypes.h"
#include "BotanicusGardeningRobotBase.generated.h"
class ABotanicusGardeningRobot;
class UPointLightComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class UBoxComponent;
UCLASS()
class BOTANICUS_API ABotanicusGardeningRobotBase : public ABotanicusLargeEquipmentActor
{
    GENERATED_BODY()
public:
    ABotanicusGardeningRobotBase();
    virtual void OnConstruction(const FTransform& Transform) override;
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
    FVector DockForward() const;
    FVector ApproachLocation() const;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Station") TObjectPtr<USceneComponent> BaseVisualPivot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Dock") TObjectPtr<USceneComponent> ApproachPoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Station") TObjectPtr<USceneComponent> StationCollisionRoot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Station") TObjectPtr<UBoxComponent> StationBackCollision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Station") TObjectPtr<UBoxComponent> StationRoofCollision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Station") TObjectPtr<UBoxComponent> StationFootCollision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Station") TObjectPtr<UBoxComponent> StationInteractionTarget;
    UPROPERTY() FVector DockApproachLocalOffset=FVector(120,0,0);
    bool IsStationAvailable() const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot") FBotanicusGardeningConfig Config;
    UPROPERTY(EditAnywhere,Category="Robot") TSubclassOf<ABotanicusGardeningRobot> RobotClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Water") bool bWaterSupplyEnabled=true;
    UPROPERTY(EditAnywhere,Category="Robot|Indicator") FLinearColor ChargingColor=FLinearColor(.8f,.001f,.001f);
    UPROPERTY(EditAnywhere,Category="Robot|Indicator") FLinearColor FullColor=FLinearColor(.15f,1,.15f);
    UPROPERTY() FVector BatteryDisplayLocalOffset=FVector(-60.2,0,55);
    UPROPERTY() FRotator BatteryDisplayLocalRotation=FRotator::ZeroRotator;
    UPROPERTY() float BatteryDisplayScale=.04f;
    UPROPERTY() FVector LEDLocalOffset=FVector(26,-21,15);
    UPROPERTY(EditAnywhere,Category="Robot|Indicator",meta=(ClampMin="0")) float LEDIntensity=1.5f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Indicator") TObjectPtr<UWidgetComponent> BatteryDisplay;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot|Indicator") TObjectPtr<UStaticMeshComponent> PowerLED;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot") TObjectPtr<USceneComponent> DockPoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") TObjectPtr<ABotanicusGardeningRobot> Robot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Replicated,Category="Robot") FGuid BaseId;
private:
    void EnsureRobot();
    void SetupIndicators();
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LEDMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> IntegratedLEDOffMaterial;
    bool bLastPowerReady=false,bHasLEDState=false;
    void UpdateIndicator();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Indicator;
    FBotanicusGardeningSave PendingSave;
    bool bInstalled=false,bEnding=false,bSpawnedOnce=false;
    double NextSpawnAttempt=0;
    FTimerHandle StationTimer;
};
