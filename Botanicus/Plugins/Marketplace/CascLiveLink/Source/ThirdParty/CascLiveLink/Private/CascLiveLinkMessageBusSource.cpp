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

#include "CascLiveLinkMessageBusSource.h"
#include "CascLiveLinkInterface.h"

#include "Async/Async.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorClassUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "ILiveLinkClient.h"
#include "CascLiveLinkModule.h"
#include "LiveLinkLog.h"
#include "CascLiveLinkMessages.h"
#include "LiveLinkRoleTrait.h"
#include "LiveLinkTypes.h"
#include "UObject/Package.h"
#include "LiveLinkClient.h"
#include "Logging/StructuredLog.h"

#include "Kismet/GameplayStatics.h"

#include "Roles/CascLiveLinkAnimSequenceRole.h"
#include "Roles/CascLiveLinkTimelineTypes.h"

#include "CascLiveLinkTimelineModule.h"
#include "CascLiveLinkAnimSequenceHelper.h"
#include "CascLiveLinkUtils.h"

#include "MessageEndpointBuilder.h"
#include "Misc/App.h"

FCascLiveLinkMessageBusSource::FCascLiveLinkMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset)
: FLiveLinkMessageBusSource(InSourceType, InSourceMachineName, InConnectionAddress, InMachineTimeOffset)
{
}

void FCascLiveLinkMessageBusSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	FLiveLinkMessageBusSource::ReceiveClient(InClient, InSourceGuid);

	if (IsMessageEndpointConnected())
	{
		FCascLiveLinkTimelineModule::GetModule().GetOnTimeChangedDelegate().AddRaw(this, &FCascLiveLinkMessageBusSource::HandleTimeChangeReturn);
	}
}

void FCascLiveLinkMessageBusSource::InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder)
{
	EndpointBuilder
		.Handling<FCascLiveLinkListAssetsRequestMessage>(this, &FCascLiveLinkMessageBusSource::HandleListAssetsRequest)
		.Handling<FCascLiveLinkListAnimSequenceSkeletonRequestMessage>(this, &FCascLiveLinkMessageBusSource::HandleListAnimSequenceSkeletonRequest)
		.Handling<FCascLiveLinkListActorsRequestMessage>(this, &FCascLiveLinkMessageBusSource::HandleListActorsRequest)
		.Handling<FCascLiveLinkTimeChangeMessage>(this, &FCascLiveLinkMessageBusSource::HandleTimeChangeRequest)
		.Handling<FCascLiveLinkShutdownMessage>(this, &FCascLiveLinkMessageBusSource::HandleShutdownRequest);
	
	FLiveLinkMessageBusSource::InitializeMessageEndpoint(EndpointBuilder);
}

