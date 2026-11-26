#include "BotanicusRobotAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Presentation/BotanicusHeldItemProfile.h"

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
	LeftHeldPoseAlpha = 0.0f;
	RightHeldPoseAlpha = 0.0f;
	bHasHeldItem = false;
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
	CarryPoseAlpha = FMath::FInterpConstantTo(CarryPoseAlpha, PoseTarget, DeltaSeconds, 1.0f / FMath::Max(0.01f, bIsCarryingBox ? CarryPoseBlendInDuration : CarryPoseBlendOutDuration));
	HandIKAlpha = FMath::FInterpConstantTo(HandIKAlpha, bIsCarryingBox ? 1.0f : 0.0f,
		DeltaSeconds, 1.0f / FMath::Max(0.01f, HandIKBlendDuration));
}

void UBotanicusRobotAnimInstance::UpdateHandTargets(const ABotanicusDeliveryParcelActor* Parcel,
	const FBotanicusHeldItemPose& Held, float DeltaSeconds)
{
	const USkeletalMeshComponent& Mesh = *GetSkelMeshComponent();
	const auto* Carton = Parcel ? &Parcel->GetCarryProfile() : nullptr;
	const auto UpdateArm = [&](bool bLeft, FCarryArm& Arm, FVector& Target, FVector& Elbow, FRotator& Wrist, float& Weight)
	{
		TOptional<FTransform> Grip = bLeft ? Held.Left : Held.Right;
		TOptional<FTransform> Alternate;
		FVector ElbowDirection = bLeft ? LeftElbowDirection + Held.LeftElbowOffset : RightElbowDirection + Held.RightElbowOffset;
		if (Carton)
		{
			Grip = Parcel->GetHandGripTransform(bLeft, false, *Carton);
			Alternate = Parcel->GetHandGripTransform(bLeft, true, *Carton);
			ElbowDirection += bLeft ? Carton->LeftElbowOffset : Carton->RightElbowOffset;
		}
		UpdateCarryArm(Mesh, Grip, Alternate, bLeft, ElbowDirection,
			(bLeft ? LeftCarryWrist : RightCarryWrist).Quaternion(), Arm, Target, Elbow, Wrist, Weight, DeltaSeconds);
		if (!Carton && Grip.IsSet())
			Weight *= bLeft ? Held.LeftInfluence : Held.RightInfluence;
	};

	UpdateArm(true, LeftArm, LeftHandIKTarget, LeftElbowIKTarget, LeftWristIKRotation, LeftHandIKAlpha);
	UpdateArm(false, RightArm, RightHandIKTarget, RightElbowIKTarget, RightWristIKRotation, RightHandIKAlpha);

	const auto UpdateArmLayer = [&](float Previous, bool bActive, const FCarryArm& Arm, float Influence)
	{
		const float Duration = bActive ? CarryPoseBlendInDuration : CarryPoseBlendOutDuration;
		const float Target = bActive ? FMath::Lerp(0.45f, 1.0f, Arm.ReachWeight) * Influence : 0.0f;
		return FMath::FInterpConstantTo(Previous, Target, DeltaSeconds, 1.0f / FMath::Max(0.01f, Duration));
	};
	LeftHeldPoseAlpha = UpdateArmLayer(LeftHeldPoseAlpha, Held.Left.IsSet(), LeftArm, Held.LeftLayerInfluence);
	RightHeldPoseAlpha = UpdateArmLayer(RightHeldPoseAlpha, Held.Right.IsSet(), RightArm, Held.RightLayerInfluence);
}

