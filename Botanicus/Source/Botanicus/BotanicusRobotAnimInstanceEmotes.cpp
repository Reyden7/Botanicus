#include "BotanicusRobotAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontageEvaluationState.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "BotanicusCharacter.h"
#include "Presentation/BotanicusEmoteMotion.h"

namespace
{
	class FBotanicusRobotAnimProxy final : public FAnimInstanceProxy
	{
	public:
		using FAnimInstanceProxy::FAnimInstanceProxy;

		TSet<TWeakObjectPtr<const UAnimSequence>> MotionSequences;

	protected:
		virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override
		{
			EvaluateAnimationNode_WithRoot(Output, InRootNode);
			if (InRootNode != GetRootNode() || !Output.Pose.IsValid() || MotionSequences.IsEmpty())
				return true;

			for (const FMontageEvaluationState& State : GetMontageEvaluationData())
			{
				const auto* Montage = State.Montage.Get();
				const float Weight = State.BlendInfo.GetBlendedValue();
				if (!Montage || Weight <= ZERO_ANIMWEIGHT_THRESH)
					continue;

				const FAnimTrack* Track = Montage->GetAnimationData(UBotanicusEmoteCatalog::FullBodySlot);
				const FAnimSegment* Segment = Track ? Track->GetSegmentAtTime(State.MontagePosition) : nullptr;
				float Position = 0.f;
				const auto* Sequence = Segment ? Cast<UAnimSequence>(Segment->GetAnimationData(State.MontagePosition, Position)) : nullptr;
				if (!Sequence || !MotionSequences.Contains(Sequence))
					continue;

				const FTransform First = Sequence->ExtractRootTrackTransform(FAnimExtractContext(), nullptr);
				const FTransform Authored = Sequence->ExtractRootTrackTransform(FAnimExtractContext(Position), nullptr);
				const FTransform Planar = BotanicusEmoteMotion::ProjectPlanar(First, Authored);
				const FQuat InverseYaw = Planar.GetRotation().Inverse();
				const FVector Offset = Authored.GetTranslation() - First.GetTranslation();
				const FVector VisualPosition = First.GetTranslation() + InverseYaw.RotateVector(Offset - Planar.GetTranslation());

				// The capsule already followed the travel and heading. Keep the root's
				// original lift and tilt in the pose, including during the slot's fade.
				FTransform& Root = Output.Pose[FCompactPoseBoneIndex(0)];
				Root.AddToTranslation((VisualPosition - Authored.GetTranslation()) * Weight);
				Root.SetRotation((FQuat::Slerp(FQuat::Identity, InverseYaw, Weight) * Root.GetRotation()).GetNormalized());
			}
			return true;
		}
	};
}

FAnimInstanceProxy* UBotanicusRobotAnimInstance::CreateAnimInstanceProxy()
{
	return new FBotanicusRobotAnimProxy(this);
}

void UBotanicusRobotAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete static_cast<FBotanicusRobotAnimProxy*>(Proxy);
}

void UBotanicusRobotAnimInstance::UpdateRobotEmoteMontage(const ABotanicusCharacter& Owner)
{
	const FBotanicusEmoteState& State = Owner.GetRobotEmoteState();
	const bool bActive = Owner.IsRobotEmotePlaying();
	if (!bActive || !EmoteRevision.IsSet() || *EmoteRevision != State.Revision)
	{
		if (EmoteMontage.IsValid())
			Montage_Stop(.15f, EmoteMontage.Get());
		EmoteMontage.Reset();
	}
	if (!bActive)
		return;

	auto* Catalog = Owner.GetRobotEmoteCatalog();
	const FBotanicusEmoteEntry* Entry = Catalog ? Catalog->Find(State.EmoteId) : nullptr;
	if (!Entry || !Entry->IsPlayable(Owner.GetRobotEmoteSkeleton()))
		return;

	const UAnimSequence* MotionSequence = Catalog->GetMotionSequence(*Entry);
	UAnimSequenceBase* Asset = Entry->Animation;
	const float Length = Asset->GetPlayLength();
	const double Elapsed = Owner.GetRobotEmoteElapsedTime();
	const double Position = Elapsed * Entry->GetEffectivePlayRate();
	if (!Entry->bLoop && Position >= Length && !MotionSequence)
		return;

	const float Offset = static_cast<float>(Entry->bLoop ? FMath::Fmod(Position, Length) :
		FMath::Min(Position, static_cast<double>(Length) - UE_KINDA_SMALL_NUMBER));
	if (EmoteRevision.IsSet() && *EmoteRevision == State.Revision)
	{
		if (MotionSequence && EmoteMontage.IsValid())
			Montage_SetPosition(EmoteMontage.Get(), Offset / Asset->RateScale);
		return;
	}
	EmoteRevision = State.Revision;

	const float Rate = FMath::Max(.05f, Entry->PlayRate);
	if (auto* Montage = Cast<UAnimMontage>(Asset))
	{
		FAlphaBlendArgs Blend;
		Blend.BlendTime = Entry->BlendInDuration;
		if (Montage_PlayWithBlendIn(Montage, Blend, Rate, EMontagePlayReturnType::MontageLength, Offset, false) > 0.f)
			EmoteMontage = Montage;
	}
	else
	{
		EmoteMontage = PlaySlotAnimationAsDynamicMontage(Asset, UBotanicusEmoteCatalog::FullBodySlot,
			Entry->BlendInDuration, Entry->BlendOutDuration, Rate, 1, -1.f, Offset / Asset->RateScale);
	}

	if (auto* Montage = EmoteMontage.Get())
	{
		if (MotionSequence)
		{
			GetProxyOnGameThread<FBotanicusRobotAnimProxy>().MotionSequences.Add(MotionSequence);
			// Sample the same timeline as CharacterMovement, including after a network correction.
			Montage_SetPlayRate(Montage, 0.f);
		}
		else
		{
			for (int32 Index = 0; Index < Montage->CompositeSections.Num(); ++Index)
			{
				const FName Section = Montage->CompositeSections[Index].SectionName;
				const FName Next = Index + 1 < Montage->CompositeSections.Num() ? Montage->CompositeSections[Index + 1].SectionName :
					(Entry->bLoop ? Montage->CompositeSections[0].SectionName : NAME_None);
				Montage_SetNextSection(Section, Next, Montage);
			}
		}
	}
}
