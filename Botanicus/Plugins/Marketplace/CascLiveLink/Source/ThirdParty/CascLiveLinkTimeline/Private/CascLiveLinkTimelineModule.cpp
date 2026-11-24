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

#include "CascLiveLinkTimelineModule.h"

#include "AnimPreviewInstance.h"
#include "IAnimationEditorModule.h"
#include "IPersonaPreviewScene.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "CascLiveLinkUtils.h"
#include "PersonaModule.h"

#include "Animation/DebugSkelMeshComponent.h"

#include "Async/Async.h"

#include "UObject/WeakObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/ObjectMacros.h"

IMPLEMENT_MODULE(FCascLiveLinkTimelineModule, CascLiveLinkTimeline)

#define LOCTEXT_NAMESPACE "FCascLiveLinkTimelineModule"

namespace
{
	inline FFrameNumber AsFrameNumber(double TimeInSeconds, const FFrameRate& FrameRate)
	{
		const double TimeAsFrame = (TimeInSeconds * FrameRate.Numerator) / FrameRate.Denominator + KINDA_SMALL_NUMBER;
		return static_cast<int32>(FMath::FloorToDouble(TimeAsFrame));
	}

	int32 GetNumberOfFrames(UAnimSequenceBase& AnimSequenceBase)
	{
		return AnimSequenceBase.GetNumberOfSampledKeys();
	}
}

void FCascLiveLinkTimelineModule::StartupModule()
{
	bAnimSequenceEditorTime = false;
	bLevelSequenceEditorTimeSync = false;
	bSetGlobalTime = false;
	bIgnoreTimeChange = false;
	bBlockTimeChangeFeedback = true;
	LastFrameTime.Time.FrameNumber.Value = 0;

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
	OnSequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateRaw(this, &FCascLiveLinkTimelineModule::OnSequencerCreated));

	FPersonaModule& PersonaModule = FModuleManager::Get().LoadModuleChecked<FPersonaModule>(TEXT("Persona"));
	OnPreviewSceneCreatedHandle = PersonaModule.OnPreviewSceneCreated().AddRaw(this, &FCascLiveLinkTimelineModule::OnAnimSequenceEditorPreviewSceneCreated);
}

void FCascLiveLinkTimelineModule::ShutdownModule()
{
	if (OnSequencerCreatedHandle.IsValid())
	{
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>(TEXT("Sequencer"));
		if (SequencerModule)
		{
			SequencerModule->UnregisterOnSequencerCreated(OnSequencerCreatedHandle);
		}
	}
	WeakSequencer = nullptr;

	if (OnPreviewSceneCreatedHandle.IsValid())
	{
		FPersonaModule* PersonaModule = FModuleManager::GetModulePtr<FPersonaModule>(TEXT("Persona"));
		if (PersonaModule)
		{
			PersonaModule->OnPreviewSceneCreated().Remove(OnPreviewSceneCreatedHandle);
		}
	}
	WeakPreviewScene.Reset();
}

void FCascLiveLinkTimelineModule::OnSequencerCreated(TSharedRef<ISequencer> Sequencer)
{
	if (WeakSequencer.IsValid())
	{
		auto OldSequencer = WeakSequencer.Pin();
		if (OldSequencer)
		{
			UnregisterSequencer(OldSequencer.ToSharedRef());
		}
	}

	bLevelSequenceEditorTimeSync = true;
	bSetGlobalTime = false;
	WeakSequencer = TWeakPtr<ISequencer>(Sequencer);

	OnSequencerClosedHandle = Sequencer->OnCloseEvent().AddRaw(this, &FCascLiveLinkTimelineModule::OnSequencerClosed);

	OnSequencerGlobalTimeChangedHandle = Sequencer->OnGlobalTimeChanged().AddRaw(this, &FCascLiveLinkTimelineModule::OnSequencerTimeChanged, WeakSequencer);

	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		if (auto Sequencer = WeakSequencer.Pin())
		{
			auto LocalTime = Sequencer->GetLocalTime();
			bSetGlobalTime = true;
			Sequencer->SetLocalTimeDirectly(LocalTime.Time + 1);
			bSetGlobalTime = true;
			Sequencer->SetLocalTimeDirectly(LocalTime.Time);
		}
	});
}

void FCascLiveLinkTimelineModule::OnSequencerClosed(TSharedRef<ISequencer> Sequencer)
{
	UnregisterSequencer(Sequencer);
}

void FCascLiveLinkTimelineModule::UnregisterSequencer(TSharedRef<ISequencer> Sequencer)
{
	Sequencer->OnCloseEvent().Remove(OnSequencerClosedHandle);
	OnSequencerClosedHandle.Reset();
	Sequencer->OnGlobalTimeChanged().Remove(OnSequencerGlobalTimeChangedHandle);
	OnSequencerGlobalTimeChangedHandle.Reset();

	WeakSequencer = nullptr;
}