void FCascLiveLinkMessageBusSource::InitializeAndPushStaticData_AnyThread(FName SubjectName,
																		  TSubclassOf<ULiveLinkRole> SubjectRole,
																		  const FLiveLinkSubjectKey& SubjectKey,
																		  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
																		  UScriptStruct* MessageTypeInfo)
{
	check(MessageTypeInfo->IsChildOf(FLiveLinkBaseStaticData::StaticStruct()));

	const FLiveLinkBaseStaticData* Message = reinterpret_cast<const FLiveLinkBaseStaticData*>(Context->GetMessage());

	if (SubjectRole->IsChildOf(UCascLiveLinkAnimSequenceRole::StaticClass()) &&
		MessageTypeInfo->IsChildOf(FCascLiveLinkAnimSequenceStaticData::StaticStruct()))
	{
		const FCascLiveLinkAnimSequenceStaticData& StaticData = *reinterpret_cast<const FCascLiveLinkAnimSequenceStaticData*>(Context->GetMessage());
		{
			FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
			FCascLiveLinkAnimSequenceParams& TimelineParams = SubjectTimelineParams.FindOrAdd(SubjectName);
			TimelineParams.SequenceName = StaticData.SequenceName;
			TimelineParams.SequencePath = StaticData.SequencePath;
			TimelineParams.FrameRate = StaticData.FrameRate;
			TimelineParams.StartFrame = StaticData.StartFrame;
			TimelineParams.EndFrame = StaticData.EndFrame;
			TimelineParams.LinkedAssetPath = StaticData.LinkedAssetPath;
		}

		TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataStruct = MakeShareable(new FLiveLinkStaticDataStruct(MessageTypeInfo));
		StaticDataStruct->InitializeWith(MessageTypeInfo, Message);
		AsyncTask(ENamedThreads::GameThread, [this, SubjectName, StaticDataStruct]()
		{
			PushStaticDataToAnimSequence(SubjectName, StaticDataStruct);
			SendMessage(FMessageEndpoint::MakeMessage<FCascLiveLinkAnimSequenceStaticDataReturnMessage>());
		});

		FLiveLinkStaticDataStruct DataStruct(MessageTypeInfo);
		PushClientSubjectStaticData_AnyThread(SubjectKey, SubjectRole, MoveTemp(DataStruct));
	}
	else
	{
		FLiveLinkMessageBusSource::InitializeAndPushStaticData_AnyThread(SubjectName, SubjectRole, SubjectKey, Context, MessageTypeInfo);
	}
}

void FCascLiveLinkMessageBusSource::InitializeAndPushFrameData_AnyThread(FName SubjectName,
																		 const FLiveLinkSubjectKey& SubjectKey,
																		 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
																		 UScriptStruct* MessageTypeInfo)
{
	check(MessageTypeInfo->IsChildOf(FLiveLinkBaseFrameData::StaticStruct()));

	FLiveLinkFrameDataStruct DataStruct(MessageTypeInfo);
	const FLiveLinkBaseFrameData* Message = reinterpret_cast<const FLiveLinkBaseFrameData*>(Context->GetMessage());

	if (MessageTypeInfo->IsChildOf(FCascLiveLinkAnimSequenceFrameData::StaticStruct()))
	{
		TSharedPtr<FLiveLinkFrameDataStruct, ESPMode::ThreadSafe> FrameDataStruct = MakeShareable(new FLiveLinkFrameDataStruct(MessageTypeInfo));
		FrameDataStruct->InitializeWith(MessageTypeInfo, Message);
		FrameDataStruct->GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();

		FScopeLock Lock(&SubjectTimelineParamsCriticalSection);
		if (auto TimelineParams = SubjectTimelineParams.Find(SubjectName))
		{
			TSharedPtr<FCascLiveLinkAnimSequenceParams, ESPMode::ThreadSafe> TimelineParamsDup = MakeShareable(new FCascLiveLinkAnimSequenceParams(*TimelineParams));
			AsyncTask(ENamedThreads::GameThread, [this, FrameDataStruct, TimelineParamsDup]()
			{
				auto FrameDataPtr = FrameDataStruct.Get();
				UCascLiveLinkAnimSequenceHelper::PushFrameDataToAnimSequence(*FrameDataPtr->Cast<FCascLiveLinkAnimSequenceFrameData>(),
																			 *TimelineParamsDup.Get());
			});
		}
		DataStruct.GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();
		PushClientSubjectFrameData_AnyThread(SubjectKey, MoveTemp(DataStruct));
	}
	else
	{
		FLiveLinkMessageBusSource::InitializeAndPushFrameData_AnyThread(SubjectName, SubjectKey, Context, MessageTypeInfo);
	}
}

