// MIT License

// Copyright (c) 2026 Nekki Limited.
// Copyright (c) 2022 Autodesk, Inc.

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "CascLiveLinkAnimSequenceHelper.h"

#include "CascLiveLinkUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/Skeleton.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/StructuredLog.h"

#include "Misc/FeedbackContext.h"

#include "Roles/CascLiveLinkTimelineTypes.h"

#include "AssetToolsModule.h"
#include "CoreGlobals.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "CascLiveLinkAnimSequenceHelper"

namespace
{
	float GetAnimSequenceLength(const UAnimSequence& AnimSequence)
	{
		return AnimSequence.GetPlayLength();
	}

	int32 GetAnimSequenceNumberOfFrames(const UAnimSequence& AnimSequence)
	{
		return AnimSequence.GetDataModel()->GetNumberOfFrames();
	}

	void RandomizeVector3Array(TArray<FVector3f>& VecArray)
	{
		for (auto& k : VecArray)
		{
			k.X = FMath::FRand();
			k.Y = FMath::FRand();
			k.Z = FMath::FRand();
		}
	}

	void RandomizeQuaternionArray(TArray<FQuat4f>& QuatArray)
	{
		for (auto& k : QuatArray)
		{
			k.X = FMath::FRand();
			k.Y = FMath::FRand();
			k.Z = FMath::FRand();
			k.W = FMath::FRand();
		}
	}
}

UCascLiveLinkAnimSequenceHelper::UCascLiveLinkAnimSequenceHelper(const FObjectInitializer&)
{
}

