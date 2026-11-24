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

#include "LiveLinkMessageBusSourceFactory.h"

#include "CascLiveLinkSourceFactory.generated.h"

// Factory class responsible for creating CascLiveLink message bus sources. Provides the UI panel and instantiation logic for the LiveLink source.
UCLASS()
class CASCLIVELINK_API UCascLiveLinkSourceFactory : public ULiveLinkMessageBusSourceFactory
{
public:
	GENERATED_BODY()

	// Returns the display name of this source factory shown in the LiveLink source picker UI.
	virtual FText GetSourceDisplayName() const override;

	// Returns the tooltip text for this source factory shown in the LiveLink source picker UI.
	virtual FText GetSourceTooltip() const override;

	// Builds and returns the widget panel used to configure and create a new LiveLink source.
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;

protected:
	// Instantiates and returns a new CascLiveLink message bus source with the given connection parameters.
	virtual TSharedPtr<FLiveLinkMessageBusSource> MakeSource(const FText& Name,
															 const FText& MachineName,
															 const FMessageAddress& Address,
															 double TimeOffset) const override;

private:
	// Called when the user selects a provider from the discovery panel. Creates the LiveLink source from the selected poll result.
	void OnSourceSelected(FProviderPollResultPtr SelectedSource, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const;
};
