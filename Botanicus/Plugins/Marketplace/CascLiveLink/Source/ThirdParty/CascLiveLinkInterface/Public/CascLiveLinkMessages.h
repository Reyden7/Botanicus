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

#include "LiveLinkMessages.h"

#include "UObject/Object.h"

#include "CascLiveLinkMessages.generated.h"

// Extended Ping discovery message for CascLiveLink. Sent by the client when searching for available LiveLink providers.
USTRUCT()
struct CASCLIVELINKINTERFACE_API FCascLiveLinkPingMessage : public FLiveLinkPingMessage
{
	GENERATED_BODY()

	// CascLiveLink plugin version on the sender's side.
	UPROPERTY()
	FString CascLiveLinkVersion;

	// Unreal Engine version on the sender's side.
	UPROPERTY()
	FString UnrealVersion;

	// Default constructor. Initializes fields with default values.
	FCascLiveLinkPingMessage();

	// Constructs the message with a poll request GUID and LiveLink protocol version.
	FCascLiveLinkPingMessage(const FGuid& InPollRequest, int32 InLiveLinkVersion);
};

// Extended Pong response message for CascLiveLink. Sent by the provider in response to FCascLiveLinkPingMessage.
USTRUCT()
struct CASCLIVELINKINTERFACE_API FCascLiveLinkPongMessage : public FLiveLinkPongMessage
{
	GENERATED_BODY()

	// CascLiveLink plugin version on the provider's side.
	UPROPERTY()
	FString CascLiveLinkVersion;

	// Unreal Engine version on the provider's side.
	UPROPERTY()
	FString UnrealVersion;

	// Default constructor. Initializes fields with default values.
	FCascLiveLinkPongMessage();

	// Constructs the message with provider name, machine name, poll request GUID, and LiveLink protocol version.
	FCascLiveLinkPongMessage(const FString& InProviderName, const FString& InMachineName, const FGuid& InPollRequest, int32 InLiveLinkVersion);
};

// Wrapper struct around a string array. Used as a TMap value since UE does not support nested containers directly.
USTRUCT()
struct FStringArray
{
	GENERATED_BODY()

	// The array of strings.
	UPROPERTY()
	TArray<FString> Array;
};

// Request message to retrieve a list of assets of the specified class.
USTRUCT()
struct FCascLiveLinkListAssetsRequestMessage
{
	GENERATED_BODY()

	// Asset class name to filter by (e.g. "StaticMesh", "SkeletalMesh").
	UPROPERTY()
	FString AssetClass;

	// If true, assets belonging to subclasses of AssetClass will also be included in the result.
	UPROPERTY()
	bool SearchSubClasses = false;
};

// Response message containing the list of assets grouped by class name.
USTRUCT()
struct FCascLiveLinkListAssetsReturnMessage
{
	GENERATED_BODY()

	// Map of assets grouped by class name. Key is the class name, value is the list of asset paths.
	UPROPERTY()
	TMap<FString, FStringArray> AssetsByClass;
};

// Request message to retrieve all AnimSequences grouped by skeleton. Carries no parameters — requests the full list from the editor.
USTRUCT()
struct FCascLiveLinkListAnimSequenceSkeletonRequestMessage
{
	GENERATED_BODY()
};

// Response message containing AnimSequences grouped by skeleton asset.
USTRUCT()
struct FCascLiveLinkListAnimSequenceSkeletonReturnMessage
{
	GENERATED_BODY()

	// Map of AnimSequences grouped by skeleton. Key is the skeleton path/name, value is the list of AnimSequence paths.
	UPROPERTY()
	TMap<FString, FStringArray> AnimSequencesBySkeleton;
};

// Request message to retrieve a list of actors of the specified class placed in the level.
USTRUCT()
struct FCascLiveLinkListActorsRequestMessage
{
	GENERATED_BODY()

	// Actor class name to filter by (e.g. "SkeletalMeshActor").
	UPROPERTY()
	FString ActorClass;
};

// Response message containing the list of actors grouped by class name.
USTRUCT()
struct FCascLiveLinkListActorsReturnMessage
{
	GENERATED_BODY()

	// Map of actors grouped by class name. Key is the class name, value is the list of actor names or identifiers.
	UPROPERTY()
	TMap<FString, FStringArray> ActorsByClass;
};

// Message sent by the provider when the current playback time changes (e.g. during scrubbing or playback).
USTRUCT()
struct FCascLiveLinkTimeChangeMessage
{
	GENERATED_BODY()

	// New frame-rate-qualified time value used to synchronize playback on the client side.
	UPROPERTY()
	FQualifiedFrameTime Time;
};

// Shutdown notification message. Sent by the provider before disconnecting to notify all connected clients.
USTRUCT()
struct FCascLiveLinkShutdownMessage
{
	GENERATED_BODY()
};

// Response message carrying static data for an AnimSequence. The exact data composition is defined by the provider implementation.
USTRUCT()
struct FCascLiveLinkAnimSequenceStaticDataReturnMessage
{
	GENERATED_BODY()
};