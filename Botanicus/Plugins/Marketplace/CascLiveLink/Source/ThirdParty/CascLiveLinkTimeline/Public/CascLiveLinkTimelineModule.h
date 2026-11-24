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

#pragma once

#include "CoreMinimal.h"

#include "Misc/QualifiedFrameTime.h"
#include "Modules/ModuleManager.h"


//Casc LiveLink timeline module
class FCascLiveLinkTimelineModule : public IModuleInterface
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimeChanged, const FQualifiedFrameTime& Time);
	
	//Called before the plugin is unloaded, right before the plugin object is destroyed.
	virtual void StartupModule() override;
	// Called before the plugin is unloaded, right before the plugin object is destroyed.
	virtual void ShutdownModule() override;

	static inline FCascLiveLinkTimelineModule& GetModule()
	{
		static const FName ModuleName = "CascLiveLinkTimeline";
		return FModuleManager::LoadModuleChecked<FCascLiveLinkTimelineModule>(ModuleName);
	}

	static inline bool IsModuleLoaded()
	{
		static const FName ModuleName = "CascLiveLinkTimeline";
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	// Open animation editor window
	CASCLIVELINKTIMELINE_API void OpenAnimEditorWindow(const FString& Path, const FString& Name) const;

	// Close animation editor window
	CASCLIVELINKTIMELINE_API bool CloseAnimEditorWindow(const FString& Path, const FString& Name) const;

	// Set current time
	CASCLIVELINKTIMELINE_API void SetCurrentTime(const FQualifiedFrameTime& Time);

	// Set last time
	CASCLIVELINKTIMELINE_API void SetLastTime();

	// Enable animation sequence editor time
	CASCLIVELINKTIMELINE_API void EnableAnimSequenceEditorTime(bool bEnable) { bAnimSequenceEditorTime = bEnable; }

	// Get  time changed delegate
	CASCLIVELINKTIMELINE_API FOnTimeChanged& GetOnTimeChangedDelegate() { return OnTimeChangedDelegate; }

	// Work with animation sequence start frame
	CASCLIVELINKTIMELINE_API void AddAnimSequenceStartFrame(const FString& Name, int32 StartTime);
	CASCLIVELINKTIMELINE_API void RemoveAnimSequenceStartFrame(const FString& Name);
	CASCLIVELINKTIMELINE_API void RemoveAllAnimSequenceStartFrames() { AnimSequenceStartFrames.Empty(); }

private:
	// Work create and close
	void OnSequencerCreated(TSharedRef<class ISequencer> Sequencer);
	void OnSequencerClosed(TSharedRef<class ISequencer> Sequencer);
	void UnregisterSequencer(TSharedRef<ISequencer> Sequencer);

	//Sequencer time changed
	void OnSequencerTimeChanged(TWeakPtr<class ISequencer> InSequencer);

	// Animation editor events
	void OnAnimSequenceEditorPreviewSceneCreated(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);
	void HandleInvalidateViews();

	void SetAnimSequenceEditorTime(const FQualifiedFrameTime& Time, class UAnimPreviewInstance* PreviewAnimInstance = nullptr);
	void SetSequencerTime(const FQualifiedFrameTime& Time);

private:
	// Sequencer
	FDelegateHandle OnSequencerCreatedHandle;
	FDelegateHandle OnSequencerClosedHandle;
	FDelegateHandle OnSequencerGlobalTimeChangedHandle;
	TWeakPtr<ISequencer> WeakSequencer;
	FQualifiedFrameTime LastFrameTime;
	bool bLevelSequenceEditorTimeSync;
	bool bSetGlobalTime;

	// Animation sequence editor
	FDelegateHandle OnPreviewSceneCreatedHandle;
	TWeakPtr<IPersonaPreviewScene> WeakPreviewScene;
	bool bAnimSequenceEditorTime;

	// Animation Sequencer time change
	FOnTimeChanged OnTimeChangedDelegate;
	bool bIgnoreTimeChange;
	bool bBlockTimeChangeFeedback;
	
	// Animation start frames
	TMap<FString, int32> AnimSequenceStartFrames;
};
