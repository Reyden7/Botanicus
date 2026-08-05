// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Path/BotanicusPathActor.h"
#include "BotanicusPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UBotanicusMultiplayerSubsystem;
class UBotanicusQuickBarComponent;
class UBotanicusQuickBarWidget;
class UBotanicusTopDownToolbarWidget;
class UBotanicusCarryProgressWidget;
class UBotanicusOrderCatalogWidget;
class UBotanicusWorkbenchUpgradeWidget;
class UBotanicusDevelopmentPanelWidget;
class UBotanicusBuildingCatalogWidget;
class UBotanicusSharedFundsWidget;
class UBotanicusShopObjectivesWidget;
class UBotanicusDaySummaryWidget;
class UBotanicusClockWidget;
class UBotanicusStorageQuantityWidget;
class UBotanicusInteractionTargetWidget;
class UTextRenderComponent;
class UBotanicusThrowPowerWidget;
class ABotanicusGameState;
class ACameraActor;
class AActor;
class ABotanicusPathActor;
class ABotanicusCommunicationDoorActor;
class ABotanicusDeliveryParcelActor;
class ABotanicusDeliveryZoneActor;
class ABotanicusLargeEquipmentActor;
class ABotanicusPlaceableItemActor;
class ABotanicusPlantPotActor;
class ABotanicusSalesDisplayActor;
struct FBotanicusQuickBarSlot;
class ABotanicusSalePotActor;
class ABotanicusCashRegisterActor;
class ABotanicusWateringCanActor;
class ABotanicusWaterReserveActor;
class ABotanicusComputerActor;
class ABotanicusStorageShelfActor;
class ABotanicusPreparationWorkbenchActor;
class UActorComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
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

	UFUNCTION(BlueprintPure, Category="Botanicus|Furniture")
	bool IsFurnitureMoveModeActive() const
	{
		return bFurnitureMoveModeActive;
	}

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void BeginPathPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Visitors")
	void BeginVisitorRoutePlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Visitors")
	void BeginVisitorParkingPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Visitors")
	void BeginVisitorSalesAreaPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Visitors")
	void BeginVisitorCheckoutPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Economy")
	void BeginRefundZonePlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Delivery")
	void BeginDeliveryZonePlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void ConfirmPathPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void CancelPathPlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void BeginPathDeletion();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Path")
	void CancelPathDeletion();
	void CancelVisitorZonePlacement();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building")
	void BeginDoorEditing();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building")
	void CancelDoorEditing();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building")
	void ToggleBuildingCatalog();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Building")
	void PurchaseCatalogBuilding(FName BuildingKey);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Delivery")
	void ToggleOrderCatalog();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Development")
	void ToggleDevelopmentPanel();

	UFUNCTION(Client, Reliable)
	void ClientOpenOrderCatalogFromComputer();

	UFUNCTION(Client, Reliable)
	void ClientOpenPreparationWorkbenchUpgrade(
		ABotanicusPreparationWorkbenchActor* Workbench);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Preparation")
	void ClosePreparationWorkbenchUpgrade();

	void RequestPreparationWorkbenchLevel(
		ABotanicusPreparationWorkbenchActor* Workbench,
		int32 TargetLevel);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Delivery")
	void PlaceCatalogOrder(FName ItemKey);

	UFUNCTION(BlueprintPure, Category="Botanicus|Delivery")
	int32 GetAvailableFunds() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopLevel() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetSelfCheckoutLimit() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetSelfCheckoutOwnedOrOrderedCount() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Delivery")
	bool CanOrderCatalogItem(FName ItemKey) const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	bool IsMainShopOpen() const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Shop")
	void ToggleMainShopOpen();

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopVisitorCapacity() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTargetVisitorPopulation() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopUpgradeCost() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopRequiredPlantSales() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetMainShopRequiredCatalogOrders() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTotalPlantsSold() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	int32 GetTotalCatalogOrders() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Shop")
	bool CanUpgradeMainShop() const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Shop")
	void UpgradeMainShop();

	UFUNCTION(BlueprintPure, Category="Botanicus|Preparation")
	int32 GetPreparationWorkbenchLevel() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Preparation")
	int32 GetPreparationWorkbenchUpgradeCost() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Preparation")
	bool CanUpgradePreparationWorkbench() const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Preparation")
	void UpgradePreparationWorkbench();

	/** Development helper exposed by the command panel. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Development")
	void AddTestCredits();

	UFUNCTION(BlueprintPure, Category="Botanicus|Development")
	float GetDevelopmentTimeScale() const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Development")
	void CycleDevelopmentTimeScale();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Development")
	void AdjustMainShopLevelForDevelopment(int32 Delta);

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

	/** Server-only reward issued when a prepared plant is sold. */
	void CreditPlantSale(FName PlantItemKey, int32 SalePrice);

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
	bool bQuickBarReorganizationMode = false;

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
	void ToggleQuickBarReorganizationMode();
	void InitializeSharedFundsWidget();
	void InitializeShopObjectivesWidget();
	void InitializeDaySummaryWidget();
	void InitializeClockWidget();
	void InitializeStorageQuantityWidget();
	void InitializeInteractionTargetWidget();
	void InitializeTopDownToolbarWidget();
	void InitializeOrderCatalogWidget();
	void InitializeWorkbenchUpgradeWidget();
	void InitializeDevelopmentPanelWidget();
	void InitializeBuildingCatalogWidget();
	void BeginPathPlacementInternal(EBotanicusPathType PathType);
	void BeginVisitorZonePlacement(int32 ZoneType);
	void PlaceVisitorZoneAtCursor();
	void HideEbsDemoHud();
	void RefreshTopDownRoofVisibility();
	void RestoreTopDownRoofVisibility();
	void RefreshTopDownBuildingLabels(bool bShowLabels);
	void HideTopDownBuildingLabels();
	float GetTopDownBuildingViewDistance() const;
	FText ResolveTopDownBuildingName(AActor* BuildingActor) const;
	void AdvanceEbsViewMode();
	void MoveBuildingCameraForward(float AxisValue);
	void MoveBuildingCameraRight(float AxisValue);
	void ZoomBuildingCamera(float Direction);
	void BeginBuildingCameraOrbit();
	void EndBuildingCameraOrbit();
	void UpdateBuildingCameraOrbit();
	void InitializeBuildingCameraOrbitFromCurrentView();
	void ApplyBuildingCameraOrbit();
	void BeginBuildingCameraFreeLook();
	void EndBuildingCameraFreeLook();
	void UpdateBuildingCameraFreeLook();
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
	void BeginParcelMoveCharge(
		ABotanicusDeliveryParcelActor* Parcel);
	void UpdateParcelMoveCharge(float DeltaTime);
	void CancelParcelMoveCharge();
	void BeginDeliveryParcelMove(
		ABotanicusDeliveryParcelActor* Parcel);
	void UpdateDeliveryParcelPlacement(float DeltaTime);
	void RotateDeliveryParcelPlacement(float Direction);
	void ConfirmDeliveryParcelPlacement();
	void CancelDeliveryParcelPlacement();
	bool ResolveDeliveryParcelPlacement(
		const ABotanicusDeliveryParcelActor* Parcel,
		const FVector& RequestedLocation,
		float RequestedYaw,
		FTransform& OutTransform) const;
	bool TryHandleNearbyWateringCan();
	bool TryRefillHeldWateringCan();
	void UpdateWateringCanRefill(float DeltaTime);
	void EndWateringCanRefill(bool bNotifyServer);
	bool TryOpenNearbyWorkbenchUpgrade();
	bool TryUseNearbyComputer();
	bool TryMoveNearbyPlaceableItem();
	void BeginPlaceableItemMoveCharge(
		ABotanicusPlaceableItemActor* WorldItem);
	void UpdatePlaceableItemMoveCharge(float DeltaTime);
	void CancelPlaceableItemMoveCharge();
	bool TryBeginNearbyPlantPotAction();
	bool TryBeginNearbySalePotAction();
	bool TryPlaceSelectedSalePotOnWorkbench();
	bool TryBeginNearbyParcelCut();
	void EndParcelCut();
	bool TryPlacePlantOnNearbySalesDisplay();
	bool TryCheckoutNearbyRegister();
	void EndPlantPotAction();
	bool TryHandleNearbyLargeEquipment();
	void BeginEquipmentCarryCharge(
		ABotanicusLargeEquipmentActor* Equipment);
	void UpdateEquipmentCarryCharge(float DeltaTime);
	void CancelEquipmentCarryCharge(bool bNotifyServer);
	void ToggleFurnitureMoveMode();
	void RefreshFurnitureHighlights();
	bool IsFurnitureActor(const AActor* Actor) const;
	void SetFurnitureActorHighlighted(
		AActor* Actor,
		bool bHighlighted) const;
	void RefreshInteractionTargetHighlight();
	void RefreshInteractionTargetName(AActor* TargetActor);
	void SetInteractionTargetHighlighted(
		AActor* Actor,
		bool bHighlighted) const;
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
	void ReleaseQuickBarThrowCharge();
	void ThrowSelectedQuickBarItem(float HoldDuration);
	void InitializeThrowPowerWidget();
	void UpdateThrowPowerWidget();
	void HideThrowPowerWidget();
	void ApplyCarriedItemState(
		const FBotanicusQuickBarSlot& Slot,
		ABotanicusPlaceableItemActor* Item) const;
	void UpdateEquippedQuickBarItem();
	void DestroyEquippedQuickBarItem();

	UFUNCTION()
	void HandleEquippedQuickBarChanged();

	void BeginWorldItemMove(
		ABotanicusPlaceableItemActor* WorldItem);
	void UpdateQuickBarItemPlacement(float DeltaTime);
	void RotateQuickBarItemPlacement(float Direction);
	void AdjustQuickBarPlacementQuantity(int32 Direction);
	int32 GetMaximumQuickBarPlacementQuantity(
		ABotanicusStorageShelfActor* Shelf,
		int32 SlotIndex) const;
	void ConfirmQuickBarItemPlacement();
	void CancelQuickBarItemPlacement();
	void BeginStorageCollectionQuantitySelection(
		ABotanicusPlaceableItemActor* WorldItem);
	void TryBeginCollectedItemInspection();
	void AdjustStorageCollectionQuantity(int32 Direction);
	void ConfirmStorageCollectionQuantitySelection();
	void CancelStorageCollectionQuantitySelection(
		bool bNotifyServer);
	void RefreshStorageQuantityWidget(
		bool bStoring,
		FName ItemKey,
		int32 Quantity,
		int32 MaximumQuantity);
	bool ResolveQuickBarItemPlacement(
		FName ItemKey,
		const FVector& RequestedLocation,
		float RequestedYaw,
		FTransform& OutTransform,
		const AActor* IgnoredWorldItem = nullptr,
		int32 RequestedQuantity = 1) const;
	bool FindAimedStorageSlot(
		ABotanicusStorageShelfActor*& OutShelf,
		int32& OutSlotIndex) const;
	bool FindStorageDestinationAtLocation(
		FName ItemKey,
		int32 ItemQuantity,
		const FVector& ItemExtent,
		const FVector& WorldLocation,
		const AActor* IgnoredWorldItem,
		ABotanicusStorageShelfActor*& OutShelf,
		int32& OutSlotIndex,
		ABotanicusPlaceableItemActor*& OutExistingStack) const;
	void DrawQuickBarItemAlignmentGuides(
		const FTransform& PlacementTransform) const;
	bool IsLookingAtWorldItem(
		const AActor* Item,
		float MaximumDistance) const;
	float GetMoveHoldDurationForActor(
		const AActor* Actor,
		float DefaultDuration) const;
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
	bool BuildManualDoorCandidates(AActor* SelectedActor);
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
	void ServerBeginDeliveryParcelMove(
		ABotanicusDeliveryParcelActor* Parcel);

	UFUNCTION(Server, Reliable)
	void ServerConfirmDeliveryParcelMove(
		ABotanicusDeliveryParcelActor* Parcel,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw);

	UFUNCTION(Server, Reliable)
	void ServerCancelDeliveryParcelMove(
		ABotanicusDeliveryParcelActor* Parcel);

	UFUNCTION(Server, Reliable)
	void ServerBeginParcelCut(
		ABotanicusDeliveryParcelActor* Parcel);

	UFUNCTION(Server, Reliable)
	void ServerEndParcelCut(
		ABotanicusDeliveryParcelActor* Parcel);

	UFUNCTION(Server, Reliable)
	void ServerToggleWateringCan(
		ABotanicusWateringCanActor* WateringCan);

	UFUNCTION(Server, Reliable)
	void ServerRefillWateringCan(
		ABotanicusWaterReserveActor* WaterReserve);

	UFUNCTION(Server, Reliable)
	void ServerEndWateringCanRefill(
		ABotanicusWaterReserveActor* WaterReserve);

	UFUNCTION(Server, Reliable)
	void ServerBeginPlantPotAction(
		ABotanicusPlantPotActor* PlantPot);

	UFUNCTION(Server, Reliable)
	void ServerEndPlantPotAction(
		ABotanicusPlantPotActor* PlantPot);

	UFUNCTION(Server, Reliable)
	void ServerBeginSalePotAction(
		ABotanicusSalePotActor* SalePot);

	UFUNCTION(Server, Reliable)
	void ServerEndSalePotAction(
		ABotanicusSalePotActor* SalePot);

	UFUNCTION(Server, Reliable)
	void ServerPlacePlantOnSalesDisplay(
		ABotanicusSalesDisplayActor* SalesDisplay);

	UFUNCTION(Server, Reliable)
	void ServerCheckoutRegister(
		ABotanicusCashRegisterActor* CashRegister);

	UFUNCTION(Server, Reliable)
	void ServerPlaceCatalogOrder(FName ItemKey);

	UFUNCTION(Server, Reliable)
	void ServerUpgradeMainShop();

	UFUNCTION(Server, Reliable)
	void ServerUpgradePreparationWorkbench();

	UFUNCTION(Server, Reliable)
	void ServerUseWorkbenchUpgradeTerminal(
		ABotanicusPreparationWorkbenchActor* Workbench);

	UFUNCTION(Server, Reliable)
	void ServerSetPreparationWorkbenchLevel(
		ABotanicusPreparationWorkbenchActor* Workbench,
		int32 TargetLevel);

	UFUNCTION(Server, Reliable)
	void ServerSetMainShopOpen(bool bOpen);

	UFUNCTION(Server, Reliable)
	void ServerAddTestCredits();

	UFUNCTION(Server, Reliable)
	void ServerSetDevelopmentTimeScale(float TimeScale);

	UFUNCTION(Server, Reliable)
	void ServerAdjustMainShopLevelForDevelopment(int32 Delta);

	UFUNCTION(Server, Reliable)
	void ServerUseComputer(ABotanicusComputerActor* Computer);

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
	void ServerSetFurnitureMoveMode(bool bEnabled);

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
		ABotanicusLargeEquipmentActor* Equipment,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw);

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
		float RequestedYaw,
		int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerThrowQuickBarItem(
		int32 SlotIndex,
		FGuid InstanceId,
		FName ItemKey,
		FVector_NetQuantizeNormal ThrowDirection,
		float HoldDuration);

	UFUNCTION(Server, Reliable)
	void ServerBeginPlaceableItemMove(
		ABotanicusPlaceableItemActor* WorldItem);

	UFUNCTION(Server, Reliable)
	void ServerCollectStorageItem(
		ABotanicusPlaceableItemActor* WorldItem,
		int32 Quantity);

	UFUNCTION(Client, Reliable)
	void ClientBeginCollectedItemInspection(
		int32 SlotIndex,
		FName ItemKey);

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
	void ServerBeginManualDoorPlacement(AActor* SelectedActor);

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
		const TArray<FVector_NetQuantize10>& RequestedPoints,
		uint8 RequestedPathType);

	UFUNCTION(Server, Reliable)
	void ServerCreateVisitorZone(
		FVector_NetQuantize10 RequestedLocation,
		uint8 RequestedZoneType);

	UFUNCTION(Server, Reliable)
	void ServerCreateRefundZone(
		FVector_NetQuantize10 RequestedLocation);

	UFUNCTION(Server, Reliable)
	void ServerCreateDeliveryZone(
		FVector_NetQuantize10 RequestedLocation);

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
	TObjectPtr<UBotanicusWorkbenchUpgradeWidget>
		WorkbenchUpgradeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusDevelopmentPanelWidget>
		DevelopmentPanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusBuildingCatalogWidget> BuildingCatalogWidget;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPathActor> PathPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusCommunicationDoorActor>
		CommunicationDoorPreviewActor;

	TSet<TWeakObjectPtr<UPrimitiveComponent>>
		TopDownHiddenRoofComponents;

	TMap<
		TWeakObjectPtr<AActor>,
		TWeakObjectPtr<UTextRenderComponent>>
		TopDownBuildingLabels;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="500.0"))
	float BuildingCameraHeight = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float MinimumBuildingCameraHeight = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float MaximumBuildingCameraHeight = 25000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="10.0"))
	float BuildingCameraZoomStep = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="0.01"))
	float BuildingCameraOrbitSensitivity = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="100.0"))
	float BuildingDetailViewDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="1.0", ClampMax="89.0"))
	float MinimumBuildingCameraOrbitPitch = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="1.0", ClampMax="89.0"))
	float MaximumBuildingCameraOrbitPitch = 85.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="-89.0", ClampMax="-1.0"))
	float MinimumBuildingCameraFreeLookPitch = -89.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Building Camera", meta=(ClampMin="-89.0", ClampMax="-1.0"))
	float MaximumBuildingCameraFreeLookPitch = -10.0f;

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
	TArray<FBotanicusPendingOrder> PendingOrders;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	int32 BuildingProgressionLevel = 1;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="0.1", ClampMax="5.0"))
	float EquipmentLiftHoldDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="0.1", ClampMax="5.0"))
	float PlaceableItemMoveHoldDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="0.1", ClampMax="5.0"))
	float StarterFixtureMoveHoldDuration = 2.0f;

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

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Throw", meta=(ClampMin="0.1", ClampMax="1.0"))
	float QuickBarThrowHoldThreshold = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Throw", meta=(ClampMin="0.5", ClampMax="5.0"))
	float MaximumQuickBarThrowHoldDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Throw", meta=(ClampMin="100.0"))
	float MinimumQuickBarThrowSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Throw", meta=(ClampMin="100.0"))
	float MaximumQuickBarThrowSpeed = 2200.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Path", meta=(ClampMin="25.0"))
	float BuildingEntranceSnapDistance = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Path", meta=(ClampMin="25.0"))
	float ExistingPathSnapDistance = 260.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ValidBuildingPlacementMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InvalidBuildingPlacementMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FurnitureHighlightMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InteractionHighlightMaterial;

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
	TObjectPtr<UBotanicusSharedFundsWidget>
		SharedFundsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusShopObjectivesWidget>
		ShopObjectivesWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusDaySummaryWidget>
		DaySummaryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusClockWidget>
		ClockWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusStorageQuantityWidget>
		StorageQuantityWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusInteractionTargetWidget>
		InteractionTargetWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusThrowPowerWidget>
		ThrowPowerWidget;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalQuickBarItemPreview;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalInspectedQuickBarItem;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalEquippedQuickBarItem;

	TWeakObjectPtr<UBotanicusQuickBarComponent>
		LocalEquippedQuickBarSource;
	FGuid LocalEquippedQuickBarInstanceId;
	FName LocalEquippedQuickBarItemKey = NAME_None;
	int32 LocalEquippedQuickBarSlotIndex = INDEX_NONE;
	int32 LocalEquippedQuickBarQuantity = 0;
	bool bLocalEquippedQuickBarDirty = true;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlaceableItemActor>
		LocalStorageCollectionItem;

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

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusWaterReserveActor>
		LocalActiveWaterReserve;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusWaterReserveActor>
		ServerActiveWaterReserve;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusSalePotActor>
		LocalActiveSalePot;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusDeliveryParcelActor>
		LocalActiveParcelCut;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusDeliveryParcelActor>
		LocalParcelMoveCandidate;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusDeliveryParcelActor>
		LocalParcelMovePreview;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusDeliveryParcelActor>
		LocalMovedDeliveryParcel;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusDeliveryParcelActor>
		ServerMovedDeliveryParcel;

	TArray<FTransform> LocalBuildingOriginalTransforms;
	TArray<FTransform> ServerBuildingOriginalTransforms;
	TObjectPtr<AActor> ServerSnappedMovingWall;
	TObjectPtr<AActor> ServerSnappedExistingWall;
	TObjectPtr<ABotanicusCommunicationDoorActor>
		ServerManualDoorToMove;
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
	float BuildingCameraOrbitYaw = 0.0f;
	float BuildingCameraOrbitPitch = 85.0f;
	float BuildingCameraOrbitDistance = 1800.0f;
	float BuildingOrbitSavedMouseX = 0.0f;
	float BuildingOrbitSavedMouseY = 0.0f;
	bool bBuildingCameraOrbitActive = false;
	bool bBuildingCameraOrbitInitialized = false;
	bool bBuildingCameraFreeLookActive = false;
	float LargeEquipmentPlacementYaw = 0.0f;
	float LargeEquipmentPreviewUpdateAccumulator = 0.0f;
	float QuickBarItemPlacementYaw = 0.0f;
	float QuickBarItemPreviewUpdateAccumulator = 0.0f;
	double QuickBarThrowChargeStartTime = 0.0;
	float EquipmentCarryChargeElapsed = 0.0f;
	float PlaceableItemMoveChargeElapsed = 0.0f;
	float ParcelMoveChargeElapsed = 0.0f;
	float WaterRefillRequestAccumulator = 0.0f;
	double LastServerWaterRefillPulseTime = -1000.0;
	float ParcelPlacementYaw = 0.0f;
	float ParcelPreviewUpdateAccumulator = 0.0f;
	float TopDownRoofRefreshAccumulator = 0.0f;
	bool bLocalLargeEquipmentPlacementValid = false;
	bool bLocalQuickBarItemPlacementValid = false;
	bool bQuickBarThrowChargeActive = false;
	bool bLocalParcelPlacementValid = false;
	bool bEquipmentCarryHoldActivated = false;
	bool bEquipmentCarryKeyHeld = false;
	bool bPlantPotActionHeld = false;
	bool bWaterRefillActionHeld = false;
	bool bServerWaterRefillChanged = false;
	bool bParcelCutActionHeld = false;
	bool bCatalogOrderStateRestored = false;
	bool bOrderCatalogOpenedFromComputer = false;
	bool bFurnitureMoveModeActive = false;
	bool bServerFurnitureMoveModeActive = false;
	float FurnitureHighlightRefreshAccumulator = 0.0f;
	float InteractionHighlightRefreshAccumulator = 0.0f;
	TSet<TWeakObjectPtr<AActor>> LocalHighlightedFurniture;
	TWeakObjectPtr<AActor> LocalInteractionHighlightActor;
	int32 LocalQuickBarItemSlotIndex = INDEX_NONE;
	int32 LocalQuickBarPlacementQuantity = 1;
	int32 LocalStorageCollectionQuantity = 1;
	int32 LocalStorageCollectionMaximum = 1;
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
	EBotanicusPathType PendingPathType =
		EBotanicusPathType::Standard;
	int32 PendingVisitorZoneType = INDEX_NONE;
	bool bCommunicationDoorPlacementActive = false;
	bool bDoorEditSelectionActive = false;
	bool bServerManualDoorPlacement = false;
	bool bAzertyForwardPressed = false;
	bool bAzertyBackwardPressed = false;
	bool bAzertyLeftPressed = false;
	bool bAzertyRightPressed = false;
	int32 LocalCommunicationDoorCandidateIndex = INDEX_NONE;
	TArray<FVector> PendingPathPoints;
	TArray<FName> PendingPurchasedBuildingActorNames;
	TArray<FTransform> PendingPurchasedBuildingTransforms;
	FTimerHandle PurchasedBuildingSnapshotRetryTimer;
	FTimerHandle CollectedItemInspectionRetryTimer;
	TMap<FGuid, FTimerHandle> PendingOrderTimers;
	int32 PurchasedBuildingSnapshotRetryCount = 0;
	int32 PendingCollectedItemSlotIndex = INDEX_NONE;
	int32 CollectedItemInspectionRetryCount = 0;
	FName PendingCollectedItemKey = NAME_None;
};