void FCascLiveLinkMessageBusSource::HandleListAssetsRequest(const FCascLiveLinkListAssetsRequestMessage& Message,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{

	if (Message.AssetClass.IsEmpty())
	{
		return;
	}			

	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		UE_LOGFMT(LogCascLiveLink, Display, "Start find Assets {0}", Message.AssetClass);

		TArray<FAssetData> OutAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FCascLiveLinkListAssetsReturnMessage* ReturnMessage = FMessageEndpoint::MakeMessage<FCascLiveLinkListAssetsReturnMessage>();
		UClass* AssetClass = FCascLiveLinkUtils::FindObject<UClass>(Message.AssetClass);
		if (AssetClass)
		{
			FTopLevelAssetPath AssetClassPath(AssetClass->GetPathName());
			AssetRegistry.GetAssetsByClass(AssetClassPath, OutAssetData, Message.SearchSubClasses);
			if (OutAssetData.Num() > 0)
			{
				TMap<FString, FStringArray>& AssetsByClass = ReturnMessage->AssetsByClass;
				for (auto& AssetData : OutAssetData)
				{
					auto& Class = AssetsByClass.FindOrAdd(AssetData.AssetClassPath.ToString());
					Class.Array.Add(AssetData.GetSoftObjectPath().ToString());
				}
			}
            else
            {
				UE_LOGFMT(LogCascLiveLink, Display, "Empty Asset Data");
            }
		}
		SendMessage(ReturnMessage);
	});
}

void FCascLiveLinkMessageBusSource::HandleListAnimSequenceSkeletonRequest(const FCascLiveLinkListAnimSequenceSkeletonRequestMessage& Message,
																		  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		TArray<FAssetData> OutAssetData;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		auto ReturnMessage = FMessageEndpoint::MakeMessage<FCascLiveLinkListAnimSequenceSkeletonReturnMessage>();
		FTopLevelAssetPath AssetClassPath(UAnimSequence::StaticClass()->GetPathName());
		AssetRegistry.GetAssetsByClass(AssetClassPath, OutAssetData, true);

		if (OutAssetData.Num() > 0)
		{
			TMap<FString, FStringArray>& AssetsByClass = ReturnMessage->AnimSequencesBySkeleton;
			for (auto& AssetData : OutAssetData)
			{
				UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
				if (AnimSequence && AnimSequence->GetSkeleton())
				{
					FString SkeletonName;
					UPackage* Package = AnimSequence->GetSkeleton()->GetPackage();
					if (Package)
					{
						SkeletonName = Package->GetName();
					}
					else
					{
						FString PathName = AnimSequence->GetSkeleton()->GetPathName();
						int32 Index = 0;
						PathName.FindLastChar('.', Index);
						SkeletonName = Index > 0 ? PathName.Mid(0, Index) : PathName;
					}

					FStringArray& Class = AssetsByClass.FindOrAdd(SkeletonName);
					FString ObjectPath = AssetData.GetSoftObjectPath().ToString();

					int32 Index = 0;
					ObjectPath.FindLastChar('.', Index);
					Class.Array.Add(Index > 0 ? ObjectPath.Mid(0, Index) : ObjectPath);
				}
			}
		}

		MessageEndpoint->Send(ReturnMessage, EMessageFlags::Reliable, {}, nullptr, { ConnectionAddress }, FTimespan::Zero(), FDateTime::MaxValue());

	});
}

void FCascLiveLinkMessageBusSource::HandleListActorsRequest(const FCascLiveLinkListActorsRequestMessage& Message,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.ActorClass.IsEmpty())
	{
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
 		TArray<AActor*> OutActors;
		UWorld* EditorWorld = nullptr;
#if WITH_EDITOR
		if (UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
		{
			EditorWorld = UnrealEditorSubsystem->GetEditorWorld();
		}
#endif // WITH_EDITOR

		if (!EditorWorld)
		{
			return;
		}

		UGameplayStatics::GetAllActorsOfClass(EditorWorld, FEditorClassUtils::GetClassFromString(Message.ActorClass), OutActors);

		FCascLiveLinkListActorsReturnMessage* ReturnMessage = FMessageEndpoint::MakeMessage<FCascLiveLinkListActorsReturnMessage>();
		if (OutActors.Num() > 0)
		{
			// Build the list of actors of the matching class, including child classes
			auto& ActorsByClass = ReturnMessage->ActorsByClass;
			for (auto& Actor : OutActors)
			{
				auto& Class = ActorsByClass.FindOrAdd(Actor->GetClass()->GetName());
				FString ActorPath;
				if (Actor->GetFolderPath() != NAME_None)
				{
					ActorPath = Actor->GetFolderPath().ToString() + "/";
				}
				ActorPath += Actor->GetActorLabel();
				Class.Array.Add(ActorPath);
			}
		}

		MessageEndpoint->Send(ReturnMessage, EMessageFlags::Reliable, {}, nullptr, { ConnectionAddress }, FTimespan::Zero(), FDateTime::MaxValue());
	});
}

