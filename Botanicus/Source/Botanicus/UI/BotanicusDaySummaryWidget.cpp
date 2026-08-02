// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusDaySummaryWidget.h"

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
void ConfigureDaySummaryText(
	UTextBlock* Text,
	int32 Size,
	const FLinearColor& Color)
{
	if (!Text)
	{
		return;
	}
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), Size));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
}
}

void UBotanicusDaySummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusDaySummaryWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState ||
		GameState->GetDaySummaryRevision() <=
			LastSeenSummaryRevision)
	{
		return;
	}

	LastSeenSummaryRevision =
		GameState->GetDaySummaryRevision();
	RefreshSummary();
	SetVisibility(ESlateVisibility::Visible);
}

void UBotanicusDaySummaryWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.025f, 0.07f, 0.045f, 0.99f));
	Panel->SetPadding(FMargin(28.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(620.0f, 430.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	TitleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureDaySummaryText(
		TitleLabel,
		28,
		FLinearColor(0.96f, 0.78f, 0.24f, 1.0f));
	UVerticalBoxSlot* TitleSlot =
		Column->AddChildToVerticalBox(TitleLabel);
	TitleSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 20.0f));

	SummaryLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureDaySummaryText(
		SummaryLabel,
		19,
		FLinearColor::White);
	UVerticalBoxSlot* SummarySlot =
		Column->AddChildToVerticalBox(SummaryLabel);
	SummarySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SummarySlot->SetVerticalAlignment(VAlign_Center);

	NextDayLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureDaySummaryText(
		NextDayLabel,
		16,
		FLinearColor(0.55f, 0.9f, 0.66f, 1.0f));
	UVerticalBoxSlot* NextSlot =
		Column->AddChildToVerticalBox(NextDayLabel);
	NextSlot->SetPadding(FMargin(0.0f, 15.0f));

	UButton* ContinueButton = WidgetTree->ConstructWidget<UButton>();
	ContinueButton->SetBackgroundColor(
		FLinearColor(0.08f, 0.48f, 0.22f, 1.0f));
	UTextBlock* ContinueLabel =
		WidgetTree->ConstructWidget<UTextBlock>();
	ContinueLabel->SetText(
		NSLOCTEXT(
			"BotanicusDay",
			"ContinuePreparation",
			"CONTINUER LA PREPARATION"));
	ConfigureDaySummaryText(
		ContinueLabel,
		16,
		FLinearColor::White);
	ContinueLabel->SetMargin(FMargin(18.0f, 10.0f));
	ContinueButton->AddChild(ContinueLabel);
	UVerticalBoxSlot* ButtonSlot =
		Column->AddChildToVerticalBox(ContinueButton);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ContinueButton->OnClicked.AddDynamic(
		this,
		&UBotanicusDaySummaryWidget::HandleContinueClicked);
}

void UBotanicusDaySummaryWidget::RefreshSummary()
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	const int32 Sales = GameState->GetLastDayPlantsSold();
	const int32 Revenue = GameState->GetLastDayRevenue();
	const int32 SalesTarget = GameState->GetLastDaySalesTarget();
	const int32 RevenueTarget = GameState->GetLastDayRevenueTarget();
	const bool bSalesComplete = Sales >= SalesTarget;
	const bool bRevenueComplete = Revenue >= RevenueTarget;

	if (TitleLabel)
	{
		TitleLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("BILAN DU JOUR %d"),
					GameState->GetLastCompletedDayNumber())));
	}
	if (SummaryLabel)
	{
		const FString SatisfactionText =
			GameState->GetLastDayReviewCount() > 0
				? FString::Printf(
					TEXT("%d %% (%d avis)"),
					GameState->GetLastDayAverageSatisfaction(),
					GameState->GetLastDayReviewCount())
				: TEXT("Aucun avis");
		SummaryLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"%s  Plantes vendues : %d / %d\n\n"
						"%s  Chiffre d'affaires : %d / %d credits\n\n"
						"Satisfaction moyenne : %s\n\n"
						"Evolution de la reputation : %+d points"),
					bSalesComplete ? TEXT("[OK]") : TEXT("[  ]"),
					Sales,
					SalesTarget,
					bRevenueComplete ? TEXT("[OK]") : TEXT("[  ]"),
					Revenue,
					RevenueTarget,
					*SatisfactionText,
					GameState->GetLastDayReputationDelta())));
	}
	if (NextDayLabel)
	{
		NextDayLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"JOUR %d : magasin ferme. Preparez vos plantes puis ouvrez quand vous etes pret."),
					GameState->GetCurrentDayNumber())));
	}
}

void UBotanicusDaySummaryWidget::HandleContinueClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
