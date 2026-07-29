// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/BotanicusMultiplayerSubsystem.h"

#include "Botanicus.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"

const FName UBotanicusMultiplayerSubsystem::BotanicusBuildSetting(TEXT("BOTANICUS_BUILD"));
const FString UBotanicusMultiplayerSubsystem::BotanicusBuildValue(TEXT("Botanicus-Dev-1"));

void UBotanicusMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(
				this,
				&UBotanicusMultiplayerSubsystem::HandleSessionInviteAccepted));
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Multiplayer subsystem initialized. OSS=%s SteamAvailable=%s"),
		*GetOnlineSubsystemName(),
		IsSteamAvailable() ? TEXT("true") : TEXT("false"));
}

void UBotanicusMultiplayerSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		ClearOperationDelegates();

		if (InviteAcceptedHandle.IsValid())
		{
			Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
			InviteAcceptedHandle.Reset();
		}
	}

	SessionSearch.Reset();
	Super::Deinitialize();
}

EBotanicusOnlineState UBotanicusMultiplayerSubsystem::GetOnlineState() const
{
	return OnlineState;
}

bool UBotanicusMultiplayerSubsystem::IsSteamAvailable() const
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	return Subsystem &&
		Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM &&
		Subsystem->GetSessionInterface().IsValid();
}

bool UBotanicusMultiplayerSubsystem::HasActiveSession() const
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		return Sessions->GetNamedSession(NAME_GameSession) != nullptr;
	}

	return false;
}

FString UBotanicusMultiplayerSubsystem::GetOnlineSubsystemName() const
{
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		return Subsystem->GetSubsystemName().ToString();
	}

	return TEXT("None");
}

void UBotanicusMultiplayerSubsystem::HostSession(int32 MaxPlayers, const FString& LobbyMap)
{
	if (!EnsureIdle(NSLOCTEXT("BotanicusOnline", "HostOperation", "Créer une partie")))
	{
		return;
	}

	if (LobbyMap.IsEmpty())
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "MissingLobbyMap", "La carte du lobby est manquante."));
		return;
	}

	PendingMaxPlayers = FMath::Clamp(MaxPlayers, 2, 8);
	PendingHostMap = LobbyMap;

	if (HasActiveSession())
	{
		BeginDestroySession(EPendingAction::Host);
	}
	else
	{
		BeginCreateSession();
	}
}

void UBotanicusMultiplayerSubsystem::FindSessions(int32 MaxResults)
{
	if (!EnsureIdle(NSLOCTEXT("BotanicusOnline", "SearchOperation", "Rechercher des parties")))
	{
		return;
	}

	IOnlineSessionPtr Sessions = GetSessionInterface();
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Sessions || !Subsystem)
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "NoSessionInterfaceSearch", "Le service de sessions en ligne est indisponible."));
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = FMath::Clamp(MaxResults, 1, 500);
	SessionSearch->PingBucketSize = 50;
	SessionSearch->bIsLanQuery = Subsystem->GetSubsystemName() == NULL_SUBSYSTEM;
	// UE 5.8 replaced the legacy SEARCH_PRESENCE query with an explicit lobby query.
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(
		BotanicusBuildSetting,
		BotanicusBuildValue,
		EOnlineComparisonOp::Equals);

	FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(
			this,
			&UBotanicusMultiplayerSubsystem::HandleFindSessionsComplete));

	SetOnlineState(EBotanicusOnlineState::Searching);

	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		FindSessionsHandle.Reset();
		SessionSearch.Reset();
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "SearchStartFailed", "Impossible de lancer la recherche de parties."));
	}
}

void UBotanicusMultiplayerSubsystem::JoinSessionByIndex(int32 ResultIndex)
{
	if (!EnsureIdle(NSLOCTEXT("BotanicusOnline", "JoinOperation", "Rejoindre une partie")))
	{
		return;
	}

	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "InvalidSearchResult", "Cette partie n’est plus disponible. Relancez la recherche."));
		return;
	}

	PendingJoinResult = SessionSearch->SearchResults[ResultIndex];
	bHasPendingJoinResult = true;

	if (HasActiveSession())
	{
		BeginDestroySession(EPendingAction::Join);
	}
	else
	{
		BeginJoinSession(PendingJoinResult);
	}
}

void UBotanicusMultiplayerSubsystem::LeaveSession(const FString& MenuMap)
{
	if (!EnsureIdle(NSLOCTEXT("BotanicusOnline", "LeaveOperation", "Quitter la partie")))
	{
		return;
	}

	PendingMenuMap = MenuMap;

	if (HasActiveSession())
	{
		BeginDestroySession(EPendingAction::Leave);
	}
	else
	{
		if (!PendingMenuMap.IsEmpty())
		{
			TravelToMap(PendingMenuMap, false);
		}
		OnSessionLeft.Broadcast(true);
	}
}

