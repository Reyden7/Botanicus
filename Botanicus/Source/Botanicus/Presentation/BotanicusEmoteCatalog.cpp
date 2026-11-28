#include "Presentation/BotanicusEmoteCatalog.h"
#include "Presentation/BotanicusEmoteMotion.h"
#include "Animation/AnimSequence.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"

const FName UBotanicusEmoteCatalog::FullBodySlot(TEXT("FullBody_Emote"));

float FBotanicusEmoteEntry::GetEffectivePlayRate() const
{
	return FMath::Max(.05f, PlayRate) * (Animation ? FMath::Max(UE_SMALL_NUMBER, Animation->RateScale) : 1.f);
}

bool FBotanicusEmoteEntry::IsPlayable(const USkeleton* RobotSkeleton) const
{
	if (EmoteId.IsNone() || !Animation || !RobotSkeleton || Animation->GetSkeleton() != RobotSkeleton ||
		Animation->GetPlayLength() <= UE_SMALL_NUMBER || Animation->RateScale <= UE_SMALL_NUMBER)
		return false;

	if (const auto* Montage = Cast<UAnimMontage>(Animation))
		return Montage->SlotAnimTracks.Num() == 1 &&
			Montage->SlotAnimTracks[0].SlotName == UBotanicusEmoteCatalog::FullBodySlot;
	return Animation->CanBeUsedInComposition();
}

const FBotanicusEmoteEntry* UBotanicusEmoteCatalog::Find(FName EmoteId) const
{
	if (EmoteId.IsNone())
		return nullptr;
	return Emotes.FindByPredicate([EmoteId](const FBotanicusEmoteEntry& Entry)
	{
		return Entry.EmoteId == EmoteId;
	});
}

const UAnimSequence* UBotanicusEmoteCatalog::GetMotionSequence(const FBotanicusEmoteEntry& Entry)
{
	const auto* Sequence = Cast<UAnimSequence>(Entry.Animation);
	if (!Sequence || Sequence->bEnableRootMotion || Sequence->bForceRootLock)
		return nullptr;

	const bool* Cached = MovingSequences.Find(Sequence);
	if (Cached)
		return *Cached ? Sequence : nullptr;

	// Ignore the small root sway of gestures that keep their original stance.
	const FTransform First = Sequence->ExtractRootTrackTransform(FAnimExtractContext(), nullptr);
	const int32 Samples = FMath::Max(1, FMath::CeilToInt(Sequence->GetPlayLength() * 30.f));
	bool bMoves = false;
	for (int32 Index = 1; Index <= Samples && !bMoves; ++Index)
	{
		const double Time = Sequence->GetPlayLength() * Index / Samples;
		const FTransform Planar = BotanicusEmoteMotion::ProjectPlanar(First,
			Sequence->ExtractRootTrackTransform(FAnimExtractContext(Time), nullptr));
		bMoves = Planar.GetTranslation().SizeSquared2D() > FMath::Square(10.f) ||
			FMath::Abs(Planar.Rotator().Yaw) > 25.f;
	}
	MovingSequences.Add(Sequence, bMoves);
	return bMoves ? Sequence : nullptr;
}
