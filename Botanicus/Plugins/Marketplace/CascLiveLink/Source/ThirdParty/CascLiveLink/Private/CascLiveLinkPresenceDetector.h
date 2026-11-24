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
#include <atomic>

#include "CoreMinimal.h"

#include "LiveLinkMessageBusFinder.h"
#include "CascLiveLinkMessages.h"
#include "MessageEndpoint.h"

#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"


// Background thread worker that periodically broadcasts Ping messages to discover available CascLiveLink providers on the network.
class FCascLiveLinkPresenceDetector : public FRunnable
{
public:
	// Constructs the detector and spawns the background polling thread.
	FCascLiveLinkPresenceDetector();

	// Destructor. Stops the polling thread and releases all resources.
	virtual ~FCascLiveLinkPresenceDetector();

	// Main loop of the background thread. Periodically sends Ping messages and waits for Pong responses.
	virtual uint32 Run() override;

	// Signals the background thread to stop polling and exit its run loop.
	virtual void Stop() override;

	// Returns true if the background polling thread is currently active.
	bool IsRunning() const { return bIsRunning; }

	// Increments the active request counter. Polling starts when the first request is added.
	void AddPresenceRequest();

	// Decrements the active request counter. Polling stops when the counter reaches zero.
	void RemovePresenceRequest();

	// Copies the current list of discovered provider poll results into the provided array.
	void GetResults(TArray<FProviderPollResultPtr>& Results);

private:
	// Handles an incoming Pong response from a provider and stores its poll result.
	void HandlePongMessage(const struct FCascLiveLinkPongMessage& Message,
						   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

private:
	// Message bus endpoint used to send Ping messages and receive Pong responses.
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	// Handle to the background thread running the polling loop.
	class FRunnableThread* Thread;

	// Indicates whether the polling thread is currently running.
	std::atomic_bool bIsRunning;

	// Guards concurrent access to PollResults from the polling thread and external callers.
	FCriticalSection CriticalSection;

	// Number of active presence requests. Polling is active while this value is greater than zero.
	std::atomic_int32_t NumberRequests;

	// Accumulated list of provider poll results received since the last poll cycle.
	TArray<FProviderPollResultPtr> PollResults;

	// Interval in seconds between consecutive Ping broadcast messages.
	double PingRequestFrequency;

	// Unique identifier for the current Ping session, used to match Pong responses to their originating Ping.
	FGuid PingId;
};