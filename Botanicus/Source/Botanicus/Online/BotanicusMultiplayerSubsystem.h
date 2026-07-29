// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BotanicusMultiplayerSubsystem.generated.h"

UENUM(BlueprintType)
enum class EBotanicusOnlineState : uint8
{
	Idle,
	Creating,
	Searching,
	Joining,
	Destroying,
	Traveling
};

/** Safe, Blueprint-facing summary of an internal online session search result. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Online")
	int32 ResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Online")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category="Online")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category="Online")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Online")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Online")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category="Online")
	bool bJoinable = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBotanicusOnlineStateChangedSignature,
	EBotanicusOnlineState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBotanicusOnlineErrorSignature,
	FText, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FBotanicusSessionsFoundSignature,
	bool, bWasSuccessful,
	const TArray<FBotanicusSessionInfo>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBotanicusOnlineResultSignature,
	bool, bWasSuccessful);

/**
 * Steam session backend shared by menus and gameplay.
 *
 * Uses UE's production-proven Online Subsystem API. Steam lobby search results stay
 * private to C++; UMG receives stable summaries and joins by their displayed index.
 */
UCLASS()
class BOTANICUS_API UBotanicusMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Botanicus|Online")
	EBotanicusOnlineState GetOnlineState() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Online")
	bool IsSteamAvailable() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Online")
	bool HasActiveSession() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Online")
	FString GetOnlineSubsystemName() const;

	/**
	 * Creates a Steam lobby and opens LobbyMap as a listen server.
	 * LobbyMap must use a package path such as /Game/FirstPerson/Lvl_FirstPerson.
	 */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Online")
	void HostSession(
		int32 MaxPlayers = 4,
		const FString& LobbyMap = TEXT("/Game/FirstPerson/Lvl_FirstPerson"));

	UFUNCTION(BlueprintCallable, Category="Botanicus|Online")
	void FindSessions(int32 MaxResults = 100);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Online")
	void JoinSessionByIndex(int32 ResultIndex);

	/** Destroys the active session and optionally opens a menu map. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Online")
	void LeaveSession(const FString& MenuMap = FString());

	/** Opens Steam's native friend invitation overlay for the active session. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Online")
	bool ShowSteamInviteOverlay();

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Online")
	FBotanicusOnlineStateChangedSignature OnOnlineStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Online")
	FBotanicusOnlineErrorSignature OnOnlineError;

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Online")
	FBotanicusOnlineResultSignature OnSessionCreated;

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Online")
	FBotanicusSessionsFoundSignature OnSessionsFound;

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Online")
	FBotanicusOnlineResultSignature OnSessionJoined;

	UPROPERTY(BlueprintAssignable, Category="Botanicus|Online")
	FBotanicusOnlineResultSignature OnSessionLeft;

private:
	enum class EPendingAction : uint8
	{
		None,
		Host,
		Join,
		Leave
	};

	static const FName BotanicusBuildSetting;
	static const FString BotanicusBuildValue;

	EBotanicusOnlineState OnlineState = EBotanicusOnlineState::Idle;
	EPendingAction PendingAction = EPendingAction::None;

	FString PendingHostMap;
	FString PendingMenuMap;
	int32 PendingMaxPlayers = 4;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FOnlineSessionSearchResult PendingJoinResult;
	bool bHasPendingJoinResult = false;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle DestroySessionHandle;
	FDelegateHandle InviteAcceptedHandle;

	IOnlineSessionPtr GetSessionInterface() const;
	void SetOnlineState(EBotanicusOnlineState NewState);
	void ReportError(const FText& Message);
	bool EnsureIdle(const FText& OperationName);

	void BeginCreateSession();
	void BeginJoinSession(const FOnlineSessionSearchResult& SearchResult);
	void BeginDestroySession(EPendingAction Action);
	void TravelToMap(const FString& MapPath, bool bListenServer);

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(
		FName SessionName,
		EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleSessionInviteAccepted(
		bool bWasSuccessful,
		int32 LocalUserNum,
		FUniqueNetIdPtr UserId,
		const FOnlineSessionSearchResult& InviteResult);

	void ClearOperationDelegates();
};
