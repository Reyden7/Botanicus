// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "BotanicusOrderCatalogWidget.generated.h"

class ABotanicusPlayerController;
class UButton;
class UImage;
class UScrollBox;
class USlider;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UBotanicusBuildingCatalogRowWidget;

enum class EBotanicusCommandPanelTab : uint8
{
	Seeds,
	GardeningTools,
	Preparation,
	Sales,
	Buildings
};

/** Blue progression row for the shared main nursery shop. */
UCLASS()
class BOTANICUS_API UBotanicusMainShopUpgradeRowWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRow(
		ABotanicusPlayerController* InController);
	void RefreshProgress();

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleUpgradeClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeButtonLabel;
};

/** Blue progression row for the starter preparation workbench. */
UCLASS()
class BOTANICUS_API UBotanicusWorkbenchUpgradeRowWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRow(
		ABotanicusPlayerController* InController);
	void RefreshProgress();

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleUpgradeClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeButtonLabel;
};

/** One catalogue entry with its server-authoritative order action. */
UCLASS()
class BOTANICUS_API UBotanicusOrderItemRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRow(
		ABotanicusPlayerController* InController,
		const FBotanicusItemDefinition& InDefinition,
		bool bInShowSeedElement);
	void RefreshAvailability(int32 AvailableFunds);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleOrderClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PriceLabel;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ItemIconArea;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ElementBadgeArea;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ElementBadgeBackground;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ElementBadgeIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ElementBadgeLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OrderButton;

	FName ItemKey = NAME_None;
	int32 Price = 0;
};

/** Full native catalogue screen populated from the configured item data asset. */
UCLASS()
class BOTANICUS_API UBotanicusOrderCatalogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithController(
		ABotanicusPlayerController* InController);
	void Refresh();
	void ShowBuildingTab();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	bool BindDesignerLayout();
	void RebuildItemRows();
	void SelectTab(EBotanicusCommandPanelTab NewTab);
	void RefreshTabButtons();
	void SyncCatalogScrollbar();

	UFUNCTION()
	void HandleCatalogScrollChanged(float CurrentOffset);

	UFUNCTION()
	void HandleCatalogSliderChanged(float Value);

	UFUNCTION()
	void HandleCatalogScrollUpClicked();

	UFUNCTION()
	void HandleCatalogScrollDownClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleSeedsTabClicked();

	UFUNCTION()
	void HandleToolsTabClicked();

	UFUNCTION()
	void HandlePreparationTabClicked();

	UFUNCTION()
	void HandleSalesTabClicked();

	UFUNCTION()
	void HandleBuildingsTabClicked();

	UFUNCTION()
	void HandleShopOpenClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PendingOrdersLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PendingOrdersBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShopOpenButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShopOpenButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ShopStateIcon;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ShopStateBackground;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ItemsBox;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ItemsScroll;

	UPROPERTY(Transient)
	TObjectPtr<USlider> CatalogScrollSlider;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBotanicusOrderItemRowWidget>> ItemRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBotanicusBuildingCatalogRowWidget>> BuildingRows;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusMainShopUpgradeRowWidget> MainShopUpgradeRow;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusWorkbenchUpgradeRowWidget>
		WorkbenchUpgradeRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> TabButtons;

	EBotanicusCommandPanelTab ActiveTab =
		EBotanicusCommandPanelTab::Seeds;

	float RefreshAccumulator = 0.0f;
	bool bSyncingCatalogScrollbar = false;
};
