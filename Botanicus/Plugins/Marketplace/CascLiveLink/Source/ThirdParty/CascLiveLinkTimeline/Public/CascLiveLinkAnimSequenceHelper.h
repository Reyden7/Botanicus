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

#include "Animation/Skeleton.h"
#include "UObject/ObjectMacros.h"

#include "CascLiveLinkAnimSequenceHelper.generated.h"

// Animation sequence helper
UCLASS(HideCategories=Object)
class UCascLiveLinkAnimSequenceHelper : public UObject
{
	GENERATED_UCLASS_BODY()

	// Push static data to animation sequence
	static CASCLIVELINKTIMELINE_API void PushStaticDataToAnimSequence(const struct FCascLiveLinkAnimSequenceStaticData& StaticData,
																		  TArray<FName>& BoneTrackRemapping,
																		  FString& AnimSequenceName);
	// Push frame data to animation sequence
	static CASCLIVELINKTIMELINE_API void PushFrameDataToAnimSequence(const struct FCascLiveLinkAnimSequenceFrameData& FrameData,
																		 const struct FCascLiveLinkAnimSequenceParams& TimelineParams);

private:
	// Update animation sequence
	static bool StaticUpdateAnimSequence(class UAnimSequence& AnimSequence,
										 USkeleton* Skeleton,
										 float SequenceLength,
										 int32 NumberOfFrames,
										 const FFrameRate& FrameRate);

	// Compute animation sequence length 
	static float ComputeAnimSequenceLength(int32 InNumberOfFrames, double InFrameRate)
	{
		return (InNumberOfFrames > 0 && InFrameRate > 0.0) ? static_cast<float>((InNumberOfFrames - 1) / InFrameRate) : 0.0f;
	}
};
