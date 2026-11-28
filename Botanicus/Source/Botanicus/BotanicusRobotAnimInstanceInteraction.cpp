#include "BotanicusRobotAnimInstance.h"
#include "Animation/AnimSequence.h"

#include "Animation/AnimMontage.h"
#include "BotanicusCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Delivery/BotanicusBotanistBookActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Presentation/BotanicusHeldItemComponent.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Tools/BotanicusMultiToolActor.h"

namespace
{
	using EAction = EBotanicusRobotAction;

	bool IsTool(const AActor* Item, EBotanicusMultiToolMode Mode, FName Standalone = NAME_None)
	{
		if (const auto* Tool = Cast<ABotanicusMultiToolActor>(Item))
			return Tool->GetMultiToolMode() == Mode;
		const auto* Placeable = Cast<ABotanicusPlaceableItemActor>(Item);
		return !Standalone.IsNone() && Placeable && Placeable->GetItemKey() == Standalone;
	}

	bool MatchesAction(EAction Action, const AActor* Item)
	{
		switch (Action)
		{
		case EAction::Water:
			return Item && Item->IsA<ABotanicusWateringCanActor>();
		case EAction::PourSoil:
			if (const auto* Placeable = Cast<ABotanicusPlaceableItemActor>(Item))
				return Placeable->GetItemKey() == TEXT("PottingSoil");
			return false;
		case EAction::Harvest:
			return (Item && Item->IsA<ABotanicusMultiToolActor>()) || IsTool(Item, EBotanicusMultiToolMode::Shovel, TEXT("GardenTrowel"));
		case EAction::CutTape:
			return IsTool(Item, EBotanicusMultiToolMode::Cutter, TEXT("BoxCutter"));
		case EAction::Whip:
			return Item && Item->IsA<ABotanicusMultiToolActor>();
		case EAction::Scan:
		case EAction::ScanConfirm:
			return Item && Item->IsA<ABotanicusCheckoutScanner>();
		case EAction::ReadBook:
			return Item && Item->IsA<ABotanicusBotanistBookActor>();
		default:
			return true;
		}
	}

	bool IsCoinAction(EAction Action)
	{
		return Action == EAction::CollectCoins || Action == EAction::ReceiveCoins;
	}
}

void UBotanicusRobotAnimInstance::InitializeRobotActionProfiles()
{
	auto& Computer = ActionProfiles.Add(EAction::PressComputer);
	Computer.BlendIn = .05f;
	Computer.BlendOut = .08f;
	Computer.IKInfluence = .75f;
	Computer.LookInfluence = .55f;

	auto& Plant = ActionProfiles.Add(EAction::Plant);
	Plant.ReachTowardTarget = 13.f;
	Plant.LocalWristRotation = FRotator(-16, 0, 8);
	Plant.ObjectTilt = FRotator(-16, 0, 6);

	auto& Water = ActionProfiles.Add(EAction::Water);
	Water.LocalHandOffset = FVector(2, -1.5, 3);
	Water.ObjectTilt = FRotator(-23, 0, -6);
	Water.SecondaryMotion = 0.45f;

	auto& Soil = ActionProfiles.Add(EAction::PourSoil);
	Soil.BlendIn = .10f;
	Soil.LocalHandOffset = FVector(3, 0, 4);
	Soil.ObjectTilt = FRotator(-28, 0, 0);
	Soil.ToolAimInfluence = .22f;
	Soil.SecondaryMotion = 0.85f;

	auto& Harvest = ActionProfiles.Add(EAction::Harvest);
	Harvest.Duration = .27f;
	Harvest.ReachTowardTarget = 12.f;
	Harvest.ObjectTilt = FRotator(-23, 0, 8);
	Harvest.ToolAimInfluence = .35f;

	auto& Cut = ActionProfiles.Add(EAction::CutTape);
	Cut.BlendIn = .045f;
	Cut.ReachTowardTarget = 6.f;
	Cut.LocalHandOffset.Z = 4.f;
	Cut.TraceTravel = 8.f;
	Cut.ObjectTilt = FRotator(-9, 0, 0);
	Cut.ToolAimInfluence = .5f;

	auto& Whip = ActionProfiles.Add(EAction::Whip);
	Whip.Duration = .26f;
	Whip.BlendIn = .045f;
	Whip.LocalHandOffset = FVector(5, 4, 7);
	Whip.ReachTowardTarget = 8.f;
	Whip.ObjectTilt = FRotator(17, -23, -31);
	Whip.ToolAimInfluence = .22f;

	auto& Scan = ActionProfiles.Add(EAction::Scan);
	Scan.ToolAimInfluence = .85f;
	Scan.LocalHandOffset = FVector(4, 0, 2);
	auto& Confirm = ActionProfiles.Add(EAction::ScanConfirm);
	Confirm.Duration = .20f;
	Confirm.LocalHandOffset = FVector(2, 0, 1.5);
	Confirm.ToolAimInfluence = .85f;
	Confirm.ObjectTilt = FRotator(4, 0, -3);

	auto& Book = ActionProfiles.Add(EAction::ReadBook);
	Book.LookInfluence = 1.f;
	Book.ElbowOffset = FVector(0, 0, -2);

	auto& Collect = ActionProfiles.Add(EAction::CollectCoins);
	Collect.ReachTowardTarget = 13.f;
	Collect.LocalWristRotation = FRotator(-15, 0, 12);
	auto& Receive = ActionProfiles.Add(EAction::ReceiveCoins);
	Receive.Duration = .33f;
	Receive.ReachTowardTarget = 11.f;
	Receive.LocalWristRotation = FRotator(11, 0, -14);
}

