// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusShopObjectivesWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UCanvasPanelSlot;
class UImage;

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

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ObjectivesBody;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DayTitleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DailySalesLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DailySalesProgressLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DailyRevenueLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DailyRevenueProgressLabel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> DailyRevenueIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlantSalesLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlantSalesProgressLabel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> PlantSalesIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CatalogOrdersLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CatalogOrdersProgressLabel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> CatalogOrdersIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsGoalLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsGoalProgressLabel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> FundsGoalIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReputationGoalLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReputationGoalProgressLabel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> ReputationGoalIcon;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PanelCanvasSlot;

	bool bExpanded = true;
};
