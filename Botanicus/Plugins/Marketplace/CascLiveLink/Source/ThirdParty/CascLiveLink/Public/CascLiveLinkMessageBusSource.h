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

#include "LiveLinkMessageBusSource.h"

#include "HAL/ThreadSafeBool.h"
#include "IMessageContext.h"
#include "LiveLinkRole.h"
#include "MessageEndpoint.h"
#include "Roles/CascLiveLinkTimelineTypes.h"

// LiveLink message bus source implementation for CascLiveLink. Handles incoming messages and pushes animation data to the LiveLink client.
class CASCLIVELINK_API FCascLiveLinkMessageBusSource : public FLiveLinkMessageBusSource
{
public:
	// Constructs the source with connection identity and time offset relative to the remote machine.
	FCascLiveLinkMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset);

	// Registers the LiveLink client and source GUID, and begins receiving data from the message bus.
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;

	// Initiates a graceful shutdown of this source and returns true when it is safe to destroy it.
	virtual bool RequestSourceShutdown() override;

protected:
	// Returns the internal name identifier used to distinguish this source type on the message bus.
	virtual const FName& GetSourceName() const override { static FName Name(TEXT("CascLiveLinkMessageBusSource")); return Name; }

	// Registers all message handlers on the provided endpoint builder.
	virtual void InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder) override;

	// Constructs and pushes static subject data to the LiveLink client. Called from any thread when a new subject is first encountered.
	virtual void InitializeAndPushStaticData_AnyThread(FName SubjectName,
													   TSubclassOf<ULiveLinkRole> SubjectRole,
													   const FLiveLinkSubjectKey& SubjectKey,
													   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
													   UScriptStruct* MessageTypeInfo) override;

	// Constructs and pushes per-frame subject data to the LiveLink client. Called from any thread on each frame update.
	virtual void InitializeAndPushFrameData_AnyThread(FName SubjectName,
													  const FLiveLinkSubjectKey& SubjectKey,
													  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
													  UScriptStruct* MessageTypeInfo) override;

private:
	// Handles an incoming request to list available assets. Sends back a response with matching asset paths.
	void HandleListAssetsRequest(const struct FCascLiveLinkListAssetsRequestMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	// Handles an incoming request to list AnimSequences grouped by skeleton. Sends back the grouped result.
	void HandleListAnimSequenceSkeletonRequest(const struct FCascLiveLinkListAnimSequenceSkeletonRequestMessage& Message,
											   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	// Handles an incoming request to list actors placed in the current level. Sends back a response with matching actors.
	void HandleListActorsRequest(const struct FCascLiveLinkListActorsRequestMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	// Handles an incoming timeline time change message and applies the new time to the active sequence.
	void HandleTimeChangeRequest(const struct FCascLiveLinkTimeChangeMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	// Handles an incoming shutdown notification from the remote provider and initiates source teardown.
	void HandleShutdownRequest(const struct FCascLiveLinkShutdownMessage& Message,
								 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	// Applies a validated frame time change to the AnimSequence editor timeline on the game thread.
	void HandleTimeChangeReturn(const FQualifiedFrameTime& Time);

	// Populates and pushes static skeleton data for an AnimSequence subject to the LiveLink client.
	void PushStaticDataToAnimSequence(const FName& SubjectName,
									  TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr);

	// Populates and pushes static binding data for a LevelSequence subject to the LiveLink client.
	void PushStaticDataToLevelSequence(const FName& SubjectName,
									  TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr);

private:
	// Maps each AnimSequence subject name to its timeline parameters received from the provider.
	TMap<FName, FCascLiveLinkAnimSequenceParams> SubjectTimelineParams;

	// Maps each LevelSequence subject name to its timeline and binding parameters received from the provider.
	TMap<FName, FCascLiveLinkLevelSequenceParams> SubjectLevelSequenceParams;

	// Guards concurrent access to SubjectTimelineParams and SubjectLevelSequenceParams from multiple threads.
	FCriticalSection SubjectTimelineParamsCriticalSection;
};