void FCascLiveLinkMessageBusSource::HandleTimeChangeRequest(const FCascLiveLinkTimeChangeMessage& Message,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	AsyncTask(ENamedThreads::GameThread, [Message, this]()
	{
		FCascLiveLinkTimelineModule::GetModule().SetCurrentTime(Message.Time);
	});
}

void FCascLiveLinkMessageBusSource::HandleShutdownRequest(const FCascLiveLinkShutdownMessage&, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&)
{
	UE_LOG(LogCascLiveLink, Display, TEXT("Shutdown Live Link"));

	FCascLiveLinkTimelineModule::GetModule().GetOnTimeChangedDelegate().RemoveAll(this);
	FCascLiveLinkTimelineModule::GetModule().RemoveAllAnimSequenceStartFrames();
	static_cast<FLiveLinkClient*>(Client)->RemoveAllSources();
	FLiveLinkMessageBusSource::RequestSourceShutdown();
}

void FCascLiveLinkMessageBusSource::HandleTimeChangeReturn(const FQualifiedFrameTime& Time)
{
	if (IsMessageEndpointConnected())
	{
		auto Message = FMessageEndpoint::MakeMessage<FCascLiveLinkTimeChangeMessage>();
		Message->Time = Time;
		SendMessage(Message);
	}
}

bool FCascLiveLinkMessageBusSource::RequestSourceShutdown()
{
	if (IsMessageEndpointConnected())
	{
		SendMessage(FMessageEndpoint::MakeMessage<FCascLiveLinkShutdownMessage>());
	}

	FCascLiveLinkTimelineModule::GetModule().GetOnTimeChangedDelegate().RemoveAll(this);
	FCascLiveLinkTimelineModule::GetModule().RemoveAllAnimSequenceStartFrames();

	return FLiveLinkMessageBusSource::RequestSourceShutdown();
}

void FCascLiveLinkMessageBusSource::PushStaticDataToAnimSequence(const FName& SubjectName,
																 TSharedPtr<FLiveLinkStaticDataStruct, ESPMode::ThreadSafe> StaticDataPtr)
{
	if (!StaticDataPtr)
	{
		return;
	}

	auto TimelineStaticDataPtr = StaticDataPtr->Cast<FCascLiveLinkAnimSequenceStaticData>();
	if (!TimelineStaticDataPtr)
	{
		return;
	}

	auto& StaticData = *TimelineStaticDataPtr;

	TArray<FName> BoneTrackRemapping;
	FString AnimSequenceName;
	UCascLiveLinkAnimSequenceHelper::PushStaticDataToAnimSequence(StaticData, BoneTrackRemapping, AnimSequenceName);
	{
		FScopeLock Lock(&SubjectTimelineParamsCriticalSection);

		if (auto Params = SubjectTimelineParams.Find(SubjectName))
		{
			Params->BoneTrackRemapping = MoveTemp(BoneTrackRemapping);
			Params->FullSequenceName = AnimSequenceName;
			Params->CurveNames.Empty();
			Params->CurveNames.Append(StaticData.PropertyNames);
		}
	}

	if (!AnimSequenceName.IsEmpty())
	{
		FCascLiveLinkTimelineModule::GetModule().AddAnimSequenceStartFrame(AnimSequenceName, StaticData.StartFrame);
	}
}
