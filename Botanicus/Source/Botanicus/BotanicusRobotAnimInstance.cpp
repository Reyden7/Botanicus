#include "BotanicusRobotAnimInstance.h"
#include "Delivery/BotanicusBotanistBookActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimSequence.h"

#include "BotanicusCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Tools/BotanicusMultiToolComponent.h"

namespace
{
	constexpr float TurnClipLength = 56.0f / 60.0f;
	constexpr float RelaxedIdleLength = 168.0f / 60.0f;
	constexpr float ObservationIdleLength = 380.0f / 60.0f;
	constexpr float MaxWalkingRealignment = 720.0f;
	constexpr float MinimumGroundContactAirTime = 0.075f;
	constexpr float RisingThreshold = 10.0f;
	constexpr float TurnExitBlend = 0.14f;

	bool IsTurning(ERobotGroundPose Pose)
	{
		return Pose == ERobotGroundPose::TurnLeft || Pose == ERobotGroundPose::TurnRight;
	}

	FRotator LookRotation(float Yaw, float Pitch, float PitchBasisYaw)
	{
		// The robot looks along mesh +Y. Rotate its +X pitch axis with the body pose.
		const FQuat Basis(FVector::UpVector, FMath::DegreesToRadians(PitchBasisYaw));
		const FVector PitchAxis = Basis.RotateVector(FVector::ForwardVector);
		const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		const FQuat PitchRotation(PitchAxis, FMath::DegreesToRadians(Pitch));
		return (YawRotation * PitchRotation).Rotator();
	}
}

UBotanicusRobotAnimInstance::UBotanicusRobotAnimInstance()
{
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
	InitializeRobotActionProfiles();
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ButtonPress(
		TEXT("/Game/Characters/Robot/Animation/Mixamo/interaction/button_press.button_press"));
	ButtonPressAnimation = ButtonPress.Object;
}

void UBotanicusRobotAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	ABotanicusCharacter* Owner = Cast<ABotanicusCharacter>(TryGetPawnOwner());
	Character = Owner;
	Movement = Owner ? Owner->GetCharacterMovement() : nullptr;
	MultiTool = Owner ? Owner->FindComponentByClass<UBotanicusMultiToolComponent>() : nullptr;

	Speed = 0.0f;
	Direction = 0.0f;
	Velocity = FVector::ZeroVector;
	Acceleration = FVector::ZeroVector;
	bIsMoving = false;
	bIsAccelerating = false;
	bIsInAir = false;
	bIsSprinting = false;
	LocomotionPlayRate = 1.0f;
	AirPose = ERobotAirPose::Ground;
	GroundPose = ERobotGroundPose::Locomotion;
	bHasFacing = false;
	bHasIdleSeed = false;
	bWasIdleEligible = false;
	IdleDelayRemaining = 0.0f;
	IdleVariationTime = 0.0f;
	TurnClipCorrection = FRotator::ZeroRotator;
	BodyRotation = FRotator::ZeroRotator;
	NeckRotation = FRotator::ZeroRotator;
	SkullRotation = FRotator::ZeroRotator;
	LookYaw = 0.0f;
	LookPitch = 0.0f;
	ResetBoxCarry();
	RobotAction = EBotanicusRobotAction::None;
	RobotActionAlpha = 0.f;
	ActiveRobotCue = {};
	RobotActionMount.Reset();
	InteractionMontage.Reset();
	EmoteMontage.Reset();
	EmoteRevision.Reset();
	AirborneDuration = 0.0f;
	TurnCooldownRemaining = 0.0f;
	JumpTime = JumpLiftTime;
	JumpTranslation = FVector::ZeroVector;
	FallingTranslation = FVector::ZeroVector;
	LandingTime = LandingContactTime;
	TurnTime = 0.0f;
}

void UBotanicusRobotAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ABotanicusCharacter* Owner = Character.Get();
	const UCharacterMovementComponent* CharacterMovement = Movement.Get();
	if (!Owner || !CharacterMovement)
	{
		return;
	}

	Velocity = CharacterMovement->Velocity;
	Acceleration = CharacterMovement->GetCurrentAcceleration();
	Speed = Velocity.Size2D();
	bIsMoving = Speed > MovingThreshold;
	bIsAccelerating = !Acceleration.IsNearlyZero();
	bIsInAir = CharacterMovement->IsFalling();
	bIsSprinting = Owner->IsSprinting();

	float TargetPlayRate = 1.0f;
	if (bIsMoving)
	{
		TargetPlayRate = bIsSprinting ? RunPlayRate : WalkPlayRate;
	}
	LocomotionPlayRate = FMath::FInterpTo(LocomotionPlayRate, TargetPlayRate, DeltaSeconds, 8.0f);

	const float ActorYaw = Owner->GetActorRotation().Yaw;
	if (!bHasFacing)
	{
		BodyYaw = ActorYaw;
		bHasFacing = true;
	}
	UpdateAirPose(*CharacterMovement, DeltaSeconds);
	const ABotanicusDeliveryParcelActor* Parcel = Owner->GetCarriedParcelForAnimation();
	const float EffectiveDilation = Owner->GetActorTimeDilation();
	const float CarryDeltaSeconds = DeltaSeconds / FMath::Max(EffectiveDilation, UE_SMALL_NUMBER);
	const bool bEmote = Owner->IsRobotEmotePlaying();
	const FBotanicusRobotToolMotion ToolMotion = UpdateRobotAction(*Owner, Owner->GetRobotHeldGrip(), Parcel != nullptr || bEmote, CarryDeltaSeconds);
	UpdateRobotEmoteMontage(*Owner);
	if (auto* Tool = MultiTool.Get(); Tool && !Owner->IsLocallyControlled())
	{
		EBotanicusMultiToolMode VisualMode = EBotanicusMultiToolMode::Neutral;
		if (bApplyRobotActionHands && RobotAction == EBotanicusRobotAction::Harvest)
			VisualMode = EBotanicusMultiToolMode::Shovel;
		else if (bApplyRobotActionHands && RobotAction == EBotanicusRobotAction::Whip)
			VisualMode = EBotanicusMultiToolMode::ElectricWhip;
		Tool->SetActionVisualMode(VisualMode);
	}
	FBotanicusHeldItemPose Held = Owner->UpdateHeldItemPose(DeltaSeconds, ToolMotion,
		bEmote || (bApplyRobotActionHands && RobotAction == EBotanicusRobotAction::PressComputer));
	ApplyRobotActionHands(*Owner, Held);
	bHasHeldItem = Held.IsSet();
	UpdateBoxCarryState(Parcel, CarryDeltaSeconds);
	UpdateGroundPose(*Owner, ActorYaw, DeltaSeconds);

	// The mesh inherits network smoothing from the legacy movement mesh.
	// Subtract its smoothed yaw to keep a stationary pose from chasing capsule corrections.
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const FQuat BaseRotation = Owner->GetBaseRotationOffset() * Mesh->GetRelativeRotation().Quaternion();
	const float SmoothedYaw = FRotator::NormalizeAxis(Mesh->GetComponentRotation().Yaw - BaseRotation.Rotator().Yaw);
	// The grip mount and torso share the same smoothed movement-mesh heading.
	BodyRotation.Yaw = (bIsCarryingBox || bHasHeldItem) ? 0.0f : FMath::FindDeltaAngleDegrees(SmoothedYaw, BodyYaw);
	UpdateLookPose(*Owner, DeltaSeconds);
	UpdateHandTargets(Parcel, Held, CarryDeltaSeconds);

	// Preserve the previous heading through the stop, while the Blend Space fades to idle.
	if (bIsMoving)
	{
		const FVector LocalVelocity = FRotator(0.0f, BodyYaw, 0.0f).UnrotateVector(Velocity);
		Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
	}
}

