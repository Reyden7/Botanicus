// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BotanicusPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UBotanicusMultiplayerSubsystem;
class ACameraActor;
class AActor;
struct FInputKeyEventArgs;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class BOTANICUS_API ABotanicusPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	ABotanicusPlayerController();

	/** Development command: creates a Steam lobby on the current prototype map. */
	UFUNCTION(Exec)
	void BotanicusHost(int32 MaxPlayers = 4);

	/** Development command: searches for compatible Botanicus Steam lobbies. */
	UFUNCTION(Exec)
	void BotanicusFind();

	/** Development command: joins a result index printed by BotanicusFind. */
	UFUNCTION(Exec)
	void BotanicusJoin(int32 ResultIndex = 0);

	/** Development command: closes the current Steam session. */
	UFUNCTION(Exec)
	void BotanicusLeave();

	/** Development command: opens Steam's invite overlay. */
	UFUNCTION(Exec)
	void BotanicusInvite();

	/** Development command: prints the active subsystem and operation state. */
	UFUNCTION(Exec)
	void BotanicusOnlineStatus();

	/** Development command: validates whole-building grouping on the current map. */
	UFUNCTION(Exec)
	void BotanicusTestBuildingGrouping();

	/** Toggles the two-state Botanicus building camera. Bound to the T key. */
	UFUNCTION(BlueprintCallable, Exec, Category="Botanicus|Building")
	void ToggleBuildingTopDownView();

	/** True while the local player is using the free top-down building camera. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Building")
	bool IsBuildingTopDownViewActive() const { return bBuildingTopDownViewActive; }

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PlayerTick(float DeltaTime) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Consumes EBS' original V binding so the three-state camera cycle cannot run. */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	UBotanicusMultiplayerSubsystem* GetBotanicusMultiplayerSubsystem() const;

	void EnterBuildingTopDownView();
	void ExitBuildingTopDownView();
	void ForceFirstPersonView();
	void AdvanceEbsViewMode();
	void MoveBuildingCameraForward(float AxisValue);
	void MoveBuildingCameraRight(float AxisValue);
	void TrySelectBuildingGroup();
	void ConfirmBuildingGroupMove();
	void CancelBuildingGroupMove();
	void RotateBuildingGroup(float Direction);
	void UpdateBuildingGroupPreview(float DeltaTime);
	void SetBuildingGroupHighlighted(bool bHighlighted);
	bool TraceTopDownCursor(FHitResult& OutHit) const;
	bool FindLandscapeHeight(const FVector2D& WorldXY, float& OutHeight) const;

	UFUNCTION(Server, Reliable)
	void ServerBeginBuildingGroupMove(AActor* HitActor);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateBuildingGroupMove(FVector_NetQuantize10 NewPivotLocation, float NewYaw);

	UFUNCTION(Server, Reliable)
	void ServerConfirmBuildingGroupMove();

	UFUNCTION(Server, Reliable)
	void ServerCancelBuildingGroupMove();

	UFUNCTION(Client, Reliable)
	void ClientBeginBuildingGroupMove(
		const TArray<AActor*>& GroupActors,
		FVector_NetQuantize10 GroupPivot,
		float InitialYaw);

	UFUNCTION(Client, Reliable)
	void ClientEndBuildingGroupMove(bool bConfirmed);

	TArray<AActor*> BuildCompleteBuildingGroup(AActor* HitActor) const;
	bool IsEbsBuildingActor(const AActor* Actor) const;
	bool IsStructuralBuildingActor(const AActor* Actor) const;
	bool IsBuildingOwnedByThisPlayer(AActor* Actor) const;
	FVector CalculateBuildingGroupPivot(const TArray<AActor*>& GroupActors) const;
	void ApplyServerBuildingGroupTransform(const FVector& NewPivot, float NewYaw);
	void ClearServerBuildingGroupMove();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BuildingCameraActor;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="500.0"))
	float BuildingCameraHeight = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float BuildingCameraPanSpeed = 1400.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="0.0"))
	float BuildingCameraBlendTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="1.0"))
	float BuildingRotationStep = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="1.0"))
	float BuildingPreviewUpdatesPerSecond = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="1000.0"))
	float MaximumBuildingEditDistance = 30000.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalBuildingGroup;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ServerBuildingGroup;

	TArray<FTransform> LocalBuildingOriginalTransforms;
	TArray<FTransform> ServerBuildingOriginalTransforms;
	FVector LocalBuildingPivot = FVector::ZeroVector;
	FVector LocalBuildingOriginalPivot = FVector::ZeroVector;
	FVector ServerBuildingOriginalPivot = FVector::ZeroVector;
	float LocalBuildingYaw = 0.0f;
	float ServerBuildingInitialYaw = 0.0f;
	float LocalBuildingGroundOffset = 0.0f;
	float ServerBuildingGroundOffset = 0.0f;
	float BuildingPreviewUpdateAccumulator = 0.0f;
	bool bBuildingTopDownViewActive = false;
};