void UCascLiveLinkAnimSequenceHelper::PushStaticDataToAnimSequence(const FCascLiveLinkAnimSequenceStaticData& StaticData,
																   TArray<FName>& BoneTrackRemapping,
																   FString& AnimSequenceName)
{
	if (StaticData.LinkedAssetPath.IsEmpty() ||
		StaticData.SequencePath.IsEmpty() ||
		StaticData.SequenceName.IsEmpty() ||
		StaticData.BoneNames.Num() != StaticData.BoneParents.Num())
	{
		return;
	}

	auto Skeleton = FCascLiveLinkUtils::FindAsset<USkeleton>(StaticData.LinkedAssetPath);
	if (!Skeleton)
	{
		FString LinkedPath, LinkedName, LinkedExtension;
		FPaths::Split(StaticData.LinkedAssetPath, LinkedPath, LinkedName, LinkedExtension);
		Skeleton = FCascLiveLinkUtils::FindAssetInRegistry<USkeleton>(LinkedPath, LinkedName);

		if (!Skeleton)
		{
			UE_LOG(LogCascLiveLink, Error, TEXT("Could not find Skeleton %s"), *StaticData.LinkedAssetPath);
			return;
		}
	}

	auto AnimSequence = FCascLiveLinkUtils::FindAsset<UAnimSequence>(FPaths::Combine(StaticData.SequencePath,
																					 StaticData.SequenceName),
																	 StaticData.SequenceName);
	if (!AnimSequence)
	{
 		AnimSequence = FCascLiveLinkUtils::FindAssetInRegistry<UAnimSequence>(StaticData.SequencePath,
																			  StaticData.SequenceName);
	}
	if (!AnimSequence)
	{
		UE_LOGFMT(LogCascLiveLink, Display, "AnimSequence {0} not found, creating new.", FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName));
		if (FAssetToolsModule::IsModuleLoaded())
		{
			auto& AssetTools = FAssetToolsModule::GetModule().Get();
			AnimSequence = Cast<UAnimSequence>(AssetTools.CreateAsset(StaticData.SequenceName, StaticData.SequencePath, UAnimSequence::StaticClass(), nullptr));
		}
		
		if (!AnimSequence)
		{
			UE_LOGFMT(LogCascLiveLink, Error,
				   "Could not find or create AnimSequence {0} located at {1}",
				   StaticData.SequenceName,
				   StaticData.SequencePath);
			return;
		}
	}
	else
	{
		UE_LOGFMT(LogCascLiveLink, Display, "AnimSequence {0} already found, updating it.", FPaths::Combine(StaticData.SequencePath, StaticData.SequenceName));
	}

	if (AnimSequence)
	{
		{
			UE::Anim::Compression::FScopedCompressionGuard CompressionGuard(AnimSequence);

			const auto NumberOfFrames = StaticData.EndFrame - StaticData.StartFrame;

			StaticUpdateAnimSequence(*AnimSequence,
				Skeleton,
				ComputeAnimSequenceLength(NumberOfFrames,
					StaticData.FrameRate.AsDecimal()),
				NumberOfFrames,
				StaticData.FrameRate);

			{
				auto& Controller = AnimSequence->GetController();
				IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("AddNewRawTrack_Bracket", "Adding new Bone Animation Track"), false);

				BoneTrackRemapping.Empty();
				BoneTrackRemapping.Reserve(StaticData.BoneNames.Num());
				auto& RefSkeleton = Skeleton->GetReferenceSkeleton();

				for (auto& BoneName : StaticData.BoneNames)
				{
					if (RefSkeleton.FindBoneIndex(BoneName) != INDEX_NONE)
					{
						bool bValidBone = AnimSequence->GetDataModel()->IsValidBoneTrackName(BoneName);

						if (!bValidBone)
						{
							FRawAnimSequenceTrack RawTrack;
							RawTrack.PosKeys.Init(FVector3f::ZeroVector, NumberOfFrames);
							RawTrack.RotKeys.Init(FQuat4f::Identity, NumberOfFrames);
							RawTrack.ScaleKeys.Init(FVector3f::OneVector, NumberOfFrames);

							bValidBone = Controller.AddBoneCurve(BoneName, false);

							if (bValidBone)
							{
								Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, false);
							}
							else
							{
								continue;
							}
						}

						FRawAnimSequenceTrack RawTrack;
						bool ResizeSequenceRequested = false;
						if (NumberOfFrames != RawTrack.PosKeys.Num())
						{
							ResizeSequenceRequested = true;
							RawTrack.PosKeys.Init(FVector3f::ZeroVector, NumberOfFrames);
							RandomizeVector3Array(RawTrack.PosKeys);
						}
						if (NumberOfFrames != RawTrack.RotKeys.Num())
						{
							ResizeSequenceRequested = true;
							RawTrack.RotKeys.Init(FQuat4f::Identity, NumberOfFrames);
							RandomizeQuaternionArray(RawTrack.RotKeys);
						}
						if (NumberOfFrames != RawTrack.ScaleKeys.Num())
						{
							ResizeSequenceRequested = true;
							RawTrack.ScaleKeys.Init(FVector3f::OneVector, NumberOfFrames);
							RandomizeVector3Array(RawTrack.ScaleKeys);
						}

						if (ResizeSequenceRequested)
						{
							Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, false);
						}
					}
					BoneTrackRemapping.Add(BoneName);
				}

				for (auto& PropertyName : StaticData.PropertyNames)
				{
					const FString& CurveName = PropertyName.ToString();

					FName CurveFName(*CurveName);

					FAnimationCurveIdentifier CurveId(CurveFName, ERawCurveTrackTypes::RCT_Float);
					TArray<FRichCurveKey> RichCurveKeys;

					const FAnimCurveBase* RichCurve = Controller.GetModel()->FindCurve(CurveId);
					if (!RichCurve)
						Controller.AddCurve(CurveId, EAnimAssetCurveFlags::AACF_Editable, false);

					FRichCurveKey defaultCurveKey(0.0f, 0.0f);
					RichCurveKeys.Init(defaultCurveKey, NumberOfFrames);

					const FFrameRate& FrameRate = AnimSequence->GetDataModel()->GetFrameRate();
					const double Interval = FrameRate.AsInterval();

					for (int i = 0; i < RichCurveKeys.Num(); ++i)
					{
						auto& Key = RichCurveKeys[i];
						Key.Time = static_cast<float>(i) * Interval;
					}

					Controller.SetCurveKeys(CurveId, RichCurveKeys, false);
				}
			}
		}
		AnimSequence->CacheDerivedDataForCurrentPlatform();
		AnimSequenceName = AnimSequence->GetName();
	}
	
}

