#include "Presentation/BotanicusHeldItemComponent.h"

#include "BotanicusCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	// Grip orientation in the character's transport frame, measured on the robot's carry pose.
	const FRotator LeftWrist(60.05, 46.32, -79.23);
	const FRotator RightWrist(-56.43, 148.97, -58.95);
}

FBotanicusHeldItemSettings UBotanicusHeldItemComponent::ResolveSettings(const FBotanicusHeldItemSettings& Catalogue) const
{
	return bOverrideCatalogueGrip || GripSettings.Profile != EBotanicusHeldItemProfile::None ? GripSettings : Catalogue;
}

void UBotanicusHeldItemComponent::Bind(ABotanicusCharacter* Character, USceneComponent& InMount,
	UPrimitiveComponent& InGeometry, USceneComponent* InLeftGrip, USceneComponent* InRightGrip,
	EBotanicusHeldItemSource InSource, const FBotanicusHeldItemSettings& Settings)
{
	if (!IsValid(Character) || Character->IsLocallyControlled() || GetNetMode() == NM_DedicatedServer)
	{
		Unbind();
		return;
	}
	if (Carrier.Get() != Character || Mount.Get() != &InMount)
	{
		Unbind();
		Carrier = Character;
		Mount = &InMount;
		OriginalRelativeTransform = InMount.GetRelativeTransform();
		Source = InSource;
		if (Source == EBotanicusHeldItemSource::Book)
			Character->GetRobotMesh()->AddTickPrerequisiteComponent(&InGeometry);
		Character->RegisterHeldItem(this, true);
	}

	Geometry = &InGeometry;
	LeftGrip = InLeftGrip;
	RightGrip = InRightGrip;
	Source = InSource;
	SetSettings(Settings);
}

void UBotanicusHeldItemComponent::Unbind()
{
	if (ABotanicusCharacter* Character = Carrier.Get())
	{
		if (Source == EBotanicusHeldItemSource::Book && Geometry.IsValid())
			Character->GetRobotMesh()->RemoveTickPrerequisiteComponent(Geometry.Get());
		Character->RegisterHeldItem(this, false);
	}
	if (USceneComponent* Presentation = Mount.Get(); Presentation && GetOwner() && Presentation != GetOwner()->GetRootComponent())
		Presentation->SetRelativeTransform(OriginalRelativeTransform);
	Carrier.Reset();
	Mount.Reset();
	ToolMotion = {};
	LastLocalMount.Reset();
	PreviousBookMount.Reset();
	BookPoseBlendTime = 0.f;
	PreviousVelocity.Reset();
	SwayResponse = FVector::ZeroVector;
}

void UBotanicusHeldItemComponent::SetSettings(const FBotanicusHeldItemSettings& Settings)
{
	if (Source == EBotanicusHeldItemSource::Book && ActiveSettings.Profile != Settings.Profile)
	{
		PreviousBookMount = LastLocalMount;
		BookPoseBlendTime = 0.f;
	}
	ActiveSettings = Settings;
	if (HasMount())
		UpdateMount(0.f);
}

void UBotanicusHeldItemComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Unbind();
	Super::EndPlay(Reason);
}

