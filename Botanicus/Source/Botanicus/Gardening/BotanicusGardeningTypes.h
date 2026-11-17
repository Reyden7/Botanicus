#pragma once
#include "CoreMinimal.h"
#include "BotanicusGardeningTypes.generated.h"

UENUM(BlueprintType)
enum class EBotanicusGardeningState : uint8
{
    Inactive, IdleAtBase, SearchingPlant, MovingToPlant, Watering,
    ReturningToBase, RefillingWater, Charging, Blocked
};

/** Water is measured in full watering-can reservoirs (the existing 0..1 unit). */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusGardeningConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float BatteryRuntimeHours=6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float BatteryRechargeHours=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="0.9")) float BatteryReturnThreshold=.10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float WaterCapacity=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.001")) float RefillRate=.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float MovementSpeed=180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.001")) float WateringRate=.12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100")) float PlantSearchRadius=2000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.5")) float PlantSearchInterval=3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float TargetWaterRatio=.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="40")) float WateringDistance=110;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="2")) float DockingDistance=12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float UnreachableRetrySeconds=30;
};

/** Nested in the station's existing world-item snapshot; no independent duplicate spawn pass. */
USTRUCT()
struct BOTANICUS_API FBotanicusGardeningSave
{
    GENERATED_BODY()
    UPROPERTY() bool bInstalled=false;
    UPROPERTY() FGuid BaseId;
    UPROPERTY() FGuid RobotId;
    UPROPERTY() FSoftClassPath RobotClass;
    UPROPERTY() FTransform RobotTransform=FTransform::Identity;
    UPROPERTY() float Battery=1;
    UPROPERTY() float Water=1;
    UPROPERTY() bool bEnabled=true;
    UPROPERTY() bool bAwaitFullCharge=false;
    UPROPERTY() EBotanicusGardeningState State=EBotanicusGardeningState::Inactive;
};

/** Transient activity gates; intentionally absent from the save schema. */
UENUM(BlueprintType)
enum class EBotanicusRobotActivity : uint8
{
    None, StoppingForWater, AimingAtPlant, PreparingWater, Watering, FinishingWater,
    ApproachingDock, StoppingAtDock, TurningAtDock, ReversingToDock, Docked, LeavingDock
};
USTRUCT(BlueprintType)
struct FBotanicusRobotPresentation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EBotanicusRobotActivity Activity=EBotanicusRobotActivity::None;
    UPROPERTY(BlueprintReadOnly) double StartedAt=0;
    UPROPERTY(BlueprintReadOnly) float Duration=0;
    UPROPERTY(BlueprintReadOnly) float AnimationStart=0;
};
