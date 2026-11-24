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

#include "LiveLinkMessageBusFinder.h"

#include "Misc/App.h"

#include "Widgets/SCompoundWidget.h"

#include "Widgets/Views/SListView.h"

// Delegate called when the user selects a LiveLink provider from the source picker list.
DECLARE_DELEGATE_OneParam(FOnCascLiveLinkSourceSelected, FProviderPollResultPtr);

// Slate widget that displays a list of discovered CascLiveLink providers and allows the user to select one.
class SCascLiveLinkSourceFactory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SCascLiveLinkSourceFactory) {}
		// Delegate invoked when the user selects a provider from the list.
		SLATE_EVENT(FOnCascLiveLinkSourceSelected, OnSourceSelected)
	SLATE_END_ARGS()

	// Destructor. Cleans up any active provider discovery polling.
	virtual ~SCascLiveLinkSourceFactory();

	// Constructs the widget layout and initializes provider discovery.
	void Construct(const FArguments& Args);

	// Called every frame to refresh the discovered provider list if enough time has elapsed since the last update.
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);

	// Returns the provider poll result that is currently selected in the list, or nullptr if nothing is selected.
	FProviderPollResultPtr GetSelectedSource() const { return SelectedResult; }

private:
	// Called when the list view selection changes. Updates SelectedResult and broadcasts the OnSourceSelected delegate.
	void OnSelectionChanged(TSharedPtr<struct FCascLiveLinkSource> Source,
							ESelectInfo::Type Type);

	// Creates and returns a table row widget for the given provider source entry.
	TSharedRef<class ITableRow> CreateListView(TSharedPtr<struct FCascLiveLinkSource> Source,
											   const TSharedRef<class STableViewBase>& TableView) const;

private:
	// List of currently discovered CascLiveLink provider sources.
	TArray<TSharedPtr<struct FCascLiveLinkSource>> Sources;

	// Slate list view widget that renders the discovered provider sources.
	TSharedPtr<SListView<TSharedPtr<struct FCascLiveLinkSource>>> ListView;

	// Delegate broadcast when the user selects a provider from the list.
	FOnCascLiveLinkSourceSelected OnSourceSelected;

	// The provider poll result corresponding to the currently selected list entry.
	FProviderPollResultPtr SelectedResult;

	// Timestamp of the last provider list refresh, used to throttle polling frequency.
	double LastUpdateTime = 0;
};
