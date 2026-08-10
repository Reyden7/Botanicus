// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusShopObjectivesWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusGameState.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

namespace
{
void ConfigureObjectiveText(UTextBlock* Text, int32 Size = 14)
{
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), Size));
	Text->SetColorAndOpacity(FSlateColor(BotanicusHudStyle::PrimaryText()));
}

void CreateObjectiveRow(
	UWidgetTree* WidgetTree,
	UVerticalBox* Body,
	UImage*& OutIcon,
	UTextBlock*& OutName,
	UTextBlock*& OutProgress)
{
	USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>();
	RowSize->SetHeightOverride(47.0f);
	Body->AddChildToVerticalBox(RowSize);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	RowSize->SetContent(Row);

	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
	IconSize->SetWidthOverride(30.0f);
	IconSize->SetHeightOverride(30.0f);
	if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(IconSize))
	{
		Slot->SetPadding(FMargin(0.0f, 7.0f, 8.0f, 7.0f));
		Slot->SetVerticalAlignment(VAlign_Center);
	}
	OutIcon = WidgetTree->ConstructWidget<UImage>();
	IconSize->SetContent(OutIcon);

	OutName = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(OutName);
	if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(OutName))
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetVerticalAlignment(VAlign_Center);
	}

	OutProgress = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureObjectiveText(OutProgress, 14);
	OutProgress->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(OutProgress))
	{
		Slot->SetVerticalAlignment(VAlign_Center);
	}
}

void SetObjectiveProgress(
	UImage* Icon,
	UTextBlock* Label,
	UTextBlock* ProgressLabel,
	const FString& ObjectiveName,
	int32 Current,
	int32 Required)
{
	if (!Icon || !Label || !ProgressLabel)
	{
		return;
	}
	const bool bComplete = Current >= Required;
	Icon->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
		bComplete
			? TEXT("T_HUD_ObjectiveComplete")
			: TEXT("T_HUD_ObjectiveIncomplete")), true);
	Label->SetText(FText::FromString(ObjectiveName));
	ProgressLabel->SetText(FText::FromString(FString::Printf(
		TEXT("%d/%d"), FMath::Min(Current, Required), Required)));
	const FLinearColor Color = bComplete
		? BotanicusHudStyle::CompletedText()
		: BotanicusHudStyle::PrimaryText();
	Label->SetColorAndOpacity(FSlateColor(Color));
	ProgressLabel->SetColorAndOpacity(FSlateColor(Color));
}

void SetReputationProgress(
	UImage* Icon,
	UTextBlock* Label,
	UTextBlock* ProgressLabel,
	int32 Current,
	int32 Required)
{
	if (!Icon || !Label || !ProgressLabel)
	{
		return;
	}
	const bool bComplete = Current >= Required;
	Icon->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
		bComplete
			? TEXT("T_HUD_ObjectiveComplete")
			: TEXT("T_HUD_ObjectiveIncomplete")), true);
	Label->SetText(FText::FromString(TEXT("Reputation")));
	ProgressLabel->SetText(FText::FromString(FString::Printf(
		TEXT("%.2f/%.2f"), Current / 100.0f, Required / 100.0f)));
	const FLinearColor Color = bComplete
		? BotanicusHudStyle::CompletedText()
		: BotanicusHudStyle::PrimaryText();
	Label->SetColorAndOpacity(FSlateColor(Color));
	ProgressLabel->SetColorAndOpacity(FSlateColor(Color));
}
}

void UBotanicusShopObjectivesWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	if (ObjectivesPanel)
	{
		PanelCanvasSlot = Cast<UCanvasPanelSlot>(ObjectivesPanel->Slot);
	}
	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddUniqueDynamic(
			this, &UBotanicusShopObjectivesWidget::HandleToggleClicked);
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
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UCanvasPanel* ClipPanel = WidgetTree->ConstructWidget<UCanvasPanel>();
	ObjectivesPanel = ClipPanel;
	ClipPanel->SetClipping(EWidgetClipping::ClipToBounds);
	PanelCanvasSlot = Root->AddChildToCanvas(ClipPanel);
	PanelCanvasSlot->SetPosition(FVector2D::ZeroVector);
	PanelCanvasSlot->SetSize(FVector2D(370.0f, 370.0f));

	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
		TEXT("T_HUD_ObjectivesBackground")), true);
	UCanvasPanelSlot* BackgroundSlot = ClipPanel->AddChildToCanvas(Background);
	BackgroundSlot->SetPosition(FVector2D::ZeroVector);
	BackgroundSlot->SetSize(FVector2D(370.0f, 370.0f));

	TitleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	TitleLabel->SetColorAndOpacity(
		FSlateColor(BotanicusHudStyle::PrimaryText()));
	TitleLabel->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 16, TEXT("Bold")));
	TitleLabel->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* TitleSlot = ClipPanel->AddChildToCanvas(TitleLabel);
	TitleSlot->SetPosition(FVector2D(68.0f, 27.0f));
	TitleSlot->SetSize(FVector2D(226.0f, 43.0f));

	ToggleButton = WidgetTree->ConstructWidget<UButton>();
	ToggleButton->SetBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.01f));
	UCanvasPanelSlot* ToggleSlot = ClipPanel->AddChildToCanvas(ToggleButton);
	ToggleSlot->SetPosition(FVector2D(302.0f, 24.0f));
	ToggleSlot->SetSize(FVector2D(49.0f, 49.0f));
	ToggleButton->OnClicked.AddUniqueDynamic(
		this, &UBotanicusShopObjectivesWidget::HandleToggleClicked);

	UVerticalBox* ObjectivesVerticalBox =
		WidgetTree->ConstructWidget<UVerticalBox>();
	ObjectivesBody = ObjectivesVerticalBox;
	UCanvasPanelSlot* BodySlot =
		ClipPanel->AddChildToCanvas(ObjectivesVerticalBox);
	BodySlot->SetPosition(FVector2D(33.0f, 82.0f));
	BodySlot->SetSize(FVector2D(304.0f, 245.0f));

	UImage* Icon = nullptr;
	UTextBlock* Name = nullptr;
	UTextBlock* Progress = nullptr;
	CreateObjectiveRow(WidgetTree, ObjectivesVerticalBox, Icon, Name, Progress);
	PlantSalesIcon = Icon;
	PlantSalesLabel = Name;
	PlantSalesProgressLabel = Progress;
	CreateObjectiveRow(WidgetTree, ObjectivesVerticalBox, Icon, Name, Progress);
	CatalogOrdersIcon = Icon;
	CatalogOrdersLabel = Name;
	CatalogOrdersProgressLabel = Progress;
	CreateObjectiveRow(WidgetTree, ObjectivesVerticalBox, Icon, Name, Progress);
	ReputationGoalIcon = Icon;
	ReputationGoalLabel = Name;
	ReputationGoalProgressLabel = Progress;
	CreateObjectiveRow(WidgetTree, ObjectivesVerticalBox, Icon, Name, Progress);
	FundsGoalIcon = Icon;
	FundsGoalLabel = Name;
	FundsGoalProgressLabel = Progress;
	CreateObjectiveRow(WidgetTree, ObjectivesVerticalBox, Icon, Name, Progress);
	DailyRevenueIcon = Icon;
	DailyRevenueLabel = Name;
	DailyRevenueProgressLabel = Progress;
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
		TitleLabel->SetText(FText::FromString(FString::Printf(
			TEXT("OBJECTIFS NIVEAU %d"),
			GameState->GetMainShopLevel() + 1)));
	}
	SetObjectiveProgress(
		PlantSalesIcon, PlantSalesLabel, PlantSalesProgressLabel,
		TEXT("Vendre des plantes"), GameState->GetTotalPlantsSold(),
		GameState->GetRequiredPlantSalesForUpgrade());
	SetObjectiveProgress(
		CatalogOrdersIcon, CatalogOrdersLabel, CatalogOrdersProgressLabel,
		TEXT("Passer des commandes"), GameState->GetTotalCatalogOrders(),
		GameState->GetRequiredCatalogOrdersForUpgrade());
	SetReputationProgress(
		ReputationGoalIcon, ReputationGoalLabel, ReputationGoalProgressLabel,
		GameState->GetShopReputationPoints(),
		GameState->GetRequiredReputationForUpgrade());
	SetObjectiveProgress(
		FundsGoalIcon, FundsGoalLabel, FundsGoalProgressLabel,
		TEXT("Reunir les credits"), GameState->GetSharedFunds(),
		GameState->GetMainShopUpgradeCost());
	SetObjectiveProgress(
		DailyRevenueIcon, DailyRevenueLabel, DailyRevenueProgressLabel,
		TEXT("Chiffre d'affaires"), GameState->GetDailyRevenue(),
		GameState->GetDailyRevenueTarget());
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
			FVector2D(370.0f, bExpanded ? 370.0f : 88.0f));
	}
	RefreshObjectives();
}