void UBotanicusRobotAnimInstance::UpdateCarryArm(const USkeletalMeshComponent& Mesh, const TOptional<FTransform>& Primary,
	const TOptional<FTransform>& Alternate, bool bLeft, const FVector& ElbowDirection,
	const FQuat& RestWrist, FCarryArm& Arm, FVector& Target, FVector& Elbow,
	FRotator& Wrist, float& Weight, float DeltaSeconds)
{
	Arm.GripActivation = FMath::FInterpConstantTo(Arm.GripActivation, Primary.IsSet() ? 1.f : 0.f,
		DeltaSeconds, 1.f / FMath::Max(0.01f, HandIKBlendDuration));
	if (!Primary.IsSet())
	{
		Weight = FMath::Min(Weight, Arm.GripActivation * Arm.ReachWeight);
		if (Arm.GripActivation <= 0.f)
		{
			Arm.bHasTarget = false;
			Arm.ReachWeight = 0.f;
		}
		return;
	}

	const TArray<FTransform>& Pose = Mesh.GetComponentSpaceTransforms();
	if (!Pose.IsValidIndex(Arm.Upper) || !Pose.IsValidIndex(Arm.Forearm) || !Pose.IsValidIndex(Arm.Hand))
	{
		Weight = 0.0f;
		return;
	}

	const FTransform& ComponentToWorld = Mesh.GetComponentTransform();
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
	const auto ContactWeight = [&](const FVector& Position)
	{
		const float Distance = FVector::Distance(Shoulder, Position);
		const float ReachWeight = FMath::SmoothStep(MinReach - ReachFadeDistance, MinReach, Distance)
			* (1.0f - FMath::SmoothStep(Reach - ReachFadeDistance, Reach + 2.0f, Distance));
		const float Side = Body.UnrotateVector(Position).X * (bLeft ? 1.0f : -1.0f);
		const float Forward = Body.UnrotateVector(Position - Shoulder).Y;
		return ReachWeight * FMath::SmoothStep(-7.0f, 4.0f, Side) * FMath::SmoothStep(-2.0f, 8.0f, Forward);
	};

	const FVector SidePosition = ComponentToWorld.InverseTransformPosition(Primary->GetLocation());
	const float SideWeight = ContactWeight(SidePosition);
	FVector UnderPosition = FVector::ZeroVector;
	float UnderWeight = 0.f;
	if (Alternate.IsSet())
	{
		UnderPosition = ComponentToWorld.InverseTransformPosition(Alternate->GetLocation());
		UnderWeight = ContactWeight(UnderPosition);
	}

	// Fade through a grip change instead of moving the wrist through the object.
	const bool bPreferUnder = Alternate.IsSet() && (Arm.bUnder
		? SideWeight < 0.88f && UnderWeight + 0.12f >= SideWeight
		: SideWeight < 0.72f && UnderWeight > SideWeight + 0.15f);
	const float Smoothing = 1.0f - FMath::Exp(-HandIKInterpolationSpeed * DeltaSeconds);
	if (!Alternate.IsSet() || !Arm.bHasTarget || (Arm.ReachWeight < 0.10f && bPreferUnder != Arm.bUnder))
		Arm.bUnder = bPreferUnder;

	const FTransform& Grip = Arm.bUnder ? *Alternate : *Primary;
	const float GripWeight = Arm.bUnder ? UnderWeight : SideWeight;
	Target = Arm.bUnder ? UnderPosition : SidePosition;
	const float ReachTarget = bPreferUnder == Arm.bUnder ? GripWeight : 0.0f;

	const FQuat GripRotation = ComponentToWorld.InverseTransformRotation(Grip.GetRotation());
	const FQuat DesiredWrist = Pose[Arm.Forearm].GetRotation().Inverse() * GripRotation;
	const float WristAngle = RestWrist.AngularDistance(DesiredWrist);
	const float WristLimit = FMath::DegreesToRadians(WristRotationLimit);
	const float WristFactor = WristAngle > WristLimit ? WristLimit / WristAngle : 1.0f;
	const FQuat LimitedWrist = FQuat::Slerp(RestWrist, DesiredWrist, WristFactor);

	if (!Arm.bHasTarget)
	{
		Arm.ParentSpaceWrist = RestWrist;
		Arm.bHasTarget = true;
	}
	Elbow = Shoulder + Body.RotateVector(ElbowDirection);
	Arm.ParentSpaceWrist = FQuat::Slerp(Arm.ParentSpaceWrist, LimitedWrist, Smoothing);
	Wrist = Arm.ParentSpaceWrist.Rotator();
	// A disappearing contact must also stop pulling on the elbow immediately.
	Arm.ReachWeight = FMath::Min(GripWeight, FMath::Lerp(Arm.ReachWeight, ReachTarget, Smoothing));
	Weight = Arm.GripActivation * Arm.ReachWeight;
}