void UCascLiveLinkAnimSequenceHelper::PushFrameDataToAnimSequence(const FCascLiveLinkAnimSequenceFrameData& FrameData,
																  const FCascLiveLinkAnimSequenceParams& TimelineParams)
{
	if (TimelineParams.SequencePath.IsEmpty() ||
		TimelineParams.SequenceName.IsEmpty() ||
		FrameData.Frames.IsEmpty())
	{
		return;
	}

	auto AnimSequence = FCascLiveLinkUtils::FindAsset<UAnimSequence>(FPaths::Combine(TimelineParams.SequencePath,
																					 TimelineParams.SequenceName),
																	 TimelineParams.SequenceName);
	if (!AnimSequence)
	{
		UE_LOGFMT(LogCascLiveLink, Warning,
			"Could not find or create AnimSequence {0} located at {1}",
			TimelineParams.SequenceName,
			TimelineParams.SequencePath);
		return;

	}

	bool SequenceUpdated = false;
	TMap<FName, FCascLiveLinkAnimSequenceFrame> FramesByBone;
	
	if (const int32 NumberOfFrames = GetAnimSequenceNumberOfFrames(*AnimSequence);
		NumberOfFrames != FrameData.Frames.Num() - 1 && FrameData.StartFrame == 0) {
		AnimSequence->GetController().SetNumberOfFrames(FrameData.Frames.Num());
		StaticUpdateAnimSequence(*AnimSequence,nullptr, GetAnimSequenceLength(*AnimSequence), FrameData.Frames.Num(), AnimSequence->GetSamplingFrameRate());
	}

	const int32 NumberOfFrames = GetAnimSequenceNumberOfFrames(*AnimSequence);

	for (int FrameIndex = 0; FrameIndex < FrameData.Frames.Num(); ++FrameIndex)
	{
		const auto& Frame = FrameData.Frames[FrameIndex];

		auto BoneArraySize = Frame.Locations.Num();
		for (int32 BoneIndex = 0; BoneIndex < BoneArraySize; ++BoneIndex)
		{
			if (BoneIndex >= TimelineParams.BoneTrackRemapping.Num())
			{
				continue;
			}
			const FName& TrackName = TimelineParams.BoneTrackRemapping[BoneIndex];

			if (!TrackName.IsValid())
			{
				continue;
			}

			if (NumberOfFrames > 0 && FrameIndex <= NumberOfFrames)
			{
				auto BoneTrackPtr = FramesByBone.Find(TrackName);
				if (!BoneTrackPtr)
				{
					BoneTrackPtr = &FramesByBone.Emplace(TrackName);
					BoneTrackPtr->Locations.Init(FVector::ZeroVector, FrameData.Frames.Num());
					BoneTrackPtr->Rotations.Init(FQuat::Identity, FrameData.Frames.Num());
					BoneTrackPtr->Scales.Init(FVector::OneVector, FrameData.Frames.Num());
				}
				FCascLiveLinkAnimSequenceFrame& BoneTrack = *BoneTrackPtr;
				BoneTrack.Locations[FrameIndex] = Frame.Locations[BoneIndex];
				BoneTrack.Rotations[FrameIndex] = Frame.Rotations[BoneIndex];
				BoneTrack.Scales[FrameIndex] = Frame.Scales[BoneIndex];
				SequenceUpdated = true;
			}
		}
	}

	if (SequenceUpdated)
	{
		{
			UE::Anim::Compression::FScopedCompressionGuard CompressionGuard(AnimSequence);

			auto& Controller = AnimSequence->GetController();
			IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("SetBoneTrackKeys_Bracket", "Setting Bone Animation Tracks"), false);

			for (const auto& [BoneName, BoneData] : FramesByBone)
			{
				auto BoneUpdated = Controller.UpdateBoneTrackKeys(BoneName,
					FInt32Range(FInt32Range::BoundsType::Inclusive(FrameData.StartFrame), FInt32Range::BoundsType::Inclusive(FrameData.StartFrame + BoneData.Locations.Num() - 1)),
					BoneData.Locations,
					BoneData.Rotations,
					BoneData.Scales,
					false);
				if (!BoneUpdated) {
					UE_LOGFMT(LogCascLiveLink, Warning,
						"Could not update bone track AnimSequence {0} located at {1}",
						TimelineParams.SequenceName,
						TimelineParams.SequencePath);
				}
			}
		}
		AnimSequence->CacheDerivedDataForCurrentPlatform();
	}
	FCascLiveLinkUtils::RefreshContentBrowser(*AnimSequence);
}

bool UCascLiveLinkAnimSequenceHelper::StaticUpdateAnimSequence(UAnimSequence& AnimSequence,
															   USkeleton* Skeleton,
															   float SequenceLength,
															   int32 NumberOfFrames,
															   const FFrameRate& FrameRate)
{
	bool Updated = false;
	
	if (Skeleton &&
		AnimSequence.GetSkeleton() != Skeleton)
	{
		AnimSequence.SetSkeleton(Skeleton);
		Updated = true;

		AnimSequence.GetController().InitializeModel();
	}

	const float AnimSequenceLength = GetAnimSequenceLength(AnimSequence);
	const int32 AnimSequenceNumberOfFrames = GetAnimSequenceNumberOfFrames(AnimSequence);
	UE_LOGFMT(LogCascLiveLink, Display, "AnimSequenceLength {0}, AnimSequenceNumberOfFrames {1}", AnimSequenceLength, AnimSequenceNumberOfFrames);

	if ((SequenceLength > 0.0f &&
		 !FMath::IsNearlyEqual(AnimSequenceLength, SequenceLength, UE_KINDA_SMALL_NUMBER)) ||
		(NumberOfFrames > 0 &&
		 AnimSequenceNumberOfFrames != NumberOfFrames))
	{
		auto& Controller = AnimSequence.GetController();
		IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("SetNumberOfFrames_Bracket", "Setting Animation Sequence Info"), false);

		Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, false);
		Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, false);
		Controller.SetFrameRate(FrameRate);
		Controller.SetNumberOfFrames(NumberOfFrames);
		
		AnimSequence.ImportResampleFramerate = FrameRate.AsDecimal();
		AnimSequence.ImportFileFramerate = FrameRate.AsDecimal();
		Controller.NotifyPopulated();

		Updated = true;
	}

	return Updated;
}

#undef LOCTEXT_NAMESPACE