bool UBotanicusMultiplayerSubsystem::ShowSteamInviteOverlay()
{
	if (!HasActiveSession())
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "InviteWithoutSession", "Créez d’abord une partie avant d’inviter des amis."));
		return false;
	}

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem)
	{
		return false;
	}

	IOnlineExternalUIPtr ExternalUI = Subsystem->GetExternalUIInterface();
	if (!ExternalUI || !ExternalUI->ShowInviteUI(0, NAME_GameSession))
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "InviteOverlayFailed", "Impossible d’ouvrir la fenêtre d’invitation Steam."));
		return false;
	}

	return true;
}

IOnlineSessionPtr UBotanicusMultiplayerSubsystem::GetSessionInterface() const
{
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		return Subsystem->GetSessionInterface();
	}

	return nullptr;
}

void UBotanicusMultiplayerSubsystem::SetOnlineState(EBotanicusOnlineState NewState)
{
	if (OnlineState == NewState)
	{
		return;
	}

	OnlineState = NewState;
	OnOnlineStateChanged.Broadcast(OnlineState);
}

void UBotanicusMultiplayerSubsystem::ReportError(const FText& Message)
{
	UE_LOG(LogBotanicus, Warning, TEXT("%s"), *Message.ToString());
	OnOnlineError.Broadcast(Message);
}

bool UBotanicusMultiplayerSubsystem::EnsureIdle(const FText& OperationName)
{
	if (OnlineState == EBotanicusOnlineState::Idle)
	{
		return true;
	}

	ReportError(FText::Format(
		NSLOCTEXT("BotanicusOnline", "OperationAlreadyRunning", "{0} est impossible : une opération réseau est déjà en cours."),
		OperationName));
	return false;
}

void UBotanicusMultiplayerSubsystem::BeginCreateSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Sessions || !Subsystem)
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "NoSessionInterfaceCreate", "Steam n’est pas disponible. Vérifiez que Steam est lancé."));
		OnSessionCreated.Broadcast(false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = Subsystem->GetSubsystemName() == NULL_SUBSYSTEM;
	Settings.NumPublicConnections = PendingMaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.Set(
		SETTING_MAPNAME,
		PendingHostMap,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(
		BotanicusBuildSetting,
		BotanicusBuildValue,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(
			this,
			&UBotanicusMultiplayerSubsystem::HandleCreateSessionComplete));

	SetOnlineState(EBotanicusOnlineState::Creating);

	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		CreateSessionHandle.Reset();
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "CreateStartFailed", "Impossible de lancer la création de la partie Steam."));
		OnSessionCreated.Broadcast(false);
	}
}

void UBotanicusMultiplayerSubsystem::BeginJoinSession(
	const FOnlineSessionSearchResult& SearchResult)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !SearchResult.IsValid())
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "InvalidJoinTarget", "La partie à rejoindre n’est plus valide."));
		OnSessionJoined.Broadcast(false);
		return;
	}

	JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(
			this,
			&UBotanicusMultiplayerSubsystem::HandleJoinSessionComplete));

	SetOnlineState(EBotanicusOnlineState::Joining);

	if (!Sessions->JoinSession(0, NAME_GameSession, SearchResult))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		JoinSessionHandle.Reset();
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "JoinStartFailed", "Impossible de lancer la connexion à cette partie."));
		OnSessionJoined.Broadcast(false);
	}
}

void UBotanicusMultiplayerSubsystem::BeginDestroySession(EPendingAction Action)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions)
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "NoSessionInterfaceDestroy", "Le service de sessions est indisponible."));
		return;
	}

	PendingAction = Action;
	DestroySessionHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(
			this,
			&UBotanicusMultiplayerSubsystem::HandleDestroySessionComplete));

	SetOnlineState(EBotanicusOnlineState::Destroying);

	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
		DestroySessionHandle.Reset();
		PendingAction = EPendingAction::None;
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "DestroyStartFailed", "Impossible de quitter la session actuelle."));
	}
}

void UBotanicusMultiplayerSubsystem::TravelToMap(
	const FString& MapPath,
	bool bListenServer)
{
	if (MapPath.IsEmpty())
	{
		return;
	}

	SetOnlineState(EBotanicusOnlineState::Traveling);
	const FString Options = bListenServer ? TEXT("listen") : FString();
	UGameplayStatics::OpenLevel(this, FName(*MapPath), true, Options);
	SetOnlineState(EBotanicusOnlineState::Idle);
}

void UBotanicusMultiplayerSubsystem::HandleCreateSessionComplete(
	FName SessionName,
	bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	}
	CreateSessionHandle.Reset();

	OnSessionCreated.Broadcast(bWasSuccessful);

	if (!bWasSuccessful)
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "CreateFailed", "Steam n’a pas pu créer la partie."));
		return;
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Steam lobby created successfully for up to %d players."),
		PendingMaxPlayers);

	TravelToMap(PendingHostMap, true);
}

void UBotanicusMultiplayerSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
	}
	FindSessionsHandle.Reset();

	TArray<FBotanicusSessionInfo> PublicResults;
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		PublicResults.Reserve(SessionSearch->SearchResults.Num());

		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];
			const FOnlineSession& Session = Result.Session;

			FBotanicusSessionInfo Info;
			Info.ResultIndex = Index;
			Info.HostName = Session.OwningUserName;
			Info.SessionId = Session.GetSessionIdStr();
			Info.MaxPlayers =
				Session.SessionSettings.NumPublicConnections +
				Session.SessionSettings.NumPrivateConnections;
			Info.CurrentPlayers =
				Info.MaxPlayers -
				Session.NumOpenPublicConnections -
				Session.NumOpenPrivateConnections;
			Info.PingMs = Result.PingInMs;
			Info.bJoinable = Result.IsValid() && Session.NumOpenPublicConnections > 0;
			PublicResults.Add(MoveTemp(Info));
		}
	}

	SetOnlineState(EBotanicusOnlineState::Idle);
	OnSessionsFound.Broadcast(bWasSuccessful, PublicResults);

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Session search completed. Success=%s Results=%d"),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		PublicResults.Num());

	for (const FBotanicusSessionInfo& Result : PublicResults)
	{
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("  [%d] Host=%s Players=%d/%d Ping=%d Joinable=%s Session=%s"),
			Result.ResultIndex,
			*Result.HostName,
			Result.CurrentPlayers,
			Result.MaxPlayers,
			Result.PingMs,
			Result.bJoinable ? TEXT("true") : TEXT("false"),
			*Result.SessionId);
	}

	if (!bWasSuccessful)
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "SearchFailed", "La recherche de parties Steam a échoué."));
	}
}

void UBotanicusMultiplayerSubsystem::HandleJoinSessionComplete(
	FName SessionName,
	EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}
	JoinSessionHandle.Reset();

	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions)
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(FText::Format(
			NSLOCTEXT("BotanicusOnline", "JoinFailed", "Connexion à la partie impossible ({0})."),
			FText::FromString(LexToString(Result))));
		OnSessionJoined.Broadcast(false);
		return;
	}

	FString ConnectString;
	if (!Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "AddressResolutionFailed", "Steam n’a pas fourni l’adresse de connexion."));
		OnSessionJoined.Broadcast(false);
		return;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		SetOnlineState(EBotanicusOnlineState::Idle);
		ReportError(NSLOCTEXT("BotanicusOnline", "MissingPlayerController", "Aucun joueur local ne peut rejoindre la partie."));
		OnSessionJoined.Broadcast(false);
		return;
	}

	SetOnlineState(EBotanicusOnlineState::Traveling);
	UE_LOG(LogBotanicus, Display, TEXT("Joining Steam session at %s"), *ConnectString);
	OnSessionJoined.Broadcast(true);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	SetOnlineState(EBotanicusOnlineState::Idle);
}

void UBotanicusMultiplayerSubsystem::HandleDestroySessionComplete(
	FName SessionName,
	bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
	}
	DestroySessionHandle.Reset();

	const EPendingAction CompletedAction = PendingAction;
	PendingAction = EPendingAction::None;
	SetOnlineState(EBotanicusOnlineState::Idle);

	if (!bWasSuccessful)
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "DestroyFailed", "Steam n’a pas pu fermer la session actuelle."));
		if (CompletedAction == EPendingAction::Leave)
		{
			OnSessionLeft.Broadcast(false);
		}
		return;
	}

	switch (CompletedAction)
	{
	case EPendingAction::Host:
		BeginCreateSession();
		break;

	case EPendingAction::Join:
		if (bHasPendingJoinResult)
		{
			BeginJoinSession(PendingJoinResult);
		}
		break;

	case EPendingAction::Leave:
		if (!PendingMenuMap.IsEmpty())
		{
			TravelToMap(PendingMenuMap, false);
		}
		OnSessionLeft.Broadcast(true);
		break;

	default:
		break;
	}
}

void UBotanicusMultiplayerSubsystem::HandleSessionInviteAccepted(
	bool bWasSuccessful,
	int32 LocalUserNum,
	FUniqueNetIdPtr UserId,
	const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "InvalidInvite", "L’invitation Steam n’est plus valide."));
		return;
	}

	if (OnlineState != EBotanicusOnlineState::Idle)
	{
		ReportError(NSLOCTEXT("BotanicusOnline", "BusyInvite", "Attendez la fin de l’opération réseau avant d’accepter une invitation."));
		return;
	}

	PendingJoinResult = InviteResult;
	bHasPendingJoinResult = true;

	if (HasActiveSession())
	{
		BeginDestroySession(EPendingAction::Join);
	}
	else
	{
		BeginJoinSession(PendingJoinResult);
	}
}

void UBotanicusMultiplayerSubsystem::ClearOperationDelegates()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		if (CreateSessionHandle.IsValid())
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
			CreateSessionHandle.Reset();
		}
		if (FindSessionsHandle.IsValid())
		{
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
			FindSessionsHandle.Reset();
		}
		if (JoinSessionHandle.IsValid())
		{
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
			JoinSessionHandle.Reset();
		}
		if (DestroySessionHandle.IsValid())
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
			DestroySessionHandle.Reset();
		}
	}
}