FBotanicusRobotToolMotion UBotanicusRobotAnimInstance::UpdateRobotAction(
	const ABotanicusCharacter& Owner, UBotanicusHeldItemComponent* Grip, bool bBox, float DeltaSeconds)
{
	const AActor* Item = Grip ? Grip->GetOwner() : nullptr;
	bApplyRobotActionHands = false;
	const double Now = Owner.GetRobotActionTime();
	FBotanicusRobotActionCue Wanted;
	const float ComputerRate = FMath::Max(.01f, ButtonPressPlayRate);
	const float ComputerLength = ButtonPressAnimation ? ButtonPressAnimation->GetPlayLength() : 0.f;
	const float ComputerEnd = ButtonPressEndTime > 0.f ? FMath::Min(ComputerLength, ButtonPressEndTime) : ComputerLength;
	const float ComputerStart = FMath::Clamp(ButtonPressStartTime, 0.f, FMath::Max(0.f, ComputerEnd - .05f));
	const float ComputerSequenceRate = ButtonPressAnimation ? ComputerRate * FMath::Max(.01f, ButtonPressAnimation->RateScale) : ComputerRate;
	const float ComputerDuration = (ComputerEnd - ComputerStart) / ComputerSequenceRate;
	if (!bBox && !Owner.IsCarryingEquipment() && !Owner.IsLocallyControlled())
	{
		const FBotanicusRobotActionCue& Pulse = Owner.GetPendingRobotAction();
		const auto* PulseProfile = ActionProfiles.Find(Pulse.Action);
		const FName Selected = Owner.GetRobotActionItemKey();
		const bool bSelected = Pulse.ItemKey.IsNone() || Pulse.ItemKey == Selected ||
			(Pulse.Action == EAction::Plant && Selected.IsNone());
		const bool bPress = Pulse.Action == EAction::PressComputer;
		if (PulseProfile && Now - Pulse.StartedAt < (bPress ? ComputerDuration : PulseProfile->Duration) &&
			bSelected && MatchesAction(Pulse.Action, Item) && (bPress || !Owner.IsUsingOrderComputer()))
			Wanted = Pulse;

		if (Wanted.Action == EAction::None && !Owner.IsUsingOrderComputer())
		{
			for (const EAction Type : {EAction::Scan, EAction::CutTape, EAction::PourSoil, EAction::Water})
			{
				if (MatchesAction(Type, Item))
				{
					Wanted = Owner.GetRobotActionState(Type);
					if (Wanted.Action != EAction::None)
						break;
				}
			}
		}
		if (Wanted.Action == EAction::None && !Owner.IsUsingOrderComputer())
		{
			if (const auto* Book = Cast<ABotanicusBotanistBookActor>(Item); Book && Book->IsReadingPose())
				Wanted = {EAction::ReadBook, Book, Book->GetReadingLookTarget(), 0.0, {}, true, true};
		}
	}

	const bool bActive = Wanted.Action != EAction::None && ActionProfiles.Contains(Wanted.Action);
	const bool bChanged = bActive && (Wanted.Action != ActiveRobotCue.Action ||
		Wanted.StartedAt != ActiveRobotCue.StartedAt || Wanted.Source != ActiveRobotCue.Source);
	if (bChanged)
	{
		const bool bRestartPress = Wanted.Action == EAction::PressComputer && ActiveRobotCue.Action == EAction::PressComputer &&
			InteractionMontage.IsValid() && Montage_IsPlaying(InteractionMontage.Get());
		if (!bRestartPress)
		{
			if (InteractionMontage.IsValid())
				Montage_Stop(.08f, InteractionMontage.Get());
			InteractionMontage.Reset();
		}
		if (Wanted.Action == EAction::PressComputer && ButtonPressAnimation)
		{
			const auto& Press = ActionProfiles.FindChecked(Wanted.Action);
			const float Start = ComputerStart + FMath::Max(0.f, static_cast<float>(Now - Wanted.StartedAt)) * ComputerSequenceRate;
			if (bRestartPress)
				Montage_SetPosition(InteractionMontage.Get(), Start);
			else
				InteractionMontage = PlaySlotAnimationAsDynamicMontage(ButtonPressAnimation, TEXT("DefaultSlot"),
					Press.BlendIn, Press.BlendOut, ComputerRate, 1, -1.f, Start);
		}
		else if (auto* NewMontage = ActionProfiles.FindChecked(Wanted.Action).Montage.Get())
		{
			const float StartOffset = Wanted.bContinuous ? 0.f : FMath::Max(0.f, static_cast<float>(Now - Wanted.StartedAt));
			if (Montage_Play(NewMontage, 1.f, EMontagePlayReturnType::MontageLength, StartOffset, false) > 0.0f)
				InteractionMontage = NewMontage;
		}
		if (Wanted.Action != ActiveRobotCue.Action)
			RobotActionAlpha = 0.f;
		RobotActionMount = Grip;
	}
	if (bActive)
		ActiveRobotCue = Wanted;

	const auto* Profile = ActionProfiles.Find(ActiveRobotCue.Action);
	if (!Profile)
		return {};

	if (!bActive && InteractionMontage.IsValid())
	{
		Montage_Stop(Profile->BlendOut, InteractionMontage.Get());
		InteractionMontage.Reset();
	}

	if (bActive && !ActiveRobotCue.bContinuous)
	{
		const float Age = FMath::Max(0.f, static_cast<float>(Now - ActiveRobotCue.StartedAt));
		const float In = FMath::Clamp(Age / FMath::Max(.01f, Profile->BlendIn), 0.f, 1.f);
		const float Duration = ActiveRobotCue.Action == EAction::PressComputer ? ComputerDuration : Profile->Duration;
		const float Out = FMath::Clamp((Duration - Age) / FMath::Max(.01f, Profile->BlendOut), 0.f, 1.f);
		RobotActionAlpha = Profile->MotionCurve
			? FMath::Clamp(Profile->MotionCurve->GetFloatValue(Age / FMath::Max(.01f, Duration)), 0.f, 1.f)
			: FMath::SmoothStep(0.f, 1.f, FMath::Min(In, Out));
	}
	else
	{
		const float Blend = bActive ? Profile->BlendIn : Profile->BlendOut;
		RobotActionAlpha = FMath::FInterpConstantTo(RobotActionAlpha, bActive ? 1.f : 0.f, DeltaSeconds,
			1.f / FMath::Max(.01f, Blend));
	}

	RobotAction = bActive || RobotActionAlpha > 0.f ? ActiveRobotCue.Action : EAction::None;
	if (RobotAction == EAction::None || bBox || (Grip != RobotActionMount.Get() && !bActive))
		return {};
	bApplyRobotActionHands = true;

	const FQuat Basis = Owner.GetMesh()->GetComponentQuat() * Owner.GetBaseRotationOffset().Inverse();
	const FVector Origin = Owner.GetMesh()->GetComponentLocation() - Basis.RotateVector(Owner.GetBaseTranslationOffset());
	RobotActionDirection = ActiveRobotCue.bHasTarget
		? (ActiveRobotCue.Target - Origin - FVector(0, 0, 14)).GetSafeNormal() : Owner.GetBaseAimRotation().Vector();
	if (!Grip || IsCoinAction(RobotAction) || RobotAction == EAction::ReadBook
		|| (Profile->bUseMontageInsteadOfMotion && InteractionMontage.IsValid()))
		return {};

	const FVector LocalAim = Basis.UnrotateVector(RobotActionDirection);
	const FRotator Aim = LocalAim.Rotation();
	const float BasePitch = (RobotAction == EAction::Scan || RobotAction == EAction::ScanConfirm)
		? FRotator::NormalizeAxis(Owner.GetBaseAimRotation().Pitch) : 0.f;
	FRotator Tilt = Profile->ObjectTilt + FRotator(FMath::Clamp(Aim.Pitch - BasePitch, -40.0, 40.0),
		FMath::Clamp(Aim.Yaw, -32.0, 32.0), 0.0) * Profile->ToolAimInfluence;
	FVector Offset = Profile->LocalHandOffset + LocalAim * Profile->ReachTowardTarget;
	Offset += Basis.UnrotateVector(ActiveRobotCue.TracePosition) * Profile->TraceTravel;
	const float Motion = FMath::Sin(static_cast<float>(Now - ActiveRobotCue.StartedAt) * 16.f) * Profile->SecondaryMotion;
	Offset.Z += Motion;
	return {Offset * RobotActionAlpha, Tilt * RobotActionAlpha,
		RobotAction == EAction::Water ? RobotActionAlpha : 0.f};
}

