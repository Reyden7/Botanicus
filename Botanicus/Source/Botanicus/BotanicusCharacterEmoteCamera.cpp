#include "BotanicusCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

bool ABotanicusCharacter::AddEmoteCameraOrbit(float Yaw, float Pitch)
{
	if (!IsLocallyControlled() || !IsRobotEmotePlaying())
		return false;

	EmoteCameraOrbit.Yaw = FMath::Clamp(EmoteCameraOrbit.Yaw + Yaw, -40., 40.);
	EmoteCameraOrbit.Pitch = FMath::Clamp(EmoteCameraOrbit.Pitch + Pitch, -18., 18.);
	return true;
}

FVector ABotanicusCharacter::SweepEmoteCamera(const FVector& Focus, const FVector& Destination) const
{
	if (!bEmoteCameraCollisionEnabled || Focus.Equals(Destination))
		return Destination;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusEmoteCamera), false, this);
	const FCollisionShape Probe = FCollisionShape::MakeSphere(FMath::Max(1.f, EmoteSpringArm->ProbeSize));
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Focus, Destination, FQuat::Identity, EmoteSpringArm->ProbeChannel, Probe, Query))
		return Hit.bStartPenetrating ? Focus : Hit.Location;
	return Destination;
}

FVector ABotanicusCharacter::ResolveEmoteCameraObstacles(const FVector& Focus, const FVector& DesiredOffset)
{
	if (!bEmoteCameraCollisionEnabled)
		return DesiredOffset;

	const FVector Front = SweepEmoteCamera(Focus, Focus + DesiredOffset) - Focus;
	if (Front.Equals(DesiredOffset, 1.))
	{
		EmoteCameraObstacleAngle = 0.;
		return Front;
	}

	// Favor the front and keep the same side when two passages are similarly clear.
	static constexpr double Angles[] = {35., -35., 70., -70., 105., -105., 145., -145., 180.};
	FVector Best = Front;
	double BestAngle = 0.;
	double BestScore = Front.Size() - FMath::Abs(EmoteCameraObstacleAngle) * .15;
	for (double Angle : Angles)
	{
		const FVector Candidate = DesiredOffset.RotateAngleAxis(Angle, FVector::UpVector);
		const FVector Safe = SweepEmoteCamera(Focus, Focus + Candidate) - Focus;
		const double Score = Safe.Size() - FMath::Abs(Angle) * .45 -
			FMath::Abs(FMath::FindDeltaAngleDegrees(EmoteCameraObstacleAngle, Angle)) * .15;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestAngle = Angle;
			Best = Safe;
		}
	}
	EmoteCameraObstacleAngle = BestAngle;
	return Best;
}

void ABotanicusCharacter::SetEmoteCameraBodyVisible(bool bShowRobot)
{
	if (!SavedEmoteCameraVisibility.IsSet() || bEmoteCameraShowsRobot == bShowRobot)
		return;

	bEmoteCameraShowsRobot = bShowRobot;
	const FEmoteCameraVisibility& Saved = *SavedEmoteCameraVisibility;
	GetMesh()->SetOwnerNoSee(bShowRobot || Saved.bLegacyOwnerNoSee);
	RobotMesh->SetOwnerNoSee(!bShowRobot && Saved.bRobotOwnerNoSee);
	RobotMesh->SetVisibility(bShowRobot || Saved.bRobotVisible, false);
	RobotMesh->SetHiddenInGame(!bShowRobot && Saved.bRobotHiddenInGame, false);
	FirstPersonMesh->SetVisibility(!bShowRobot && Saved.bArmsVisible, false);
}

void ABotanicusCharacter::ResetEmoteCamera()
{
	if (SavedEmoteCameraVisibility.IsSet())
	{
		SetEmoteCameraBodyVisible(false);
		RobotMesh->VisibilityBasedAnimTickOption = SavedEmoteCameraVisibility->AnimTickOption;
		SavedEmoteCameraVisibility.Reset();
	}
	EmoteCameraBlend = 0.f;
	EmoteCameraObstacleAngle = 0.;
	EmoteCameraOrbit = FRotator::ZeroRotator;
}

void ABotanicusCharacter::EndViewTarget(APlayerController* PC)
{
	if (PC && PC->IsLocalPlayerController() && PC->GetPawn() == this)
		ResetEmoteCamera();
	Super::EndViewTarget(PC);
}

void ABotanicusCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	if (!IsLocallyControlled())
		return;

	const bool bEmoteActive = IsRobotEmotePlaying();
	if (!bEmoteActive && !SavedEmoteCameraVisibility.IsSet())
		return;

	const bool bEnter = !SavedEmoteCameraVisibility.IsSet();
	if (bEnter)
	{
		SavedEmoteCameraVisibility = {!!GetMesh()->bOwnerNoSee, !!RobotMesh->bOwnerNoSee,
			RobotMesh->IsVisible(), !!RobotMesh->bHiddenInGame, FirstPersonMesh->IsVisible(), RobotMesh->VisibilityBasedAnimTickOption};
		RobotMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		EmoteCameraHeading = GetActorRotation().Yaw;
	}

	const float PreviousBlend = EmoteCameraBlend;
	const float Duration = bEmoteActive ? EmoteCameraBlendInDuration : EmoteCameraBlendOutDuration;
	const float Goal = bEmoteActive ? 1.f : 0.f;
	EmoteCameraBlend = Duration > UE_SMALL_NUMBER
		? FMath::FInterpConstantTo(EmoteCameraBlend, Goal, DeltaTime, 1.f / Duration)
		: Goal;
	if (!bEmoteActive && EmoteCameraBlend <= 0.f)
	{
		ResetEmoteCamera();
		return;
	}

	const double HeadingDelta = FMath::FindDeltaAngleDegrees(EmoteCameraHeading, GetActorRotation().Yaw);
	EmoteCameraHeading += HeadingDelta * FMath::Clamp(DeltaTime * 9., 0., 1.);

	// Use the capsule for the framing, even when the animated head and torso are moving.
	const FTransform Pivot = EmoteCameraPivot->GetComponentTransform();
	const FQuat HeadingCorrection = FRotator(0., EmoteCameraHeading - GetActorRotation().Yaw, 0.).Quaternion();
	const FQuat PivotRotation = HeadingCorrection * Pivot.GetRotation();
	const FVector Focus = Pivot.GetLocation() +
		PivotRotation.RotateVector(EmoteSpringArm->GetRelativeLocation() + FVector(0., 0., EmoteCameraHeightOffset)) +
		EmoteSpringArm->TargetOffset;
	const FQuat ArmRotation = PivotRotation * EmoteCameraOrbit.Quaternion() * EmoteSpringArm->GetRelativeRotation().Quaternion();
	const FVector Desired = ArmRotation.RotateVector(FVector(-EmoteCameraDistance, EmoteCameraHorizontalOffset, 0.) +
		EmoteSpringArm->SocketOffset + EmoteCamera->GetRelativeLocation());
	const FVector SafeOffset = ResolveEmoteCameraObstacles(Focus, Desired);
	EmoteCameraOffset = bEnter ? SafeOffset : FMath::VInterpTo(EmoteCameraOffset, SafeOffset, DeltaTime, 14.f);

	FMinimalViewInfo ThirdPerson;
	EmoteCamera->GetCameraView(DeltaTime, ThirdPerson);
	ThirdPerson.Location = SweepEmoteCamera(Focus, Focus + EmoteCameraOffset);
	const float BodyClearance = GetCapsuleComponent()->GetScaledCapsuleRadius() + 25.f;
	if (FVector::DistSquared2D(GetActorLocation(), ThirdPerson.Location) <= FMath::Square(BodyClearance))
	{
		// With no usable position outside the body, keep a clear first-person view.
		EmoteCameraBlend = 0.f;
		SetEmoteCameraBodyVisible(false);
		return;
	}
	const FRotator LookAt = (Focus - ThirdPerson.Location).Rotation();
	ThirdPerson.Rotation = (LookAt.Quaternion() * EmoteCameraRotationOffset.Quaternion() *
		EmoteCamera->GetRelativeRotation().Quaternion()).Rotator();
	ThirdPerson.FOV = EmoteCameraFOV;

	// Unwrap near the half turn to keep the same path through the blend.
	const FRotator FirstPersonRotation = OutResult.Rotation;
	const double YawDelta = FMath::FindDeltaAngleDegrees(FirstPersonRotation.Yaw, ThirdPerson.Rotation.Yaw);
	EmoteCameraYawDelta = PreviousBlend <= 0.f || PreviousBlend >= 1.f
		? YawDelta : EmoteCameraYawDelta + FMath::FindDeltaAngleDegrees(EmoteCameraYawDelta, YawDelta);
	const float Weight = FMath::SmoothStep(0.f, 1.f, EmoteCameraBlend);
	if (Weight >= 1.f)
	{
		OutResult = ThirdPerson;
	}
	else
	{
		OutResult.BlendViewInfo(ThirdPerson, Weight);
		OutResult.Rotation.Yaw = FRotator::NormalizeAxis(FirstPersonRotation.Yaw + EmoteCameraYawDelta * Weight);
		OutResult.Location = SweepEmoteCamera(Focus, OutResult.Location);
	}

	// Swap representations only when the lens clears the body.
	SetEmoteCameraBodyVisible(FVector::DistSquared2D(GetActorLocation(), OutResult.Location) > FMath::Square(BodyClearance));
}

