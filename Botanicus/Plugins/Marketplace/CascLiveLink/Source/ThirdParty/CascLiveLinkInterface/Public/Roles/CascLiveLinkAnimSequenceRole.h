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

#include "Roles/LiveLinkBasicRole.h"

#include "CascLiveLinkAnimSequenceRole.generated.h"


// LiveLink role class for AnimSequence subjects. Defines the static and frame data types used for skeletal animation streaming.
UCLASS(BlueprintType, meta = (DisplayName = "AnimSequence Role"))
class CASCLIVELINKINTERFACE_API UCascLiveLinkAnimSequenceRole : public ULiveLinkBasicRole
{
	GENERATED_BODY()
public:
	// Returns the UScriptStruct describing the static data type (FCascLiveLinkAnimSequenceStaticData).
	virtual UScriptStruct* GetStaticDataStruct() const override;

	// Returns the UScriptStruct describing the per-frame data type (FCascLiveLinkAnimSequenceFrameData).
	virtual UScriptStruct* GetFrameDataStruct() const override;

	// Blueprint data is not supported for this role. Always returns nullptr.
	virtual UScriptStruct* GetBlueprintDataStruct() const override { return nullptr; }

	// Blueprint data initialization is not supported for this role. Always returns false.
	virtual bool InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData,
										 FLiveLinkBlueprintDataStruct& OutBlueprintData) const override
	{
		return false;
	}

	// Returns the localized display name of this role shown in the LiveLink UI.
	virtual FText GetDisplayName() const override;

	// Validates the provided static data struct. Sets bOutShouldLogWarning if the data is present but malformed.
	virtual bool IsStaticDataValid(const FLiveLinkStaticDataStruct& InStaticData,
								   bool& bOutShouldLogWarning) const override;

	// Validates the provided frame data struct against the static data. Sets bOutShouldLogWarning if the data is present but malformed.
	virtual bool IsFrameDataValid(const FLiveLinkStaticDataStruct& InStaticData,
								  const FLiveLinkFrameDataStruct& InFrameData,
								  bool& bOutShouldLogWarning) const override;
};