void UBotanicusRobotAnimInstance::ApplyRobotActionHands(const ABotanicusCharacter& Owner, FBotanicusHeldItemPose& Pose) const
{
	const auto* Profile = ActionProfiles.Find(RobotAction);
	if (!Profile || !bApplyRobotActionHands || RobotActionAlpha <= 0.f)
		return;

	if (RobotAction == EAction::PressComputer)
	{
		const auto* Mesh = GetSkelMeshComponent();
		const FVector Shoulder = Mesh->GetSocketLocation(TEXT("r-arm"));
		const FVector Elbow = Mesh->GetSocketLocation(TEXT("r-forearm"));
		const FTransform Hand = Mesh->GetSocketTransform(TEXT("r-hand"));
		const FVector Travel = ActiveRobotCue.Target - Shoulder;
		const double Reach = (FVector::Distance(Shoulder, Elbow) + FVector::Distance(Elbow, Hand.GetLocation())) * .91;
		const double TowardScreen = FMath::Clamp(FVector::DotProduct(Travel.GetSafeNormal(), Owner.GetActorForwardVector()), 0.0, 1.0);
		const float ClipTime = InteractionMontage.IsValid() ? Montage_GetPosition(InteractionMontage.Get()) : 0.f;
		// The hand extends from .65s, reaches the button at 1.15s and retracts after 1.8s in the source clip.
		const float Contact = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((ClipTime - .65f) / .50f, 0.f, 1.f)) *
			FMath::SmoothStep(0.f, 1.f, FMath::Clamp((2.34f - ClipTime) / .54f, 0.f, 1.f));
		Pose.Right = FTransform(Hand.GetRotation(), Shoulder + Travel.GetClampedToMaxSize(Reach));
		Pose.RightInfluence = RobotActionAlpha * Contact * Profile->IKInfluence * TowardScreen;
		Pose.RightLayerInfluence = RobotActionAlpha;
		Pose.RightElbowOffset = Profile->ElbowOffset;
		return;
	}

	const bool bCoin = IsCoinAction(RobotAction);
	if ((RobotAction == EAction::Plant && !Pose.IsSet()) || bCoin)
	{
		const bool bUseLeft = bCoin && Pose.Right.IsSet() && !Pose.Left.IsSet();
		TOptional<FTransform>& Hand = bUseLeft ? Pose.Left : Pose.Right;
		if (!Hand.IsSet())
		{
			const FQuat Basis = Owner.GetMesh()->GetComponentQuat() * Owner.GetBaseRotationOffset().Inverse();
			const FVector Origin = Owner.GetMesh()->GetComponentLocation() - Basis.RotateVector(Owner.GetBaseTranslationOffset());
			const double Side = bUseLeft ? -1.0 : 1.0;
			FVector Offset(23.f, Side * 18.f, 1.f);
			Offset += (Profile->LocalHandOffset + Basis.UnrotateVector(RobotActionDirection) * Profile->ReachTowardTarget) * RobotActionAlpha;
			const FRotator Rest = bUseLeft ? FRotator(60.05, 46.32, -79.23) : FRotator(-56.43, 148.97, -58.95);
			Hand = FTransform(Basis * (Rest + Profile->LocalWristRotation * RobotActionAlpha).Quaternion(),
				Origin + Basis.RotateVector(Offset));
			(bUseLeft ? Pose.LeftInfluence : Pose.RightInfluence) = RobotActionAlpha * Profile->IKInfluence;
			(bUseLeft ? Pose.LeftLayerInfluence : Pose.RightLayerInfluence) = RobotActionAlpha;
			(bUseLeft ? Pose.LeftElbowOffset : Pose.RightElbowOffset) = Profile->ElbowOffset * RobotActionAlpha;
		}
		return;
	}

	const float Influence = FMath::Lerp(1.f, Profile->IKInfluence, RobotActionAlpha);
	const FQuat Wrist = (Profile->LocalWristRotation * RobotActionAlpha).Quaternion();
	if (Pose.Left.IsSet())
	{
		Pose.Left->SetRotation(Pose.Left->GetRotation() * Wrist);
		Pose.LeftInfluence *= Influence;
		Pose.LeftElbowOffset += Profile->ElbowOffset * RobotActionAlpha;
	}
	if (Pose.Right.IsSet())
	{
		Pose.Right->SetRotation(Pose.Right->GetRotation() * Wrist);
		Pose.RightInfluence *= Influence;
		Pose.RightElbowOffset += Profile->ElbowOffset * RobotActionAlpha;
	}
}