void FCascLiveLinkTimelineModule::OnSequencerTimeChanged(TWeakPtr<ISequencer> InSequencer)
{
	if (bSetGlobalTime)
	{
		bSetGlobalTime = false;
		return;
	}

	if (bLevelSequenceEditorTimeSync && InSequencer.IsValid())
	{
		auto Sequencer = InSequencer.Pin();
		auto NewFrameTime = Sequencer->GetGlobalTime();
		FFrameTime SnappedFrameTime = FFrameRate::Snap(NewFrameTime.Time, NewFrameTime.Rate, NewFrameTime.Rate);
		FFrameNumber NewFrameNumber = AsFrameNumber(SnappedFrameTime.AsDecimal(), NewFrameTime.Rate);
		FFrameNumber LastFrameNumber = AsFrameNumber(LastFrameTime.Time.AsDecimal(), LastFrameTime.Rate);
		if (NewFrameNumber != LastFrameNumber)
		{
			LastFrameTime = FQualifiedFrameTime(SnappedFrameTime, NewFrameTime.Rate);
			bIgnoreTimeChange = true;

			OnTimeChangedDelegate.Broadcast(LastFrameTime);
		}
	}
}

void FCascLiveLinkTimelineModule::OnAnimSequenceEditorPreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	bAnimSequenceEditorTime = false;
	LastFrameTime.Time.FrameNumber = 0;
	WeakPreviewScene = TWeakPtr<IPersonaPreviewScene>(InPreviewScene);

	InPreviewScene->RegisterOnInvalidateViews(FSimpleDelegate::CreateRaw(this, &FCascLiveLinkTimelineModule::HandleInvalidateViews));
}

void FCascLiveLinkTimelineModule::OpenAnimEditorWindow(const FString& Path, const FString& Name) const
{
#if WITH_EDITOR
	auto AnimSequence = FCascLiveLinkUtils::FindAsset<UAnimSequence>(FPaths::Combine(Path, Name), Name);
	if (!AnimSequence)
	{
		AnimSequence = FCascLiveLinkUtils::FindAssetInRegistry<UAnimSequence>(Path, Name);
	}

	if (AnimSequence)
	{
		IAnimationEditorModule& AnimationEditorModule = FModuleManager::LoadModuleChecked<IAnimationEditorModule>("AnimationEditor");
		AnimationEditorModule.CreateAnimationEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), AnimSequence);
	}
#endif // WITH_EDITOR
}

bool FCascLiveLinkTimelineModule::CloseAnimEditorWindow(const FString& Path, const FString& Name) const
{
#if WITH_EDITOR
	TSharedPtr<IPersonaPreviewScene> PreviewScene = WeakPreviewScene.Pin();
	if (!PreviewScene)
	{
		return false;
	}

	UAnimationAsset* AnimAsset = PreviewScene->GetPreviewAnimationAsset();
	if (!AnimAsset)
	{
		return false;
	}

	if (FPaths::Combine(Path, Name) == FPaths::GetBaseFilename(AnimAsset->GetPathName(), false))
	{
		auto AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(AnimAsset);
		for (IAssetEditorInstance* ExistingEditor : AssetEditors)
		{
			if (ExistingEditor->GetEditorName() == FName("AnimationEditor"))
			{
				// Change the current anim to this one
				IAnimationEditor* AnimEditor = static_cast<IAnimationEditor*>(ExistingEditor);
				AnimEditor->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed);
				return true;
			}
		}
	}
#endif // WITH_EDITOR

	return false;
}