void UBotanicusRobotAnimInstance::UpdateAirPose(const UCharacterMovementComponent& CharacterMovement, float DeltaSeconds)
{
	if (bIsInAir)
	{
		const float VerticalSpeed = Velocity.Z;
		if (AirborneDuration <= 0.0f)
		{
			TakeoffVelocity = FMath::Max(RisingThreshold, FMath::Max(VerticalSpeed, CharacterMovement.JumpZVelocity));
		}
		AirborneDuration += DeltaSeconds;
		const FRichCurve* LiftCurve = AuthoredJumpLift.GetRichCurveConst();
		if (VerticalSpeed > RisingThreshold)
		{
			AirPose = ERobotAirPose::Jump;
			const float RiseProgress = 1.0f - FMath::Clamp(VerticalSpeed / TakeoffVelocity, 0.0f, 1.0f);
			JumpTime = FMath::Lerp(JumpLiftTime, JumpApexTime, RiseProgress);
			JumpTranslation.Z = -LiftCurve->Eval(JumpTime);
		}
		else if (AirPose != ERobotAirPose::Falling)
		{
			AirPose = ERobotAirPose::Falling;
			FallingTranslation.Z = -LiftCurve->Eval(FallingPoseTime);
		}
		return;
	}

	// A transition is driven by the movement component's return to ground, including on proxies.
	if (AirborneDuration >= MinimumGroundContactAirTime && CharacterMovement.IsMovingOnGround())
	{
		AirPose = ERobotAirPose::Landing;
		LandingProgress = 0.0f;
	}
	else if (AirPose != ERobotAirPose::Landing)
	{
		AirPose = ERobotAirPose::Ground;
	}
	AirborneDuration = 0.0f;

	if (AirPose == ERobotAirPose::Landing)
	{
		const float Duration = FMath::Max(0.12f, bIsMoving ? MovingLandingDuration : LandingDuration);
		LandingTime = FMath::Lerp(LandingContactTime, LandingRecoveryTime, LandingProgress);
		if (LandingProgress >= 1.0f)
		{
			AirPose = ERobotAirPose::Ground;
		}
		LandingProgress = FMath::Min(1.0f, LandingProgress + DeltaSeconds / Duration);
	}
}

bool UBotanicusRobotAnimInstance::HasActiveInteractionPose(const ABotanicusCharacter& Owner) const
{
	const UAnimInstance* LegacyAnimation = Owner.GetMesh()->GetAnimInstance();
	const UBotanicusMultiToolComponent* Tool = MultiTool.Get();
	return bIsCarryingBox || Owner.IsCarryingEquipment() || Owner.GetHeldWateringCan() ||
		(Tool && Tool->GetMode() != EBotanicusMultiToolMode::Neutral) ||
		IsAnyMontagePlaying() || (LegacyAnimation && LegacyAnimation->IsAnyMontagePlaying());
}

void UBotanicusRobotAnimInstance::ResetIdleVariation()
{
	IdleDelayRemaining = IdleRandom.FRandRange(IdleVariationMinDelay, FMath::Max(IdleVariationMinDelay, IdleVariationMaxDelay));
}

void UBotanicusRobotAnimInstance::AdvanceTurn(float DeltaSeconds)
{
	const float Duration = FMath::Max(MinimumTurnDuration, FMath::Abs(TurnAngle) / FMath::Max(1.0f, TurnSpeed));
	TurnElapsed += DeltaSeconds;
	const float Phase = FMath::Clamp(TurnElapsed / Duration, 0.0f, 1.0f);
	TurnTime = Phase * TurnClipLength;
	const FRichCurve* Curve = AuthoredTurnYaw.GetRichCurveConst();
	const float InitialYaw = Curve->Eval(0.0f);
	const float FinalYaw = Curve->Eval(1.0f, 122.0f);
	const float ClipYaw = Curve->Eval(Phase, 122.0f * Phase);
	const float Progress = FMath::GetMappedRangeValueClamped(FVector2f(InitialYaw, FinalYaw), FVector2f(0.0f, 1.0f), ClipYaw);
	BodyYaw = FRotator::NormalizeAxis(TurnStartYaw + TurnAngle * Progress);
	TurnClipCorrection.Yaw = -FMath::Sign(TurnAngle) * ClipYaw;
	if (Phase >= 1.0f)
	{
		GroundPose = ERobotGroundPose::Locomotion;
		TurnCooldownRemaining = TurnExitBlend;
	}
}

