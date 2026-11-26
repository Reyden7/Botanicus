#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Curves/CurveFloat.h"
#include "BotanicusRobotAnimInstance.generated.h"

class ABotanicusCharacter;
class UBotanicusMultiToolComponent;
class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class ERobotAirPose : uint8
{
	Ground,
	Jump,
	Falling,
	Landing
};

UENUM(BlueprintType)
enum class ERobotGroundPose : uint8
{
	Locomotion,
	RelaxedIdle,
	ObservationIdle,
	TurnLeft,
	TurnRight
};

UCLASS(Transient, Blueprintable)
class BOTANICUS_API UBotanicusRobotAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UBotanicusRobotAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Locomotion", meta = (BlueprintThreadSafe))
	bool WantsAirPose(ERobotAirPose Pose) const { return AirPose == Pose; }

	UFUNCTION(BlueprintPure, Category = "Locomotion", meta = (BlueprintThreadSafe))
	bool WantsGroundPose(ERobotGroundPose Pose, bool bInvert) const { return (GroundPose == Pose) != bInvert; }

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float LocomotionPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Gait", meta = (ClampMin = "0", Units = "cm/s"))
	float MovingThreshold = 3.0f;

	// Multipliers on the playback rates configured for each sample in BS_Robot_Locomotion.
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Gait", meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float WalkPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Gait", meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float RunPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Turn", meta = (ClampMin = "15.0", ClampMax = "120.0", Units = "Degrees"))
	float TurnThreshold = 55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Turn", meta = (ClampMin = "45.0", Units = "DegreesPerSecond"))
	float TurnSpeed = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Turn", meta = (ClampMin = "0.25", Units = "s"))
	float MinimumTurnDuration = 0.40f;

	// Authored yaw of Turn_right over normalized time. Turn_Left is its mirrored counterpart.
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Turn")
	FRuntimeFloatCurve AuthoredTurnYaw;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Idle", meta = (ClampMin = "1.0", Units = "s"))
	float IdleVariationMinDelay = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Idle", meta = (ClampMin = "1.0", Units = "s"))
	float IdleVariationMaxDelay = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Idle", meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float IdleVariationPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0", Units = "s"))
	float JumpLiftTime = 0.80f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0", Units = "s"))
	float JumpApexTime = 1.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0", Units = "s"))
	float FallingPoseTime = 1.33f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne")
	FRuntimeFloatCurve AuthoredJumpLift;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0", Units = "s"))
	float LandingContactTime = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0", Units = "s"))
	float LandingRecoveryTime = 1.18f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0.12", Units = "s"))
	float LandingDuration = 0.34f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Airborne", meta = (ClampMin = "0.12", Units = "s"))
	float MovingLandingDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Look", meta = (ClampMin = "0", ClampMax = "70", Units = "Degrees"))
	float HeadYawLimit = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Look", meta = (ClampMin = "0", ClampMax = "70", Units = "Degrees"))
	float HeadPitchUpLimit = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Look", meta = (ClampMin = "0", ClampMax = "70", Units = "Degrees"))
	float HeadPitchDownLimit = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Look", meta = (ClampMin = "0.1"))
	float HeadInterpolationSpeed = 7.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Debug")
	ERobotAirPose AirPose = ERobotAirPose::Ground;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Debug")
	ERobotGroundPose GroundPose = ERobotGroundPose::Locomotion;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Airborne")
	float JumpTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Airborne")
	FVector JumpTranslation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Airborne")
	FVector FallingTranslation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Airborne")
	float LandingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Idle")
	float IdleVariationTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Turn")
	float TurnTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Turn")
	FRotator TurnClipCorrection = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Turn")
	FRotator BodyRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Look")
	FRotator NeckRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Look")
	FRotator SkullRotation = FRotator::ZeroRotator;

private:
	void UpdateAirPose(const UCharacterMovementComponent& CharacterMovement, float DeltaSeconds);
	void UpdateGroundPose(const ABotanicusCharacter& Owner, float ActorYaw, float DeltaSeconds);
	void UpdateLookPose(const ABotanicusCharacter& Owner, float DeltaSeconds);
	bool HasActiveInteractionPose(const ABotanicusCharacter& Owner) const;
	void AdvanceTurn(float DeltaSeconds);
	void ResetIdleVariation();

	TWeakObjectPtr<ABotanicusCharacter> Character;
	TWeakObjectPtr<UCharacterMovementComponent> Movement;
	TWeakObjectPtr<UBotanicusMultiToolComponent> MultiTool;
	FRandomStream IdleRandom;
	bool bHasIdleSeed = false;
	bool bHasFacing = false;
	bool bWasIdleEligible = false;
	float BodyYaw = 0.0f;
	float TurnStartYaw = 0.0f;
	float TurnAngle = 0.0f;
	float TurnElapsed = 0.0f;
	float TurnCooldownRemaining = 0.0f;
	float IdleDelayRemaining = 0.0f;
	float AirborneDuration = 0.0f;
	float TakeoffVelocity = 0.0f;
	float LandingProgress = 0.0f;
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;
};
