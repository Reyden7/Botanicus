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

	virtual void InitGame(
		const FString& MapName,
		const FString& Options,
		FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Saves the current authoritative state immediately. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Exec, Category="Botanicus|Save")
	bool BotanicusSaveNow();

	void RegisterRemovedBuildingActor(FName ActorName);
	/** Coalesces inventory mutations into one save on the next server tick. */
	void ScheduleInventoryAutosave();

private:
	FString GetAutosaveSlotName() const;
	FString GetPlayerSaveKey(const AController* Controller);
	void LoadAutosave();
	void InitializeSharedEconomy();
	void RestoreWorldState();
	void CapturePlayerInventory(const AController* Controller);
	void RestorePlayerInventory(AController* Controller);
	void CapturePlayerEconomy(const AController* Controller);
	void RestorePlayerEconomy(AController* Controller);
	void EnsureStarterFixtures(AController* Controller);
	void FlushScheduledInventoryAutosave();
	void MaintainBrokenFlowerPots();
	bool FindBrokenFlowerPotSpawnTransform(
		FTransform& OutTransform) const;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusWorldSaveGame> CurrentSaveGame;

	TSet<FName> RemovedBuildingActorNames;
	TMap<TWeakObjectPtr<AController>, int32> PIERemotePlayerSlots;
	int32 NextPIERemotePlayerSlot = 0;
	bool bAutosaveReady = false;
	bool bInventoryAutosaveScheduled = false;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ambient Props",
		meta=(ClampMin="1", ClampMax="30"))
	int32 MinimumBrokenFlowerPots = 6;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ambient Props",
		meta=(ClampMin="10.0", Units="s"))
	float BrokenFlowerPotRespawnInterval = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ambient Props",
		meta=(ClampMin="500.0", Units="cm"))
	float BrokenFlowerPotSpawnRadius = 3000.0f;

	FTimerHandle BrokenFlowerPotSpawnTimer;
};



