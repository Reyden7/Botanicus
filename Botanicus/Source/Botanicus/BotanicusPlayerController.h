// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BotanicusPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UBotanicusMultiplayerSubsystem;
class UBotanicusQuickBarWidget;
class UBotanicusTopDownToolbarWidget;
class UBotanicusCarryProgressWidget;
class UBotanicusOrderCatalogWidget;
class UBotanicusBuildingCatalogWidget;
class ACameraActor;
class AActor;
class ABotanicusPathActor;
class ABotanicusCommunicationDoorActor;
class ABotanicusDeliveryParcelActor;
class ABotanicusDeliveryZoneActor;
class ABotanicusLargeEquipmentActor;
class ABotanicusPlaceableItemActor;
class ABotanicusPlantPotActor;
class UActorComponent;
class UMaterialInterface;
class UPrimitiveComponent;
struct FInputKeyEventArgs;

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusPendingOrder
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid OrderId;

	UPROPERTY(BlueprintReadOnly)
	FName ItemKey = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 ChargedPrice = 0;

	UPROPERTY(BlueprintReadOnly)
	float DeliveryServerTime = 0.0f;
};

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

	/** Development command: places one server-authoritative catalogue order. */
	UFUNCTION(Exec)
	void BotanicusOrder(FName ItemKey);

	/** Development command: validates whole-building grouping on the current map. */
	UFUNCTION(Exec)
	void BotanicusTestBuildingGrouping();

	/** Development command: places a ping in front of the controlled pawn. */
	UFUNCTION(Exec)
	void BotanicusTestPing();

	/** Toggles the two-state Botanicus building camera. Bound to the T key. */
	UFUNCTION(BlueprintCallable, Exec, Category="Botanicus|Building")
	void ToggleBuildingTopDownView();

	/** True while the local player is using the free top-down building camera. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Building")
	bool IsBuildingTopDownViewActive() const { return bBuildingTopDownViewActive; }

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void BeginPathPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void ConfirmPathPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void CancelPathPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void BeginPathDeletion();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void CancelPathDeletion();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building")
	void ToggleBuildingCatalog();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building")
	void PurchaseCatalogBuilding(FName BuildingKey);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Delivery")
	void ToggleOrderCatalog();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Delivery")
	void PlaceCatalogOrder(FName ItemKey);

	UFUNCTION(BlueprintPure, Category="Botanicus|Delivery")
	int32 GetAvailableFunds() const { return AvailableFunds; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Building")
	int32 GetBuildingProgressionLevel() const
	{
		return BuildingProgressionLevel;
	}

	int32 GetDefaultStartingFunds() const
	{
		return FMath::Max(0, StartingFunds);
	}

	const TArray<FBotanicusPendingOrder>& GetPendingOrders() const
	{
		return PendingOrders;
	}

	/**
	 * Server-only load hook. DeliveryServerTime in RestoredOrders contains a
	 * remaining duration and is converted back to server time here.
	 */
	void RestoreCatalogOrderState(
		int32 RestoredFunds,
		const TArray<FBotanicusPendingOrder>& RestoredOrders,
		int32 RestoredBuildingProgressionLevel = 1);

	/** Cancels and refunds an unconfirmed building before logout is saved. */
	void CancelPendingBuildingPurchaseForLogout();

	UFUNCTION(Client, Reliable)
	void ClientHideRemovedBuildingActors(
		const TArray<FName>& ActorNames);

	UFUNCTION(BlueprintPure, Category="Botanicus|Path")
	bool IsPathPlacementActive() const { return bPathPlacementActive; }

	/** Prevents hotbar shortcuts from colliding with construction controls. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Inventory")
	bool IsQuickBarInputBlocked() const;

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

	/** Temporary functional hotbar; its visual design can be replaced later. */
	UPROPERTY(Transient)
	TObjectPtr<UBotanicusQuickBarWidget> QuickBarWidget;

	bool bEbsDemoHudHidden = false;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
	void InitializeQuickBarWidget();
	void InitializeTopDownToolbarWidget();
	void InitializeOrderCatalogWidget();
	void InitializeBuildingCatalogWidget();
	void HideEbsDemoHud();
	void RefreshTopDownRoofVisibility();
	void RestoreTopDownRoofVisibility();
	void AdvanceEbsViewMode();
	void MoveBuildingCameraForward(float AxisValue);
	void MoveBuildingCameraRight(float AxisValue);
	void ZoomBuildingCamera(float Direction);
	void AddPathPointAtCursor();
	void RemoveLastPathPoint();
	void UpdatePathPreview();
	void RefreshTopDownToolbar();
	void TryDeletePathSegmentAtCursor();
	bool GetPathCursorPoint(FVector& OutPoint) const;
	bool SnapPathPoint(
		const FVector& RawPoint,
		FVector& OutSnappedPoint,
		ABotanicusPathActor*& OutConnectedPath) const;
	bool FindNearestBuildingEntrance(
		const FVector& RawPoint,
		FVector& OutEntrancePoint) const;
	ABotanicusPathActor* FindNearestExistingPath(
		const FVector& RawPoint,
		FVector& OutPathPoint) const;
	bool IsCursorOverTopDownToolbar() const;
	void TrySelectBuildingGroup();
	void ConfirmBuildingGroupMove();
	void CancelBuildingGroupMove();
	void RotateBuildingGroup(float Direction);
	void UpdateBuildingGroupPreview(float DeltaTime);
	void UpdateCommunicationDoorPreview();
	bool TryCollectNearbyDeliveryParcel();
	bool TryMoveNearbyPlaceableItem();
	void BeginPlaceableItemMoveCharge(
		ABotanicusPlaceableItemActor* WorldItem);
	void UpdatePlaceableItemMoveCharge(float DeltaTime);
	void CancelPlaceableItemMoveCharge();
	bool TryBeginNearbyPlantPotAction();
	void EndPlantPotAction();
	bool TryHandleNearbyLargeEquipment();
	void BeginEquipmentCarryCharge(
		ABotanicusLargeEquipmentActor* Equipment);
	void UpdateEquipmentCarryCharge(float DeltaTime);
	void CancelEquipmentCarryCharge(bool bNotifyServer);
	void BeginLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);
	void UpdateLargeEquipmentPlacement(float DeltaTime);
	void RotateLargeEquipmentPlacement(float Direction);
	void DrawLargeEquipmentAlignmentGuides(
		const FTransform& PlacementTransform) const;
	void ConfirmLargeEquipmentPlacement();
	void CancelLargeEquipmentPlacement();
	bool ResolveLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment,
		const FVector& RequestedLocation,
		float RequestedYaw,
		FTransform& OutTransform) const;
	void BeginQuickBarItemPlacement();
	void BeginWorldItemMove(
		ABotanicusPlaceableItemActor* WorldItem);
	void UpdateQuickBarItemPlacement(float DeltaTime);
	void RotateQuickBarItemPlacement(float Direction);
	void ConfirmQuickBarItemPlacement();
	void CancelQuickBarItemPlacement();
	bool ResolveQuickBarItemPlacement(
		FName ItemKey,
		const FVector& RequestedLocation,
		float RequestedYaw,
		FTransform& OutTransform,
		const AActor* IgnoredWorldItem = nullptr) const;
	void DrawQuickBarItemAlignmentGuides(
		const FTransform& PlacementTransform) const;
	bool IsLookingAtWorldItem(
		const AActor* Item,
		float MaximumDistance) const;
	ABotanicusDeliveryZoneActor* FindOrCreateDeliveryZone();
	bool DeliverCatalogItem(
		FName ItemKey,
		int32 Quantity);
	void CompleteCatalogOrder(FGuid OrderId);
	void RefreshOrderCatalog();
	void SpawnTestLargeEquipment(FName ItemKey);
	bool FindBuildingConnectionSnap(
		const TArray<TObjectPtr<AActor>>& MovingGroup,
		FVector& OutCorrection,
		AActor*& OutMovingWall,
		AActor*& OutExistingWall) const;
	bool BuildCommunicationDoorCandidates(
		const TArray<AActor*>& PurchasedGroup);
	bool IsPlainBuildingWall(const AActor* Actor) const;
	bool TryPlacePing();
	bool IsEbsConstructionModeActive(UActorComponent*& OutBuildingComponent) const;
	void SetBuildingGroupHighlighted(bool bHighlighted);
	void UpdateBuildingGroupPlacementVisual(bool bPlacementValid);
	bool TraceTopDownCursor(
		FHitResult& OutHit,
		bool* bOutOnLandscape = nullptr) const;
	bool FindLandscapeHeight(const FVector2D& WorldXY, float& OutHeight) const;

	UFUNCTION(Server, Reliable)
	void ServerBeginBuildingGroupMove(AActor* HitActor);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateBuildingGroupMove(FVector_NetQuantize10 NewPivotLocation, float NewYaw);

	UFUNCTION(Server, Reliable)
	void ServerConfirmBuildingGroupMove();

	UFUNCTION(Server, Reliable)
	void ServerCancelBuildingGroupMove();

	UFUNCTION(Server, Reliable)
	void ServerPurchaseCatalogBuilding(FName BuildingKey);

	UFUNCTION(Server, Reliable)
	void ServerCollectDeliveryParcel(
		ABotanicusDeliveryParcelActor* Parcel);

	UFUNCTION(Server, Reliable)
	void ServerBeginPlantPotAction(
		ABotanicusPlantPotActor* PlantPot);

	UFUNCTION(Server, Reliable)
	void ServerEndPlantPotAction(
		ABotanicusPlantPotActor* PlantPot);

	UFUNCTION(Server, Reliable)
	void ServerPlaceCatalogOrder(FName ItemKey);

	/** Legacy implementations kept binary-local while old test entry points are retired. */
	void ServerOrderTestDelivery_Implementation();
	void ServerOrderTestLargeEquipment_Implementation();
	void ServerOrderTestSoloEquipment_Implementation();

	UFUNCTION()
	void OnRep_OrderState();

	UFUNCTION(Server, Reliable)
	void ServerToggleCarryLargeEquipment(
		ABotanicusLargeEquipmentActor* Equipment);

	UFUNCTION(Server, Reliable)
	void ServerSetHeavyEquipmentHold(
		ABotanicusLargeEquipmentActor* Equipment,
		bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerBeginLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw);

	UFUNCTION(Server, Reliable)
	void ServerConfirmLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);

	UFUNCTION(Server, Reliable)
	void ServerCancelLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);

	UFUNCTION(Client, Reliable)
	void ClientBeginLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);

	UFUNCTION(Client, Reliable)
	void ClientCancelHeavyEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);

	UFUNCTION(Server, Reliable)
	void ServerPlaceQuickBarItem(
		int32 SlotIndex,
		FGuid InstanceId,
		FName ItemKey,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw);

	UFUNCTION(Server, Reliable)
	void ServerBeginPlaceableItemMove(
		ABotanicusPlaceableItemActor* WorldItem);

	UFUNCTION(Server, Reliable)
	void ServerConfirmPlaceableItemMove(
		ABotanicusPlaceableItemActor* WorldItem,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw);

	UFUNCTION(Server, Reliable)
	void ServerCancelPlaceableItemMove(
		ABotanicusPlaceableItemActor* WorldItem);

	UFUNCTION(Server, Reliable)
	void ServerConfirmCommunicationDoor(int32 CandidateIndex);

	UFUNCTION(Server, Reliable)
	void ServerCancelCommunicationDoor();

	UFUNCTION(Client, Reliable)
	void ClientBeginCommunicationDoorPlacement(
		const TArray<FVector_NetQuantize10>& CandidateLocations,
		const TArray<float>& CandidateYaws);

	UFUNCTION(Client, Reliable)
	void ClientEndCommunicationDoorPlacement(bool bCreated);

	UFUNCTION(Client, Reliable)
	void ClientBeginBuildingGroupMove(
		const TArray<AActor*>& GroupActors,
		FVector_NetQuantize10 GroupPivot,
		float InitialYaw);

	UFUNCTION(Client, Reliable)
	void ClientEndBuildingGroupMove(bool bConfirmed);

	UFUNCTION(Client, Unreliable)
	void ClientUpdateBuildingPlacementValidity(bool bPlacementValid);

	UFUNCTION(Client, Reliable)
	void ClientApplyPurchasedBuildingSnapshot(
		const TArray<FName>& ActorNames,
		const TArray<FTransform>& ActorTransforms);

	UFUNCTION(Client, Unreliable)
	void ClientApplyPurchasedBuildingPreviewSnapshot(
		const TArray<FName>& ActorNames,
		const TArray<FTransform>& ActorTransforms);

	UFUNCTION(Client, Reliable)
	void ClientCancelPurchasedBuildingSnapshot();

	UFUNCTION(Server, Reliable)
	void ServerPlacePing(FVector_NetQuantize10 RequestedLocation);

	UFUNCTION(Server, Reliable)
	void ServerCreatePath(
		const TArray<FVector_NetQuantize10>& RequestedPoints);

	UFUNCTION(Server, Reliable)
	void ServerDeletePathSegment(
		ABotanicusPathActor* Path,
		int32 SegmentIndex,
		FVector_NetQuantize10 RequestedHitLocation);

	TArray<AActor*> BuildCompleteBuildingGroup(AActor* HitActor) const;
	bool IsEbsBuildingActor(const AActor* Actor) const;
	bool IsStructuralBuildingActor(const AActor* Actor) const;
	bool IsBuildingOwnedByThisPlayer(AActor* Actor) const;
	bool TryAcquireBuildingGroupLock(const TArray<AActor*>& GroupActors);
	void ReleaseBuildingGroupLock();
	FVector CalculateBuildingGroupPivot(const TArray<AActor*>& GroupActors) const;
	void ApplyServerBuildingGroupTransform(const FVector& NewPivot, float NewYaw);
	void BroadcastPurchasedBuildingSnapshot(bool bReliable);
	void QueuePurchasedBuildingSnapshot(
		const TArray<FName>& ActorNames,
		const TArray<FTransform>& ActorTransforms);
	void TryApplyPurchasedBuildingSnapshot();
	bool IsServerBuildingGroupPlacementValid() const;
	void ClearServerBuildingGroupMove();
	void RefundPendingBuildingPurchase();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BuildingCameraActor;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusTopDownToolbarWidget> TopDownToolbarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusOrderCatalogWidget> OrderCatalogWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusBuildingCatalogWidget> BuildingCatalogWidget;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPathActor> PathPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusCommunicationDoorActor>
		CommunicationDoorPreviewActor;

	TSet<TWeakObjectPtr<UPrimitiveComponent>>
		TopDownHiddenRoofComponents;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="500.0"))
	float BuildingCameraHeight = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float MinimumBuildingCameraHeight = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float MaximumBuildingCameraHeight = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="10.0"))
	float BuildingCameraZoomStep = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float BuildingCameraPanSpeed = 1400.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="0.0"))
	float BuildingCameraBlendTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="1.0"))
	float BuildingRotationStep = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="1.0"))
	float BuildingPreviewUpdatesPerSecond = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="25.0"))
	float BuildingConnectionSnapDistance = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="10.0"))
	float BuildingConnectionOverlapDepth = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Editing", meta=(ClampMin="1000.0"))
	float MaximumBuildingEditDistance = 30000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ping", meta=(ClampMin="100.0"))
	float MaximumPingDistance = 30000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ping", meta=(ClampMin="0.1"))
	float PingLifeTime = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ping", meta=(ClampMin="0.1"))
	float PingCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="1.0", ClampMax="60.0"))
	float WorldItemLookAngle = 22.0f;

	UPROPERTY(EditDefaultsOnly, Config, Category="Botanicus|Delivery", meta=(ClampMin="0"))
	int32 StartingFunds = 2000;

	UPROPERTY(
		EditDefaultsOnly,
		Config,
		Category="Botanicus|Building Progression",
		meta=(ClampMin="0"))
	int32 DevelopmentLevelRewardCredits = 500;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	int32 AvailableFunds = 0;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	TArray<FBotanicusPendingOrder> PendingOrders;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	int32 BuildingProgressionLevel = 1;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="0.1", ClampMax="5.0"))
	float EquipmentLiftHoldDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="0.1", ClampMax="5.0"))
	float PlaceableItemMoveHoldDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="50.0"))
	float EquipmentPlacementDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="1.0"))
	float EquipmentRotationStep = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="0.1"))
	float FineEquipmentRotationStep = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="0.1"))
	float EquipmentAlignmentGuideTolerance = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="0.1", ClampMax="10.0"))
	float EquipmentAlignmentAngleTolerance = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="50.0"))
	float EquipmentAlignmentGuideDistance = 800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="100.0"))
	float MaximumEquipmentPlacementDistance = 650.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Placement", meta=(ClampMin="50.0"))
	float QuickBarItemPlacementDistance = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Placement", meta=(ClampMin="100.0"))
	float MaximumQuickBarItemPlacementDistance = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Path", meta=(ClampMin="25.0"))
	float BuildingEntranceSnapDistance = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Path", meta=(ClampMin="25.0"))
	float ExistingPathSnapDistance = 260.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ValidBuildingPlacementMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InvalidBuildingPlacementMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalBuildingGroup;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ServerBuildingGroup;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusLargeEquipmentActor>
		LocalLargeEquipmentPlacement;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusLargeEquipmentActor>
		LocalHeldHeavyEquipment;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusCarryProgressWidget>
		CarryProgressWidget;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalQuickBarItemPreview;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalMovedPlaceableItem;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalPlaceableItemMoveCandidate;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		ServerMovedPlaceableItem;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlantPotActor>
		LocalActivePlantPot;

	TArray<FTransform> LocalBuildingOriginalTransforms;
	TArray<FTransform> ServerBuildingOriginalTransforms;
	TObjectPtr<AActor> ServerSnappedMovingWall;
	TObjectPtr<AActor> ServerSnappedExistingWall;
	TArray<TObjectPtr<AActor>> ServerDoorCandidatePurchasedWalls;
	TArray<TObjectPtr<AActor>> ServerDoorCandidateExistingWalls;
	TArray<FVector_NetQuantize10> ServerDoorCandidateLocations;
	TArray<float> ServerDoorCandidateYaws;
	TArray<FVector_NetQuantize10> LocalDoorCandidateLocations;
	TArray<float> LocalDoorCandidateYaws;
	FVector LocalBuildingPivot = FVector::ZeroVector;
	FVector LocalBuildingOriginalPivot = FVector::ZeroVector;
	FVector ServerBuildingOriginalPivot = FVector::ZeroVector;
	float LocalBuildingYaw = 0.0f;
	float ServerBuildingInitialYaw = 0.0f;
	float LocalBuildingGroundOffset = 0.0f;
	float ServerBuildingGroundOffset = 0.0f;
	float BuildingPreviewUpdateAccumulator = 0.0f;
	float LargeEquipmentPlacementYaw = 0.0f;
	float LargeEquipmentPreviewUpdateAccumulator = 0.0f;
	float QuickBarItemPlacementYaw = 0.0f;
	float QuickBarItemPreviewUpdateAccumulator = 0.0f;
	float EquipmentCarryChargeElapsed = 0.0f;
	float PlaceableItemMoveChargeElapsed = 0.0f;
	float TopDownRoofRefreshAccumulator = 0.0f;
	bool bLocalLargeEquipmentPlacementValid = false;
	bool bLocalQuickBarItemPlacementValid = false;
	bool bEquipmentCarryHoldActivated = false;
	bool bEquipmentCarryKeyHeld = false;
	bool bPlantPotActionHeld = false;
	bool bCatalogOrderStateRestored = false;
	int32 LocalQuickBarItemSlotIndex = INDEX_NONE;
	FGuid LocalQuickBarItemInstanceId;
	FName LocalQuickBarItemKey = NAME_None;
	double LastServerPingTime = -1000.0;
	bool bLocalBuildingPlacementValid = true;
	bool bServerBuildingPlacementValid = true;
	bool bServerBuildingPurchasePlacement = false;
	int32 ServerPendingBuildingPurchasePrice = 0;
	FName ServerPendingBuildingPurchaseKey = NAME_None;
	bool bBuildingTopDownViewActive = false;
	bool bPathPlacementActive = false;
	bool bPathDeletionActive = false;
	bool bCommunicationDoorPlacementActive = false;
	bool bAzertyForwardPressed = false;
	bool bAzertyBackwardPressed = false;
	bool bAzertyLeftPressed = false;
	bool bAzertyRightPressed = false;
	int32 LocalCommunicationDoorCandidateIndex = INDEX_NONE;
	TArray<FVector> PendingPathPoints;
	TArray<FName> PendingPurchasedBuildingActorNames;
	TArray<FTransform> PendingPurchasedBuildingTransforms;
	FTimerHandle PurchasedBuildingSnapshotRetryTimer;
	TMap<FGuid, FTimerHandle> PendingOrderTimers;
	int32 PurchasedBuildingSnapshotRetryCount = 0;
};
