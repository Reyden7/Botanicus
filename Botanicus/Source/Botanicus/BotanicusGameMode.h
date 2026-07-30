// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BotanicusGameMode.generated.h"

class AController;
class APlayerController;
class UBotanicusWorldSaveGame;

/**
 * Server-authoritative game mode with automatic world persistence.
 */
UCLASS(abstract)
class ABotanicusGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABotanicusGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Saves the current authoritative state immediately. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Exec, Category="Botanicus|Save")
	bool BotanicusSaveNow();

private:
	FString GetAutosaveSlotName() const;
	FString GetPlayerSaveKey(const AController* Controller) const;
	void LoadAutosave();
	void RestoreWorldState();
	void CapturePlayerInventory(const AController* Controller);
	void RestorePlayerInventory(AController* Controller);

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusWorldSaveGame> CurrentSaveGame;
};



