#include "Presentation/BotanicusEmoteMotion.h"

#include "Animation/AnimSequence.h"
#include "BotanicusCharacter.h"
#include "Components/SkeletalMeshComponent.h"

const FName BotanicusEmoteMotion::SourceName(TEXT("RobotEmote"));

FTransform BotanicusEmoteMotion::ProjectPlanar(const FTransform& First, const FTransform& Pose)
{
	const FQuat RelativeRotation = Pose.GetRotation() * First.GetRotation().Inverse();
	const FQuat Yaw = FRotator(0., RelativeRotation.Rotator().Yaw, 0.).Quaternion();
	const FVector Translation = Pose.GetTranslation() - First.GetTranslation();
	return FTransform(Yaw, FVector(Translation.X, Translation.Y, 0.));
}

FBotanicusEmoteMotionSource::FBotanicusEmoteMotionSource()
{
	InstanceName = BotanicusEmoteMotion::SourceName;
	Priority = 1000;
	AccumulateMode = ERootMotionAccumulateMode::Override;
	FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
}

FRootMotionSource* FBotanicusEmoteMotionSource::Clone() const
{
	return new FBotanicusEmoteMotionSource(*this);
}

UScriptStruct* FBotanicusEmoteMotionSource::GetScriptStruct() const
{
	return StaticStruct();
}

bool FBotanicusEmoteMotionSource::Matches(const FRootMotionSource* Other) const
{
	return Super::Matches(Other) && Revision == static_cast<const FBotanicusEmoteMotionSource*>(Other)->Revision;
}

void FBotanicusEmoteMotionSource::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Sequence);
}

bool FBotanicusEmoteMotionSource::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!Super::NetSerialize(Ar, Map, bOutSuccess))
		return false;

	Ar << Sequence;
	Ar << PlayRate;
	Ar << Revision;
	Ar << InitialYaw;
	return true;
}

void FBotanicusEmoteMotionSource::PrepareRootMotion(float SimulationTime, float MovementTickTime,
	const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();
	const auto* Robot = Cast<ABotanicusCharacter>(&Character);
	if (!Sequence || !Robot || MovementTickTime <= UE_SMALL_NUMBER)
		return;

	const double Length = Sequence->GetPlayLength();
	const double Start = GetTime() * PlayRate;
	const double End = (GetTime() + SimulationTime) * PlayRate;
	const bool bLoop = Duration < 0.f;
	double Position = bLoop ? FMath::Fmod(Start, Length) : FMath::Min(Start, Length);
	double Remaining = bLoop ? End - Start : FMath::Min(End, Length) - Position;
	const FTransform First = Sequence->ExtractRootTrackTransform(FAnimExtractContext(), nullptr);
	const auto Sample = [this, &First](double Time)
	{
		return BotanicusEmoteMotion::ProjectPlanar(First, Sequence->ExtractRootTrackTransform(FAnimExtractContext(Time), nullptr));
	};
	const double CycleYaw = bLoop ? Sample(Length).Rotator().Yaw : 0.;
	double Heading = InitialYaw + (bLoop ? FMath::FloorToDouble(Start / Length) * CycleYaw : 0.);
	FVector Translation = FVector::ZeroVector;
	double Yaw = Heading + Sample(Position).Rotator().Yaw;

	// The robot is attached to the smoothed movement mesh. Use the authored basis
	// and the starting heading so prediction or smoothing cannot bend the path.
	const USkeletalMeshComponent* RobotMesh = Robot->GetRobotMesh();
	const FQuat MeshToActor = Character.GetBaseRotationOffset() * RobotMesh->GetRelativeRotation().Quaternion();
	const FVector MeshScale = RobotMesh->GetComponentScale();

	// Accumulate the tail and the head separately at a loop seam.
	while (Remaining > UE_SMALL_NUMBER)
	{
		const double Step = FMath::Min(Length - Position, Remaining);
		const FTransform From = Sample(Position);
		const FTransform To = Sample(Position + Step);
		const FQuat Basis = FRotator(0., Heading, 0.).Quaternion() * MeshToActor;
		Translation += Basis.RotateVector((To.GetTranslation() - From.GetTranslation()) * MeshScale);
		Yaw = Heading + To.Rotator().Yaw;
		Remaining -= Step;
		Heading += CycleYaw;
		Position = 0.;
	}
	// CharacterMovement applies this relative rotation to the capsule after the move.
	const FQuat TargetRotation = FRotator(0., Yaw, 0.).Quaternion();
	const FQuat RotationDelta = TargetRotation * Character.GetActorQuat().Inverse();
	RootMotionParams.Set(FTransform(RotationDelta, Translation / MovementTickTime));
	SetTime(GetTime() + SimulationTime);
}

FVector UBotanicusEmoteMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	const auto* Robot = Cast<ABotanicusCharacter>(CharacterOwner);
	return Robot && Robot->IsRobotEmotePlaying() ? FVector::ZeroVector : Super::ConstrainInputAcceleration(InputAcceleration);
}

bool UBotanicusEmoteMovementComponent::CanCrouchInCurrentState() const
{
	const auto* Robot = Cast<ABotanicusCharacter>(CharacterOwner);
	return (!Robot || !Robot->IsRobotEmotePlaying()) && Super::CanCrouchInCurrentState();
}
