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

#include "LiveLinkTypes.h"
#include "UObject/ObjectMacros.h"

#include "CascLiveLinkTimelineTypes.generated.h"

class UAnimSequence;
class USkeleton;

// Base parameter struct for timeline-related data. Contains common properties shared across different timeline types.
struct CASCLIVELINKINTERFACE_API FCascLiveLinkTimelineBaseParams
{
	// Playback frame rate of the timeline.
	FFrameRate FrameRate;

	// First frame of the timeline range.
	int32 StartFrame;

	// Last frame of the timeline range.
	int32 EndFrame;

	// Display name of the sequence.
	FString SequenceName;

	// Asset path to the sequence.
	FString SequencePath;

	// Asset path of the asset linked to this timeline.
	FString LinkedAssetPath;
};

// Parameter struct for AnimSequence-specific data. Extends the base timeline params with skeleton and curve information.
struct CASCLIVELINKINTERFACE_API FCascLiveLinkAnimSequenceParams : public FCascLiveLinkTimelineBaseParams
{
	// Bone name remapping table. Maps source bone indices to target bone names.
	TArray<FName> BoneTrackRemapping;

	// Names of all animation curves present in the sequence.
	TArray<FName> CurveNames;

	// Full asset path including the sequence name.
	FString FullSequenceName;
};

// Base static data struct for timeline-based LiveLink subjects. Holds timeline properties that do not change per frame.
USTRUCT(BlueprintType)
struct CASCLIVELINKINTERFACE_API FCascLiveLinkTimelineBaseStaticData : public FLiveLinkBaseStaticData
{
	GENERATED_BODY()
public:
	// Playback frame rate of the timeline.
	UPROPERTY(EditAnywhere, Category = "TimelineParams")
	FFrameRate FrameRate;

	// Index of the first frame of the timeline range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimelineParams")
	int32 StartFrame = 0;

	// Index of the last frame of the timeline range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimelineParams")
	int32 EndFrame = 1;

	// Display name of the sequence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimelineParams")
	FString SequenceName;

	// Asset path to the sequence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimelineParams")
	FString SequencePath;

	// Asset path of the asset linked to this timeline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimelineParams")
	FString LinkedAssetPath;
};

// Static data for AnimSequence LiveLink subjects. Contains skeleton hierarchy data that does not change every frame.
USTRUCT(BlueprintType)
struct CASCLIVELINKINTERFACE_API FCascLiveLinkAnimSequenceStaticData : public FCascLiveLinkTimelineBaseStaticData
{
	GENERATED_BODY()

	// Returns the index of the root bone, identified as the bone whose parent index is negative.
	int32 FindRootBone() const { return BoneParents.IndexOfByPredicate([](int32 BoneParent) { return BoneParent < 0; }); }

	// Names of each bone in the skeleton.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TArray<FName> BoneNames;

	// Parent bone indices. For each bone, stores the index of its parent bone in BoneNames.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeleton")
	TArray<int32> BoneParents;
};

// Stores the transform data for all bones of a skeleton at a single frame.
USTRUCT()
struct CASCLIVELINKINTERFACE_API FCascLiveLinkAnimSequenceFrame
{
	GENERATED_BODY()
public:
	// World-space or local-space location for each bone, indexed in the same order as BoneNames.
	UPROPERTY()
	TArray<FVector> Locations;

	// Rotation quaternion for each bone, indexed in the same order as BoneNames.
	UPROPERTY()
	TArray<FQuat> Rotations;

	// Scale vector for each bone, indexed in the same order as BoneNames.
	UPROPERTY()
	TArray<FVector> Scales;
};

// Parameter struct for LevelSequence-specific data. Extends the base timeline params with actor and track binding GUIDs.
struct CASCLIVELINKINTERFACE_API FCascLiveLinkLevelSequenceParams : public FCascLiveLinkTimelineBaseParams
{
	// GUID identifying the actor binding within the LevelSequence.
	FGuid ActorBinding;

	// GUID identifying the track binding within the LevelSequence.
	FGuid TrackBinding;
};

// Dynamic per-frame data for AnimSequence LiveLink subjects. Contains a batch of animation frames sent from the provider.
USTRUCT()
struct CASCLIVELINKINTERFACE_API FCascLiveLinkAnimSequenceFrameData : public FLiveLinkBaseFrameData
{
	GENERATED_BODY()

	// Index of the first frame in the Frames array relative to the sequence timeline.
	UPROPERTY()
	int32 StartFrame = 0;

	// Array of per-frame bone transform data. Each element corresponds to one frame of animation.
	UPROPERTY()
	TArray<FCascLiveLinkAnimSequenceFrame> Frames;
};