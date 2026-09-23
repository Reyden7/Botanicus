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
class UBotanicusCarryProgressWidget;
class UBotanicusOrderCatalogWidget;
class UBotanicusWorkbenchUpgradeWidget;
class UBotanicusDevelopmentPanelWidget;
class UBotanicusBotanistNotebookWidget;
class UBotanicusSharedFundsWidget;
class UBotanicusShopObjectivesWidget;
class UBotanicusDaySummaryWidget;
class UBotanicusClockWidget;
class UBotanicusStorageQuantityWidget;
class UBotanicusInteractionTargetWidget;
class UBotanicusClimateDeviceControlWidget;
class UBotanicusHudMessageWidget;
class UBotanicusHudLayoutWidget;
class UBotanicusThrowPowerWidget;
class ABotanicusGameState;
class ACameraActor;
class AActor;
class ABotanicusDeliveryParcelActor;
class ABotanicusDeliveryZoneActor;
class ABotanicusBrokenFlowerPotActor;
class ABotanicusLargeEquipmentActor;
class ABotanicusClimateDeviceActor;
class ABotanicusPlaceableItemActor;
class ABotanicusPlantPotActor;
class ABotanicusIllegalPlanterActor;
class ABotanicusIllegalCustomerCharacter;
class ABotanicusSalesDisplayActor;
struct FBotanicusQuickBarSlot;
class ABotanicusSalePotActor;
class ABotanicusCashRegisterActor;
class ABotanicusWateringCanActor;
class ABotanicusWaterReserveActor;
class ABotanicusComputerActor;
class ABotanicusStorageShelfActor;
class ABotanicusPreparationWorkbenchActor;
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

	/** Development command: places a ping in front of the controlled pawn. */
	UFUNCTION(Exec)
	void BotanicusTestPing();


	UFUNCTION(BlueprintPure, Category="Botanicus|Furniture")
	bool IsFurnitureMoveModeActive() const
	{
		return bFurnitureMoveModeActive;
	}

	/** Client state for UI, authoritative mirror for gameplay validation. */
	bool IsFurnitureMoveModeActiveForGameplay() const
	{
		return HasAuthority()
			? bServerFurnitureMoveModeActive
			: bFurnitureMoveModeActive;
	}

	/** Exact actor currently under the local interaction trace. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Interaction")
	AActor* GetLocalInteractionTarget() const
	{
		return LocalInteractionHighlightActor.Get();
	}

	/** Original world actor represented by the local placement preview. */
	ABotanicusPlaceableItemActor* GetLocallyMovedPlaceableItem() const
	{
		return LocalMovedPlaceableItem.Get();
	}

	UFUNCTION(BlueprintCallable, Category="Botanicus|Climate Device")
	void CloseClimateDeviceControl();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Climate Device")
	void RequestToggleClimateDevice(ABotanicusClimateDeviceActor* ClimateDevice);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Climate Device")
	void RequestSetClimateDevicePower(
		ABotanicusClimateDeviceActor* ClimateDevice,
		float PowerLevel);


	UFUNCTION(BlueprintCallable, Category="Botanicus|Delivery")
	void ToggleOrderCatalog();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Development")
	void ToggleDevelopmentPanel();

	UFUNCTION(BlueprintCallable, Category="Botanicus|Botanist Notebook")
	void ToggleBotanistNotebook();

	UFUNCTION(Client, Reliable)
	void ClientOpenOrderCatalogFromComputer(
		ABotanicusComputerActor* Computer);

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

	FName GetCarriedTransplantPlantKey() const
	{
		return CarriedTransplantPlantKey;
	}
	FName GetCarriedTransplantItemKey() const
	{
		return CarriedTransplantItemKey;
	}
	float GetCarriedTransplantGrowth() const
	{
		return CarriedTransplantGrowth;
	}
	float GetCarriedTransplantCare() const
	{
		return CarriedTransplantCare;
	}
	int32 GetCarriedTransplantWateringCount() const
	{
		return CarriedTransplantWateringCount;
	}
	bool IsCarriedTransplantElementalDead() const
	{
		return bCarriedTransplantElementalDead;
	}
	void RestoreCarriedTransplantState(
		FName PlantKey,
		FName ItemKey,
		float Growth,
		float Care,
		int32 WateringCount,
		bool bElementalDead);


	/** Prevents hotbar shortcuts from colliding with active item UI. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Inventory")
	bool IsQuickBarInputBlocked() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Inventory")
	bool IsMovingWorldItemOutsideInventory() const;

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

	/** Replaces the engine's yellow console-style messages with HUD feedback. */
	virtual void ClientMessage_Implementation(
		const FString& S,
		FName Type,
		float MsgLifeTime) override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	UBotanicusMultiplayerSubsystem* GetBotanicusMultiplayerSubsystem() const;

	void ForceFirstPersonView();
	void InitializeHudLayoutWidget();
	void InitializeQuickBarWidget();
	void ToggleQuickBarReorganizationMode();
	void InitializeSharedFundsWidget();
	void InitializeShopObjectivesWidget();
	void InitializeDaySummaryWidget();
	void InitializeClockWidget();
	void InitializeStorageQuantityWidget();
	void InitializeInteractionTargetWidget();
	void InitializeHudMessageWidget();
	void SetHudCursorMode(bool bEnabled);
	void InitializeOrderCatalogWidget();
	void FinishOpenOrderCatalogFromComputer();
	void CloseOrderCatalogFromComputer();
	void FinishCloseOrderCatalogFromComputer();
	void InitializeWorkbenchUpgradeWidget();
	void InitializeDevelopmentPanelWidget();
	void InitializeBotanistNotebookWidget();
	void HideEbsDemoHud();
	void HideLegacyWorldGuidance();
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
	bool TryBeginWateringCanSpray();
	void EndWateringCanSpray();
	bool TryOpenNearbyWorkbenchUpgrade();
	bool TryUseNearbyComputer();
	bool TryMoveNearbyPlaceableItem();
	bool TryBreakNearbyBrokenFlowerPot();
	void BeginPlaceableItemMoveCharge(
		ABotanicusPlaceableItemActor* WorldItem);
	void UpdatePlaceableItemMoveCharge(float DeltaTime);
	void CancelPlaceableItemMoveCharge();
	bool TryBeginNearbyPlantPotAction();
	bool TryTogglePlantInspection();
	bool TryBeginNearbySalePotAction();
	bool TryUseGardenTrowelForTransplant();
	void UpdateGardenTrowelTransplantHold(float DeltaTime);
	void CancelGardenTrowelTransplantHold();
	bool HasCarriedTransplant() const;
	void ClearCarriedTransplant();
	bool TryPlaceSelectedSalePotOnWorkbench();
	bool TryBeginDisplayedSalePotPickup();
	void UpdateDisplayedSalePotPickup(float DeltaTime);
	void CancelDisplayedSalePotPickup();
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
	void DismissInteractionPromptAfterConfirmation();
	void OpenClimateDeviceControl(ABotanicusClimateDeviceActor* ClimateDevice);
	void SetInteractionTargetHighlighted(
		AActor* Actor,
		bool bHighlighted) const;
	void BeginLargeEquipmentPlacement(
		ABotanicusLargeEquipmentActor* Equipment);
	void UpdateLargeEquipmentPlacement(float DeltaTime);
	void RotateLargeEquipmentPlacement(float Direction);
	FVector GetViewDirectedGroundPlacementLocation(
		float MinimumDistance,
		float MaximumDistance) const;
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
	void CopyWorldItemStateToPreview(
		const ABotanicusPlaceableItemActor* WorldItem,
		ABotanicusPlaceableItemActor* Preview) const;
	void UpdateEquippedQuickBarItem();
	void DestroyEquippedQuickBarItem();
	void UpdateWateringCanSpray(float DeltaTime);

	UFUNCTION()
	void HandleEquippedQuickBarChanged();

	void BeginWorldItemMove(
		ABotanicusPlaceableItemActor* WorldItem,
		bool bServerReservationAlreadyHeld = false);
	void BeginHeldWorldItemPlacement();
	bool TryPlaceHeldSalePotOnWorkbench();
	void UpdateHeldWorldItemPreview();
	void ReturnMovedWorldItemToHand();
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
		int32 RequestedQuantity = 1,
		bool bFloorOnlyPlacement = false) const;
	bool FindAimedStorageSlot(
		ABotanicusStorageShelfActor*& OutShelf,
		int32& OutSlotIndex) const;
	bool TryStoreSelectedQuickBarItemOnAimedShelf();
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
	ABotanicusDeliveryZoneActor* FindDeliveryZone();
	bool DeliverCatalogItem(
		FName ItemKey,
		int32 Quantity);
	void CompleteCatalogOrder(FGuid OrderId);
	void RefreshOrderCatalog();
	void SpawnTestLargeEquipment(FName ItemKey);
	bool TryPlacePing();

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

	UFUNCTION(Server, Unreliable)
	void ServerPulseWateringCanSpray();

	UFUNCTION(Server, Reliable)
	void ServerBeginPlantPotAction(
		ABotanicusPlantPotActor* PlantPot);

	UFUNCTION(Server, Reliable)
	void ServerEndPlantPotAction(
		ABotanicusPlantPotActor* PlantPot);

	UFUNCTION(Server, Reliable)
	void ServerBeginIllegalPlanterAction(
		ABotanicusIllegalPlanterActor* IllegalPlanter);

	UFUNCTION(Server, Reliable)
	void ServerEndIllegalPlanterAction(
		ABotanicusIllegalPlanterActor* IllegalPlanter);

	UFUNCTION(Server, Reliable)
	void ServerSellIllegalOrder(
		ABotanicusIllegalCustomerCharacter* Customer);

	UFUNCTION(Server, Reliable)
	void ServerBeginSalePotAction(
		ABotanicusSalePotActor* SalePot);

	UFUNCTION(Server, Reliable)
	void ServerEndSalePotAction(
		ABotanicusSalePotActor* SalePot);

	UFUNCTION(Server, Reliable)
	void ServerUseGardenTrowelForTransplant(AActor* TargetPot);

	UFUNCTION(Server, Reliable)
	void ServerBreakBrokenFlowerPot(
		ABotanicusBrokenFlowerPotActor* BrokenPot);

	UFUNCTION(Server, Reliable)
	void ServerRetrieveDisplayedSalePot(
		ABotanicusSalesDisplayActor* SalesDisplay);

	UFUNCTION(Server, Reliable)
	void ServerPlacePlantOnSalesDisplay(
		ABotanicusSalesDisplayActor* SalesDisplay);

	UFUNCTION(Server, Reliable)
	void ServerCheckoutRegister(
		ABotanicusCashRegisterActor* CashRegister);

	UFUNCTION()
	void OnRep_CarriedTransplantState();

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
	void ServerInteractClimateDevice(
		ABotanicusClimateDeviceActor* ClimateDevice);

	UFUNCTION(Server, Unreliable)
	void ServerSetClimateDevicePower(
		ABotanicusClimateDeviceActor* ClimateDevice,
		float PowerLevel);

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
		int32 Quantity,
		bool bFloorOnlyPlacement);

	UFUNCTION(Server, Reliable)
	void ServerStoreQuickBarItemOnShelf(
		int32 QuickBarSlotIndex,
		FGuid InstanceId,
		FName ItemKey,
		ABotanicusStorageShelfActor* Shelf,
		int32 ShelfSlotIndex,
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

	UFUNCTION(Client, Reliable)
	void ClientBeginPlaceableItemHold(float DurationSeconds);

	UFUNCTION(Client, Reliable)
	void ClientEndPlaceableItemHold();

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
	void ServerPlacePing(FVector_NetQuantize10 RequestedLocation);



	UPROPERTY(Transient)
	TObjectPtr<UBotanicusOrderCatalogWidget> OrderCatalogWidget;

	TWeakObjectPtr<ABotanicusComputerActor> ActiveComputerView;
	FTimerHandle ComputerViewTransitionTimer;

	UPROPERTY(
		EditDefaultsOnly,
		Category="Botanicus|Computer Camera",
		meta=(ClampMin="0.0", ClampMax="2.0", Units="s"))
	float ComputerCameraEnterBlendTime = 0.55f;

	UPROPERTY(
		EditDefaultsOnly,
		Category="Botanicus|Computer Camera",
		meta=(ClampMin="0.0", ClampMax="2.0", Units="s"))
	float ComputerCameraExitBlendTime = 0.4f;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusWorkbenchUpgradeWidget>
		WorkbenchUpgradeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusDevelopmentPanelWidget>
		DevelopmentPanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusBotanistNotebookWidget>
		BotanistNotebookWidget;


	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ping", meta=(ClampMin="100.0"))
	float MaximumPingDistance = 30000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ping", meta=(ClampMin="0.1"))
	float PingLifeTime = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Ping", meta=(ClampMin="0.1"))
	float PingCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="1.0", ClampMax="60.0"))
	float WorldItemLookAngle = 22.0f;

	/** Maximum player-to-target distance for every world interaction (230 cm = 2.30 m). */
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Interaction", meta=(ClampMin="25.0", ClampMax="300.0", Units="cm"))
	float MaximumWorldInteractionDistance = 230.0f;

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

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Equipment Placement", meta=(ClampMin="25.0"))
	float MinimumEquipmentPlacementDistance = 100.0f;

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

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Item Placement", meta=(ClampMin="25.0"))
	float MinimumQuickBarItemPlacementDistance = 100.0f;

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


	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FurnitureHighlightMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InteractionHighlightMaterial;


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

	/** Master Widget Blueprint that owns all freely movable HUD slots. */
	UPROPERTY(Transient)
	TObjectPtr<UBotanicusHudLayoutWidget>
		HudLayoutWidget;

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
	TObjectPtr<UBotanicusClimateDeviceControlWidget>
		ClimateDeviceControlWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusHudMessageWidget>
		HudMessageWidget;

	bool bHudCursorModeActive = false;

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

	/** A growing plant is carried directly by the player until it is repotted. */
	UPROPERTY(ReplicatedUsing=OnRep_CarriedTransplantState)
	FName CarriedTransplantPlantKey = NAME_None;

	/** Exact harvest/sale item key, including its quality suffix when relevant. */
	UPROPERTY(ReplicatedUsing=OnRep_CarriedTransplantState)
	FName CarriedTransplantItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_CarriedTransplantState)
	float CarriedTransplantGrowth = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_CarriedTransplantState)
	float CarriedTransplantCare = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_CarriedTransplantState)
	int32 CarriedTransplantWateringCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CarriedTransplantState)
	bool bCarriedTransplantElementalDead = false;

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
	TObjectPtr<ABotanicusIllegalPlanterActor>
		LocalActiveIllegalPlanter;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlantPotActor>
		LocalTrowelTransplantTarget;

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
	TObjectPtr<ABotanicusSalesDisplayActor>
		LocalDisplayedSalePotPickupCandidate;

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

	float LargeEquipmentPlacementYaw = 0.0f;
	float LargeEquipmentPreviewUpdateAccumulator = 0.0f;
	float QuickBarItemPlacementYaw = 0.0f;
	float QuickBarItemPreviewUpdateAccumulator = 0.0f;
	double QuickBarThrowChargeStartTime = 0.0;
	float EquipmentCarryChargeElapsed = 0.0f;
	float PlaceableItemMoveChargeElapsed = 0.0f;
	float DisplayedSalePotPickupElapsed = 0.0f;
	float GardenTrowelTransplantHoldElapsed = 0.0f;
	float PlantPotActionHoldElapsed = 0.0f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Interaction", meta=(ClampMin="0.1"))
	float GardenTrowelTransplantHoldDuration = 1.0f;
	float ParcelMoveChargeElapsed = 0.0f;
	float WaterRefillRequestAccumulator = 0.0f;
	float WateringCanSprayRequestAccumulator = 0.0f;
	double LastServerWaterRefillPulseTime = -1000.0;
	double LastServerWateringCanSprayPulseTime = -1000.0;
	float ParcelPlacementYaw = 0.0f;
	float ParcelPreviewUpdateAccumulator = 0.0f;
	bool bLocalLargeEquipmentPlacementValid = false;
	bool bLocalQuickBarItemPlacementValid = false;
	bool bQuickBarThrowChargeActive = false;
	bool bLocalParcelPlacementValid = false;
	bool bEquipmentCarryHoldActivated = false;
	bool bEquipmentCarryKeyHeld = false;
	bool bPlantPotActionHeld = false;
	bool bTrowelTransplantActionHeld = false;
	bool bWaterRefillActionHeld = false;
	bool bWateringCanSprayHeld = false;
	bool bServerWaterRefillChanged = false;
	bool bParcelCutActionHeld = false;
	bool bCatalogOrderStateRestored = false;
	bool bOrderCatalogOpenedFromComputer = false;
	bool bFurnitureMoveModeActive = false;
	bool bServerFurnitureMoveModeActive = false;
	float FurnitureHighlightRefreshAccumulator = 0.0f;
	float InteractionHighlightRefreshAccumulator = 0.0f;
	float LegacyWorldGuidanceRefreshAccumulator = 0.0f;
	TSet<TWeakObjectPtr<AActor>> LocalHighlightedFurniture;
	TWeakObjectPtr<AActor> LocalInteractionHighlightActor;
	TWeakObjectPtr<ABotanicusPlantPotActor> LocalInspectedPlant;
	int32 LocalQuickBarItemSlotIndex = INDEX_NONE;
	int32 LocalQuickBarPlacementQuantity = 1;
	int32 LocalStorageCollectionQuantity = 1;
	int32 LocalStorageCollectionMaximum = 1;
	FGuid LocalQuickBarItemInstanceId;
	FName LocalQuickBarItemKey = NAME_None;
	double LastServerPingTime = -1000.0;
	bool bAzertyForwardPressed = false;
	bool bAzertyBackwardPressed = false;
	bool bAzertyLeftPressed = false;
	bool bAzertyRightPressed = false;
	FTimerHandle CollectedItemInspectionRetryTimer;
	TMap<FGuid, FTimerHandle> PendingOrderTimers;
	int32 PendingCollectedItemSlotIndex = INDEX_NONE;
	int32 CollectedItemInspectionRetryCount = 0;
	FName PendingCollectedItemKey = NAME_None;
};