FBotanicusHeldItemPose UBotanicusHeldItemComponent::UpdateMount(float DeltaSeconds)
{
	FBotanicusHeldItemPose Pose;
	const ABotanicusCharacter* Character = Carrier.Get();
	USceneComponent* Presentation = Mount.Get();
	const UPrimitiveComponent* VisualGeometry = Geometry.Get();
	const EBotanicusHeldItemProfile Profile = ActiveSettings.Profile;
	if (!Character || !Presentation || !VisualGeometry || Profile == EBotanicusHeldItemProfile::None)
		return Pose;

	const bool bLeft = Profile != EBotanicusHeldItemProfile::RightHand;
	const bool bRight = Profile != EBotanicusHeldItemProfile::LeftHand;
	const bool bTwoHands = bLeft && bRight;
	const FTransform& Body = Character->GetMesh()->GetComponentTransform();
	const FQuat PivotRotation = Body.GetRotation() * Character->GetBaseRotationOffset().Inverse();
	const FVector PivotLocation = Body.GetLocation() - PivotRotation.RotateVector(Character->GetBaseTranslationOffset());
	const FVector Velocity = Character->GetVelocity();
	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		const FVector Acceleration = PreviousVelocity.IsSet()
			? PivotRotation.UnrotateVector((Velocity - *PreviousVelocity) / DeltaSeconds) : FVector::ZeroVector;
		const FVector Desired = FVector(-Acceleration.X, -Acceleration.Y, 0.0).GetClampedToMaxSize(1800.0) / 1800.0;
		const double Alpha = 1.0 - FMath::Exp(-ActiveSettings.CarrySwayDamping * DeltaSeconds);
		SwayResponse = FMath::Lerp(SwayResponse, Desired, Alpha);
		PreviousVelocity = Velocity;
	}
	const FQuat SwayRotation(FVector(-SwayResponse.Y, SwayResponse.X, 0.0).GetSafeNormal(),
		FMath::DegreesToRadians(ActiveSettings.CarrySwayMaxAngle) * SwayResponse.Size());

	const FTransform Current = Presentation->GetComponentTransform();
	const FVector Scale = Current.GetScale3D();
	const float AimPitch = FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch),
		-ActiveSettings.AimPitchLimit, ActiveSettings.AimPitchLimit);
	const FTransform GeometryRelative = VisualGeometry->GetComponentTransform().GetRelativeTransform(Current);
	FQuat ItemRotation = ToolMotion.Rotation.Quaternion() * FRotator(AimPitch, 0.0, 0.0).Quaternion() * ActiveSettings.CarryRelativeRotation.Quaternion();
	if (ActiveSettings.bAlignToMesh)
		ItemRotation *= GeometryRelative.GetRotation().Inverse();
	const FTransform ItemFrame(ItemRotation, FVector::ZeroVector, Scale);
	const FTransform GeometryInCarryFrame = GeometryRelative * ItemFrame;
	const auto* StaticMesh = Cast<UStaticMeshComponent>(VisualGeometry);
	// BoundsScale controls rendering and shadows. Use the mesh's dimensions for contact and clearance.
	const FBox Bounds = StaticMesh && StaticMesh->GetStaticMesh()
		? StaticMesh->GetStaticMesh()->GetBoundingBox().TransformBy(GeometryInCarryFrame)
		: VisualGeometry->CalcBounds(GeometryInCarryFrame).GetBox();
	const FVector Extent = Bounds.GetExtent();

	auto GetContact = [&](bool bIsLeft, const USceneComponent* Grip)
	{
		if (ActiveSettings.bUseAuthoredGrips && Grip)
			return Grip->GetComponentTransform().GetRelativeTransform(Current) * ItemFrame;

		const double Sign = bIsLeft ? -1.0 : 1.0;
		FVector Position = Bounds.GetCenter() + FVector(-Extent.X * 0.55, Sign * Extent.Y, -Extent.Z * 0.35);
		if (Profile == EBotanicusHeldItemProfile::TwoHandsLarge)
			Position = FVector(Bounds.Min.X + Extent.X * 0.3, Bounds.GetCenter().Y + Sign * FMath::Min(Extent.Y, 24.0),
				Bounds.Min.Z - 0.5);
		else if (!bTwoHands)
			Position = Bounds.GetCenter() + FVector(-Extent.X * 0.35, Sign * Extent.Y * 0.60, -Extent.Z * 0.55);
		return FTransform((bIsLeft ? LeftWrist : RightWrist).Quaternion(), Position);
	};

	const FTransform Left = GetContact(true, LeftGrip.Get());
	const FTransform Right = GetContact(false, RightGrip.Get());
	const FVector Middle = bTwoHands ? (Left.GetLocation() + Right.GetLocation()) * 0.5
		: (bLeft ? Left.GetLocation() : Right.GetLocation());
	FVector Offset = ActiveSettings.CarryRelativeLocation + ToolMotion.Offset - Middle;
	// A tool held alongside the hip can overlap the torso's depth without intersecting it.
	const bool bBesideTorso = !bTwoHands && (bRight
		? Offset.Y + Bounds.Min.Y >= 16.0 : Offset.Y + Bounds.Max.Y <= -16.0);
	if (!bTwoHands && ActiveSettings.bCarryAlongside)
	{
		// Vary the lateral clearance during a tilt without switching to a distant front mount.
		Offset.Y = bRight ? FMath::Max(Offset.Y, 16.0 - Bounds.Min.Y)
			: FMath::Min(Offset.Y, -16.0 - Bounds.Max.Y);
	}
	else if (!bBesideTorso)
		Offset.X = FMath::Max(Offset.X, 11.0 - Bounds.Min.X);
	const FTransform Pivot(PivotRotation, PivotLocation);
	FTransform LocalMount(SwayRotation * ItemRotation, Offset + SwayResponse * ActiveSettings.CarrySwayMaxOffset);
	if (PreviousBookMount.IsSet())
	{
		// Blend only the change of book layout, in the same carrier frame as the current contacts.
		BookPoseBlendTime += DeltaSeconds;
		const float Alpha = FMath::Clamp(BookPoseBlendTime / 0.08f, 0.f, 1.f);
		LocalMount.Blend(*PreviousBookMount, LocalMount, FMath::SmoothStep(0.f, 1.f, Alpha));
		if (Alpha >= 1.f)
			PreviousBookMount.Reset();
	}
	LastLocalMount = LocalMount;
	const FTransform WorldMount = LocalMount * Pivot;
	Presentation->SetWorldLocationAndRotation(WorldMount.GetLocation(), WorldMount.GetRotation());
	const FTransform Frame(WorldMount.GetRotation() * ItemRotation.Inverse(), WorldMount.GetLocation());

	if (bLeft)
		Pose.Left = Left * Frame;
	if (bRight)
		Pose.Right = Right * Frame;
	return Pose;
}

