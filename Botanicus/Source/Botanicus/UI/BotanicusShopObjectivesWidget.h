// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusShopObjectivesWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UCanvasPanelSlot;
class UImage;
class UButton;
class UCanvasPanel;

/** Collapsible orange HUD list for the shared main-shop upgrade goals. */
UCLASS()
class BOTANICUS_API UBotanicusShopObjectivesWidget
	: public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RefreshObjectives();

	UFUNCTION()
	void HandleToggleClicked();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleLabel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> ObjectivesBody;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> ObjectivesPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DayTitleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DailySalesLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DailySalesProgressLabel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DailyRevenueLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DailyRevenueProgressLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> DailyRevenueIcon;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlantSalesLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlantSalesProgressLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> PlantSalesIcon;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CatalogOrdersLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CatalogOrdersProgressLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> CatalogOrdersIcon;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FundsGoalLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FundsGoalProgressLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> FundsGoalIcon;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReputationGoalLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReputationGoalProgressLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ReputationGoalIcon;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PanelCanvasSlot;

	bool bExpanded = true;
};
