// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusShopObjectivesWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusGameState.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

namespace
{
void ConfigureObjectiveText(UTextBlock* Text)
{
	if (!Text)
	{
		return;
	}
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
	Text->SetAutoWrapText(true);
	Text->SetMargin(FMargin(7.0f, 4.0f));
}

void SetObjectiveProgress(
	UTextBlock* Label,
	const FString& ObjectiveName,
	int32 Current,
	int32 Required)
{
	if (!Label)
	{
		return;
	}
	const bool bComplete = Current >= Required;
	Label->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("%s  %s : %d/%d  -  %s"),
				bComplete ? TEXT("[OK]") : TEXT("[  ]"),
				*ObjectiveName,
				FMath::Min(Current, Required),
				Required,
				bComplete ? TEXT("TERMINE") : TEXT("EN COURS"))));
	Label->SetColorAndOpacity(
		FSlateColor(
			bComplete
				? FLinearColor(0.3f, 1.0f, 0.45f, 1.0f)
				: FLinearColor(1.0f, 0.72f, 0.22f, 1.0f)));
}

void SetReputationProgress(
	UTextBlock* Label,
	int32 Current,
	int32 Required)
{
	if (!Label)
	{
		return;
	}
	const bool bComplete = Current >= Required;
	Label->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("%s  Reputation : %.2f/%.2f etoiles  -  %s"),
				bComplete ? TEXT("[OK]") : TEXT("[  ]"),
				Current / 100.0f,
				Required / 100.0f,
				bComplete ? TEXT("TERMINE") : TEXT("EN COURS"))));
	Label->SetColorAndOpacity(
		FSlateColor(
			bComplete
				? FLinearColor(0.3f, 1.0f, 0.45f, 1.0f)
				: FLinearColor(1.0f, 0.72f, 0.22f, 1.0f)));
}
}

void UBotanicusShopObjectivesWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	RefreshObjectives();
}

void UBotanicusShopObjectivesWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshObjectives();
}

void UBotanicusShopObjectivesWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.19f, 0.075f, 0.015f, 0.94f));
	Panel->SetPadding(FMargin(9.0f));
	PanelCanvasSlot = Root->AddChildToCanvas(Panel);
	PanelCanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	PanelCanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	PanelCanvasSlot->SetPosition(FVector2D(-24.0f, 184.0f));
	PanelCanvasSlot->SetSize(FVector2D(390.0f, 360.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	UButton* ToggleButton = WidgetTree->ConstructWidget<UButton>();
	ToggleButton->SetBackgroundColor(
		FLinearColor(0.78f, 0.28f, 0.035f, 1.0f));
	TitleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	TitleLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TitleLabel->SetJustification(ETextJustify::Left);
	TitleLabel->SetMargin(FMargin(10.0f, 8.0f));
	TitleLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 17));
	ToggleButton->AddChild(TitleLabel);
	Column->AddChildToVerticalBox(ToggleButton);
	ToggleButton->OnClicked.AddDynamic(
		this,
		&UBotanicusShopObjectivesWidget::HandleToggleClicked);

	ObjectivesBody = WidgetTree->ConstructWidget<UVerticalBox>();
	UVerticalBoxSlot* BodySlot =
		Column->AddChildToVerticalBox(ObjectivesBody);
	BodySlot->SetPadding(FMargin(2.0f, 7.0f, 2.0f, 2.0f));

	DayTitleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(DayTitleLabel);
	DayTitleLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.45f, 0.86f, 1.0f, 1.0f)));
	ObjectivesBody->AddChildToVerticalBox(DayTitleLabel);

	DailySalesLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(DailySalesLabel);
	ObjectivesBody->AddChildToVerticalBox(DailySalesLabel);

	DailyRevenueLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(DailyRevenueLabel);
	ObjectivesBody->AddChildToVerticalBox(DailyRevenueLabel);

	PlantSalesLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(PlantSalesLabel);
	ObjectivesBody->AddChildToVerticalBox(PlantSalesLabel);

	CatalogOrdersLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(CatalogOrdersLabel);
	ObjectivesBody->AddChildToVerticalBox(CatalogOrdersLabel);

	ReputationGoalLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(ReputationGoalLabel);
	ObjectivesBody->AddChildToVerticalBox(ReputationGoalLabel);

	FundsGoalLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(FundsGoalLabel);
	ObjectivesBody->AddChildToVerticalBox(FundsGoalLabel);
}

void UBotanicusShopObjectivesWidget::RefreshObjectives()
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	if (TitleLabel)
	{
		TitleLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("OBJECTIFS BOUTIQUE - NIVEAU %d   %s"),
					GameState->GetMainShopLevel() + 1,
					bExpanded ? TEXT("[-]") : TEXT("[+]"))));
	}
	if (DayTitleLabel)
	{
		DayTitleLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("JOUR %d - %s"),
					GameState->GetCurrentDayNumber(),
					GameState->IsShopDayActive()
						? TEXT("MAGASIN OUVERT")
						: TEXT("PREPARATION"))));
	}
	SetObjectiveProgress(
		DailySalesLabel,
		TEXT("Ventes du jour"),
		GameState->GetDailyPlantsSold(),
		GameState->GetDailySalesTarget());
	SetObjectiveProgress(
		DailyRevenueLabel,
		TEXT("Chiffre du jour"),
		GameState->GetDailyRevenue(),
		GameState->GetDailyRevenueTarget());
	SetObjectiveProgress(
		PlantSalesLabel,
		TEXT("Vendre des plantes"),
		GameState->GetTotalPlantsSold(),
		GameState->GetRequiredPlantSalesForUpgrade());
	SetObjectiveProgress(
		CatalogOrdersLabel,
		TEXT("Passer des commandes"),
		GameState->GetTotalCatalogOrders(),
		GameState->GetRequiredCatalogOrdersForUpgrade());
	SetObjectiveProgress(
		FundsGoalLabel,
		TEXT("Reunir les credits"),
		GameState->GetSharedFunds(),
		GameState->GetMainShopUpgradeCost());
	SetReputationProgress(
		ReputationGoalLabel,
		GameState->GetShopReputationPoints(),
		GameState->GetRequiredReputationForUpgrade());
}

void UBotanicusShopObjectivesWidget::HandleToggleClicked()
{
	bExpanded = !bExpanded;
	if (ObjectivesBody)
	{
		ObjectivesBody->SetVisibility(
			bExpanded
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (PanelCanvasSlot)
	{
		PanelCanvasSlot->SetSize(
			FVector2D(390.0f, bExpanded ? 360.0f : 52.0f));
	}
	RefreshObjectives();
}
