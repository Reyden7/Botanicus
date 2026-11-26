#include "BotanicusRobotAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"

namespace
{
	// Local wrist rotations in the carry sample at frame 23 (60 fps).
	const FRotator LeftCarryWrist(-41.12, -157.18, 152.53);
	const FRotator RightCarryWrist(-46.26, 37.53, -7.27);
	constexpr float ReachFadeDistance = 5.0f;
}

void UBotanicusRobotAnimInstance::ResetBoxCarry()
{
	bIsCarryingBox = false;
	bIsInspectingBox = false;
	CarryPoseAlpha = 0.0f;
	HandIKAlpha = 0.0f;
	LeftHandIKAlpha = 0.0f;
	RightHandIKAlpha = 0.0f;
	LeftArm = {};
	RightArm = {};
	LeftWristIKRotation = LeftCarryWrist;
	RightWristIKRotation = RightCarryWrist;

	if (const USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
	{
		LeftArm.Upper = Mesh->GetBoneIndex(TEXT("l-arm"));
		LeftArm.Forearm = Mesh->GetBoneIndex(TEXT("l-forearm"));
		LeftArm.Hand = Mesh->GetBoneIndex(TEXT("l-hand"));
		RightArm.Upper = Mesh->GetBoneIndex(TEXT("r-arm"));
		RightArm.Forearm = Mesh->GetBoneIndex(TEXT("r-forearm"));
		RightArm.Hand = Mesh->GetBoneIndex(TEXT("r-hand"));
	}
}

void UBotanicusRobotAnimInstance::UpdateBoxCarryState(const ABotanicusDeliveryParcelActor* Parcel, float DeltaSeconds)
{
	bIsCarryingBox = Parcel != nullptr;
	bIsInspectingBox = bIsCarryingBox && !Parcel->IsCarriedPlacementPreview();

	const float Support = FMath::Max(LeftArm.ReachWeight, RightArm.ReachWeight);
	const float PoseTarget = bIsCarryingBox ? FMath::Lerp(0.45f, 1.0f, Support) : 0.0f;
	CarryPoseAlpha = FMath::FInterpConstantTo(CarryPoseAlpha, PoseTarget, DeltaSeconds, 1.0f / FMath::Max(0.01f, CarryPoseBlendDuration));
	HandIKAlpha = FMath::FInterpConstantTo(HandIKAlpha, bIsCarryingBox ? 1.0f : 0.0f,
		DeltaSeconds, 1.0f / FMath::Max(0.01f, HandIKBlendDuration));
}

void UBotanicusRobotAnimInstance::UpdateBoxHandTargets(const ABotanicusDeliveryParcelActor* Parcel, float DeltaSeconds)
{
	if (!Parcel)
	{
		LeftHandIKAlpha = FMath::Min(LeftHandIKAlpha, HandIKAlpha * LeftArm.ReachWeight);
		RightHandIKAlpha = FMath::Min(RightHandIKAlpha, HandIKAlpha * RightArm.ReachWeight);
		if (HandIKAlpha <= 0.0f)
		{
			LeftArm.bHasTarget = false;
			RightArm.bHasTarget = false;
			LeftArm.ReachWeight = 0.0f;
			RightArm.ReachWeight = 0.0f;
		}
		return;
	}

	const USkeletalMeshComponent& Mesh = *GetSkelMeshComponent();
	UpdateCarryArm(Mesh, Parcel->GetHandGripTransform(true), LeftElbowDirection, LeftCarryWrist.Quaternion(), LeftArm,
		LeftHandIKTarget, LeftElbowIKTarget, LeftWristIKRotation, LeftHandIKAlpha, DeltaSeconds);
	UpdateCarryArm(Mesh, Parcel->GetHandGripTransform(false), RightElbowDirection, RightCarryWrist.Quaternion(), RightArm,
		RightHandIKTarget, RightElbowIKTarget, RightWristIKRotation, RightHandIKAlpha, DeltaSeconds);
}

void UBotanicusRobotAnimInstance::UpdateCarryArm(const USkeletalMeshComponent& Mesh, const FTransform& Grip,
	const FVector& ElbowDirection, const FQuat& RestWrist, FCarryArm& Arm, FVector& Target, FVector& Elbow,
	FRotator& Wrist, float& Weight, float DeltaSeconds)
{
	const TArray<FTransform>& Pose = Mesh.GetComponentSpaceTransforms();
	if (!Pose.IsValidIndex(Arm.Upper) || !Pose.IsValidIndex(Arm.Forearm) || !Pose.IsValidIndex(Arm.Hand))
	{
		Weight = 0.0f;
		return;
	}

	const FTransform& ComponentToWorld = Mesh.GetComponentTransform();
	const FVector Desired = ComponentToWorld.InverseTransformPosition(Grip.GetLocation());
	const FQuat Body = BodyRotation.Quaternion();
	const FVector Shoulder = Pose[Arm.Upper].GetLocation();
	const FVector Forearm = Pose[Arm.Forearm].GetLocation();
	const FVector Hand = Pose[Arm.Hand].GetLocation();

	// This rig has inherited bone scale, so measure limb lengths in component space.
	const float UpperLength = FVector::Distance(Shoulder, Forearm);
	const float ForearmLength = FVector::Distance(Forearm, Hand);
	const float Reach = (UpperLength + ForearmLength) * FMath::Clamp(MaxArmStretch, 0.8f, 1.0f);
	const float MinReach = FMath::Sqrt(FMath::Square(UpperLength) + FMath::Square(ForearmLength)
		+ 2.0f * UpperLength * ForearmLength * FMath::Cos(FMath::DegreesToRadians(MaxElbowFlexion)));
	const float Distance = FVector::Distance(Shoulder, Desired);
	const FVector RelativeToShoulder = Body.UnrotateVector(Desired - Shoulder);
	const FVector RelativeToBody = Body.UnrotateVector(Desired);
	const float Side = RelativeToBody.X * (&Arm == &LeftArm ? 1.0f : -1.0f);
	const float ReachWeight = FMath::SmoothStep(MinReach - ReachFadeDistance, MinReach, Distance)
		* (1.0f - FMath::SmoothStep(Reach - ReachFadeDistance, Reach + ReachFadeDistance, Distance));
	const float SideWeight = FMath::SmoothStep(-7.0f, 4.0f, Side);
	const float FrontWeight = FMath::SmoothStep(-2.0f, 8.0f, static_cast<float>(RelativeToShoulder.Y));

	const FQuat GripRotation = ComponentToWorld.InverseTransformRotation(Grip.GetRotation());
	const FQuat DesiredWrist = Pose[Arm.Forearm].GetRotation().Inverse() * GripRotation;
	const float WristAngle = RestWrist.AngularDistance(DesiredWrist);
	const float WristLimit = FMath::DegreesToRadians(WristRotationLimit);
	const float WristFactor = WristAngle > WristLimit ? WristLimit / WristAngle : 1.0f;
	const FQuat LimitedWrist = FQuat::Slerp(RestWrist, DesiredWrist, WristFactor);

	if (!Arm.bHasTarget)
	{
		Target = Desired;
		Arm.ParentSpaceWrist = RestWrist;
		Arm.bHasTarget = true;
	}
	const float Smoothing = 1.0f - FMath::Exp(-HandIKInterpolationSpeed * DeltaSeconds);
	Target = FMath::Lerp(Target, Desired, Smoothing);
	Elbow = Shoulder + Body.RotateVector(ElbowDirection);
	Arm.ParentSpaceWrist = FQuat::Slerp(Arm.ParentSpaceWrist, LimitedWrist, Smoothing);
	Wrist = Arm.ParentSpaceWrist.Rotator();
	Arm.ReachWeight = FMath::Lerp(Arm.ReachWeight, ReachWeight * SideWeight * FrontWeight, Smoothing);
	Weight = HandIKAlpha * Arm.ReachWeight;
}