void UBotanicusRobotAnimInstance::UpdateGroundPose(const ABotanicusCharacter& Owner, float ActorYaw, float DeltaSeconds)
{
	if (!bHasIdleSeed)
	{
		const APlayerState* PlayerState = Owner.GetPlayerState();
		IdleRandom.Initialize(PlayerState ? PlayerState->GetPlayerId() : 0);
		bHasIdleSeed = true;
	}

	TurnCooldownRemaining = FMath::Max(0.0f, TurnCooldownRemaining - DeltaSeconds);
	if (bIsCarryingBox || bHasHeldItem)
	{
		bWasIdleEligible = false;
		GroundPose = ERobotGroundPose::Locomotion;
		BodyYaw = ActorYaw;
		return;
	}

	if (bIsMoving || AirPose != ERobotAirPose::Ground)
	{
		bWasIdleEligible = false;
		if (IsTurning(GroundPose))
		{
			TurnCooldownRemaining = TurnExitBlend;
		}
		GroundPose = ERobotGroundPose::Locomotion;
		BodyYaw = FRotator::NormalizeAxis(FMath::FixedTurn(BodyYaw, ActorYaw, MaxWalkingRealignment * DeltaSeconds));
		return;
	}

	if (IsTurning(GroundPose))
	{
		AdvanceTurn(DeltaSeconds);
		return;
	}

	const float RemainingYaw = FMath::FindDeltaAngleDegrees(BodyYaw, ActorYaw);
	if (TurnCooldownRemaining <= 0.0f && FMath::Abs(RemainingYaw) >= TurnThreshold)
	{
		const FRichCurve* Curve = AuthoredTurnYaw.GetRichCurveConst();
		const float ClipAngle = FMath::Abs(Curve->Eval(1.0f, 122.0f) - Curve->Eval(0.0f));
		TurnStartYaw = BodyYaw;
		TurnAngle = FMath::Clamp(RemainingYaw, -ClipAngle, ClipAngle);
		TurnElapsed = 0.0f;
		GroundPose = TurnAngle < 0.0f ? ERobotGroundPose::TurnLeft : ERobotGroundPose::TurnRight;
		bWasIdleEligible = false;
		AdvanceTurn(DeltaSeconds);
		return;
	}

	if (HasActiveInteractionPose(Owner))
	{
		bWasIdleEligible = false;
		GroundPose = ERobotGroundPose::Locomotion;
		return;
	}
	if (!bWasIdleEligible)
	{
		ResetIdleVariation();
		bWasIdleEligible = true;
	}

	if (GroundPose == ERobotGroundPose::Locomotion)
	{
		IdleDelayRemaining -= DeltaSeconds;
		if (IdleDelayRemaining <= 0.0f)
		{
			GroundPose = IdleRandom.RandRange(0, 1) == 0 ? ERobotGroundPose::RelaxedIdle : ERobotGroundPose::ObservationIdle;
			IdleVariationTime = 0.0f;
		}
		return;
	}

	const float ClipLength = GroundPose == ERobotGroundPose::RelaxedIdle ? RelaxedIdleLength : ObservationIdleLength;
	IdleVariationTime += DeltaSeconds * IdleVariationPlayRate;
	if (IdleVariationTime >= ClipLength)
	{
		GroundPose = ERobotGroundPose::Locomotion;
		ResetIdleVariation();
	}
}

void UBotanicusRobotAnimInstance::UpdateLookPose(const ABotanicusCharacter& Owner, float DeltaSeconds)
{
	FRotator Aim = Owner.GetBaseAimRotation().GetNormalized();
	if (const FBotanicusRobotActionProfile* Profile = ActionProfiles.Find(RobotAction); Profile && ActiveRobotCue.bHasTarget)
	{
		const auto* Book = Cast<ABotanicusBotanistBookActor>(ActiveRobotCue.Source.Get());
		const FVector Target = Book ? Book->GetReadingLookTarget() : ActiveRobotCue.Target;
		const FVector TowardTarget = Target - Owner.GetPawnViewLocation();
		if (!TowardTarget.IsNearlyZero())
		{
			FRotator GestureAim = TowardTarget.Rotation();
			if (Book)
				GestureAim.Pitch = -FMath::Min(Book->ReadingHeadPitch, FMath::Abs(GestureAim.Pitch));
			const float Weight = Book ? Book->GetReadingPoseWeight(Book->ReadingBlendIn, Book->ReadingBlendOut) : RobotActionAlpha;
			Aim = FQuat::Slerp(Aim.Quaternion(), GestureAim.Quaternion(), Profile->LookInfluence * Weight).Rotator();
		}
	}
	const float Yaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(BodyYaw, Aim.Yaw), -HeadYawLimit, HeadYawLimit);
	const float Pitch = FMath::Clamp(Aim.Pitch, -HeadPitchDownLimit, HeadPitchUpLimit);
	LookYaw = FMath::FInterpTo(LookYaw, Yaw, DeltaSeconds, HeadInterpolationSpeed);
	LookPitch = FMath::FInterpTo(LookPitch, Pitch, DeltaSeconds, HeadInterpolationSpeed);
	const float NeckShare = 0.35f;
	const float NeckYaw = LookYaw * NeckShare;
	NeckRotation = LookRotation(NeckYaw, LookPitch * NeckShare, BodyRotation.Yaw);
	SkullRotation = LookRotation(LookYaw - NeckYaw, LookPitch * (1.0f - NeckShare), BodyRotation.Yaw + NeckYaw);
}