void FCascLiveLinkTimelineModule::HandleInvalidateViews()
{
	if (!bAnimSequenceEditorTime || !WeakPreviewScene.IsValid() || bBlockTimeChangeFeedback)
	{
		return;
	}

	TSharedPtr<IPersonaPreviewScene> PreviewScene = WeakPreviewScene.Pin();
	if (!PreviewScene.IsValid())
	{
		return;
	}

	if (UDebugSkelMeshComponent* PreviewMeshComp = PreviewScene->GetPreviewMeshComponent())
	{
		if (UAnimPreviewInstance* PreviewInstance = PreviewMeshComp->PreviewInstance)
		{
			FQualifiedFrameTime LastFrameTimeCopy = LastFrameTime;
			if (bIgnoreTimeChange)
			{
				bIgnoreTimeChange = false;
				SetAnimSequenceEditorTime(LastFrameTimeCopy);
			}

			if (UAnimationAsset* AnimationAsset = PreviewInstance->GetCurrentAsset())
			{
				if (UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(AnimationAsset))
				{
					double TimeOffset = 0.0;
					if (auto StartTime = AnimSequenceStartFrames.Find(Sequence->GetName()))
					{
						TimeOffset = *StartTime;
					}

					double PreviewCurrentTime = PreviewInstance->GetCurrentTime();
					double CurrentTime = PreviewCurrentTime + LastFrameTimeCopy.Rate.AsSeconds(FFrameTime(static_cast<int32>(TimeOffset)));
					double PlayLength = PreviewInstance->GetLength();
					const double LastFrameTimeInSeconds = LastFrameTimeCopy.AsSeconds() - LastFrameTimeCopy.Rate.AsSeconds(FFrameTime(static_cast<int32>(TimeOffset)));
					if (LastFrameTimeCopy.Time.FrameNumber == AsFrameNumber(CurrentTime, LastFrameTimeCopy.Rate) ||
						LastFrameTimeInSeconds < 0 ||
						LastFrameTimeInSeconds > PlayLength)
					{
						return;
					}
					else if (PlayLength > 0.0f)
					{
						int32 Denominator = FMath::CeilToInt(PlayLength);
						int32 Numerator = static_cast<int32>((GetNumberOfFrames(*Sequence) - 1) * Denominator / static_cast<double>(PlayLength) + 0.5);
						if (Numerator > 0 && Denominator > 0)
						{
							FFrameRate FrameRate(Numerator, Denominator);
							FQualifiedFrameTime Time(AsFrameNumber(CurrentTime, FrameRate), FrameRate);
							const double TimeInSeconds = Time.AsSeconds();

							if (TimeInSeconds >= 0)
							{
								LastFrameTime = Time;

								OnTimeChangedDelegate.Broadcast(Time);

								SetSequencerTime(Time);
							}
						}
					}
				}
			}
		}
	}
}

void FCascLiveLinkTimelineModule::SetCurrentTime(const FQualifiedFrameTime& Time)
{
	bBlockTimeChangeFeedback = true;

	LastFrameTime = Time;

	if (bAnimSequenceEditorTime)
	{
		SetAnimSequenceEditorTime(Time);
	}
	if (bLevelSequenceEditorTimeSync)
	{
		SetSequencerTime(Time);
	}

	bBlockTimeChangeFeedback = false;
}

void FCascLiveLinkTimelineModule::SetAnimSequenceEditorTime(const FQualifiedFrameTime& Time,
																UAnimPreviewInstance* PreviewAnimInstance)
{
	if (!PreviewAnimInstance && WeakPreviewScene.IsValid())
	{
		TSharedPtr<IPersonaPreviewScene> PreviewScene = WeakPreviewScene.Pin();
		if (PreviewScene.IsValid())
		{
			if (auto PreviewMeshComp = PreviewScene->GetPreviewMeshComponent())
			{
				PreviewAnimInstance = PreviewMeshComp->PreviewInstance;
			}
		}
	}

	if (PreviewAnimInstance)
	{
		if (PreviewAnimInstance->IsPlaying())
		{
			PreviewAnimInstance->StopAnim();
		}

		double TimeOffset = 0.0;
		if (PreviewAnimInstance->GetAnimSequence())
		{
			if (auto StartTime = AnimSequenceStartFrames.Find(PreviewAnimInstance->GetAnimSequence()->GetName()))
			{
				TimeOffset = *StartTime;
			}
		}

		auto TimeInSeconds = Time.AsSeconds() - Time.Rate.AsSeconds(FFrameTime(static_cast<int32>(TimeOffset)));
		const auto PlayLength = PreviewAnimInstance->GetLength();
		if (TimeInSeconds >= 0.0f &&
			(TimeInSeconds < PlayLength || FMath::IsNearlyEqual(TimeInSeconds, static_cast<double>(PlayLength), 0.0001)))
		{
			PreviewAnimInstance->SetPosition(FMath::Min(TimeInSeconds, static_cast<double>(PlayLength)));
			TSharedPtr<IPersonaPreviewScene> PreviewScene = WeakPreviewScene.Pin();

			PreviewScene->RefreshAdditionalMeshes(false);
			PreviewScene->InvalidateViews();
		}
	}
}

void FCascLiveLinkTimelineModule::SetSequencerTime(const FQualifiedFrameTime& Time)
{
	if (WeakSequencer.IsValid())
	{
		auto Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			bSetGlobalTime = true;

			FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();
			FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
			Sequencer->SetGlobalTime(ConvertFrameTime(Time.Time.FrameNumber, Time.Rate, TickResolution));
		}
	}
}

void FCascLiveLinkTimelineModule::SetLastTime()
{
	SetCurrentTime(LastFrameTime);
}

void FCascLiveLinkTimelineModule::AddAnimSequenceStartFrame(const FString& Name, int32 StartTime)
{
	auto& StartFrame = AnimSequenceStartFrames.FindOrAdd(Name);
	StartFrame = StartTime;
}

void FCascLiveLinkTimelineModule::RemoveAnimSequenceStartFrame(const FString& Name)
{
	AnimSequenceStartFrames.Remove(Name);
}

#undef LOCTEXT_NAMESPACE
