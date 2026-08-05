// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusWorkbenchUpgradeWidget.h"

#include "BotanicusPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"

namespace
{
void SetWorkbenchTextSize(UTextBlock* Text, int32 Size)
{
	if (!Text)
	{
		return;
	}
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
}
}

void UBotanicusWorkbenchLevelRowWidget::InitializeRow(
	ABotanicusPlayerController* InController,
	ABotanicusPreparationWorkbenchActor* InWorkbench,
	int32 InTargetLevel)
{
	BotanicusController = InController;
	Workbench = InWorkbench;
	TargetLevel = FMath::Clamp(InTargetLevel, 1, 5);
	Refresh();
}

void UBotanicusWorkbenchLevelRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusWorkbenchLevelRowWidget::BuildLayout()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(
		FLinearColor(0.035f, 0.11f, 0.14f, 0.98f));
	Root->SetPadding(FMargin(15.0f, 11.0f));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* Row =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChild(Row);

	UVerticalBox* Details =
		WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* DetailsSlot =
		Row->AddChildToHorizontalBox(Details);
	DetailsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DetailsSlot->SetVerticalAlignment(VAlign_Center);

	LevelLabel = WidgetTree->ConstructWidget<UTextBlock>();
	LevelLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	SetWorkbenchTextSize(LevelLabel, 19);
	Details->AddChildToVerticalBox(LevelLabel);

	PriceLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PriceLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.93f, 0.77f, 0.25f, 1.0f)));
	SetWorkbenchTextSize(PriceLabel, 14);
	Details->AddChildToVerticalBox(PriceLabel);

	LevelButton = WidgetTree->ConstructWidget<UButton>();
	LevelButton->SetBackgroundColor(
		FLinearColor(0.07f, 0.46f, 0.40f, 1.0f));
	LevelButtonLabel =
		WidgetTree->ConstructWidget<UTextBlock>();
	LevelButtonLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	LevelButtonLabel->SetMargin(FMargin(13.0f, 9.0f));
	SetWorkbenchTextSize(LevelButtonLabel, 13);
	LevelButton->AddChild(LevelButtonLabel);
	UHorizontalBoxSlot* ButtonSlot =
		Row->AddChildToHorizontalBox(LevelButton);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
	LevelButton->OnClicked.AddDynamic(
		this,
		&UBotanicusWorkbenchLevelRowWidget::HandleLevelClicked);
}

void UBotanicusWorkbenchLevelRowWidget::Refresh()
{
	if (!IsValid(Workbench) || !BotanicusController)
	{
		if (LevelButton)
		{
			LevelButton->SetIsEnabled(false);
		}
		return;
	}

	const int32 CurrentLevel = Workbench->GetWorkbenchLevel();
	const int32 TransitionCost =
		Workbench->GetLevelTransitionCost(TargetLevel);
	const int32 UnlockCost =
		ABotanicusPreparationWorkbenchActor::
			GetLevelUnlockCost(TargetLevel);

	if (LevelLabel)
	{
		LevelLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("NIVEAU %d  -  %d EMPLACEMENT%s"),
					TargetLevel,
					TargetLevel,
					TargetLevel > 1 ? TEXT("S") : TEXT(""))));
	}
	if (PriceLabel)
	{
		PriceLabel->SetText(
			TargetLevel == 1
				? FText::FromString(TEXT("NIVEAU DE BASE"))
				: FText::FromString(
					FString::Printf(
						TEXT("PRIX DU NIVEAU : %d CREDITS"),
						UnlockCost)));
	}

	if (LevelButtonLabel)
	{
		if (TargetLevel == CurrentLevel)
		{
			LevelButtonLabel->SetText(
				FText::FromString(TEXT("NIVEAU ACTUEL")));
		}
		else if (TransitionCost > 0)
		{
			LevelButtonLabel->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("AMELIORER  -%d"),
						TransitionCost)));
		}
		else
		{
			LevelButtonLabel->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("DOWNGRADE  +%d"),
						-TransitionCost)));
		}
	}

	const bool bCanAfford =
		TransitionCost <= 0 ||
		BotanicusController->GetAvailableFunds() >= TransitionCost;
	const bool bCanChange =
		TargetLevel != CurrentLevel &&
		Workbench->CanChangeWorkbenchLevel(TargetLevel) &&
		bCanAfford;
	if (LevelButton)
	{
		LevelButton->SetIsEnabled(bCanChange);
		LevelButton->SetBackgroundColor(
			TargetLevel < CurrentLevel
				? FLinearColor(0.72f, 0.31f, 0.08f, 1.0f)
				: FLinearColor(0.07f, 0.46f, 0.40f, 1.0f));
	}
}

void UBotanicusWorkbenchLevelRowWidget::HandleLevelClicked()
{
	if (BotanicusController && IsValid(Workbench))
	{
		BotanicusController->RequestPreparationWorkbenchLevel(
			Workbench,
			TargetLevel);
	}
}

void UBotanicusWorkbenchUpgradeWidget::InitializeWithWorkbench(
	ABotanicusPlayerController* InController,
	ABotanicusPreparationWorkbenchActor* InWorkbench)
{
	BotanicusController = InController;
	Workbench = InWorkbench;
	RebuildLevelRows();
	Refresh();
}

void UBotanicusWorkbenchUpgradeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildLayout();
}

void UBotanicusWorkbenchUpgradeWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.15f)
	{
		RefreshAccumulator = 0.0f;
		Refresh();
	}
}

void UBotanicusWorkbenchUpgradeWidget::BuildLayout()
{
	UCanvasPanel* Canvas =
		WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(
		FLinearColor(0.005f, 0.012f, 0.015f, 0.70f));
	UCanvasPanelSlot* BackdropSlot =
		Canvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(760.0f);
	PanelSize->SetHeightOverride(680.0f);
	UCanvasPanelSlot* PanelSlot =
		Canvas->AddChildToCanvas(PanelSize);
	PanelSlot->SetAnchors(FAnchors(0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(760.0f, 680.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(
		FLinearColor(0.018f, 0.075f, 0.09f, 0.99f));
	Panel->SetPadding(FMargin(24.0f));
	PanelSize->AddChild(Panel);

	UVerticalBox* Content =
		WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(
		FText::FromString(
			TEXT("AMELIORATIONS DE L'ATELIER")));
	Title->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.50f, 0.95f, 0.80f, 1.0f)));
	Title->SetJustification(ETextJustify::Center);
	SetWorkbenchTextSize(Title, 28);
	Content->AddChildToVerticalBox(Title);

	SummaryLabel = WidgetTree->ConstructWidget<UTextBlock>();
	SummaryLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	SummaryLabel->SetJustification(ETextJustify::Center);
	SummaryLabel->SetMargin(FMargin(0.0f, 8.0f, 0.0f, 16.0f));
	SetWorkbenchTextSize(SummaryLabel, 16);
	Content->AddChildToVerticalBox(SummaryLabel);

	LevelsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	UVerticalBoxSlot* LevelsSlot =
		Content->AddChildToVerticalBox(LevelsBox);
	LevelsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	CloseButton = WidgetTree->ConstructWidget<UButton>();
	CloseButton->SetBackgroundColor(
		FLinearColor(0.18f, 0.24f, 0.26f, 1.0f));
	UTextBlock* CloseLabel =
		WidgetTree->ConstructWidget<UTextBlock>();
	CloseLabel->SetText(FText::FromString(TEXT("FERMER")));
	CloseLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	CloseLabel->SetMargin(FMargin(25.0f, 10.0f));
	SetWorkbenchTextSize(CloseLabel, 15);
	CloseButton->AddChild(CloseLabel);
	UVerticalBoxSlot* CloseSlot =
		Content->AddChildToVerticalBox(CloseButton);
	CloseSlot->SetHorizontalAlignment(HAlign_Center);
	CloseSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	CloseButton->OnClicked.AddDynamic(
		this,
		&UBotanicusWorkbenchUpgradeWidget::HandleCloseClicked);
}

void UBotanicusWorkbenchUpgradeWidget::RebuildLevelRows()
{
	if (!LevelsBox || !BotanicusController || !IsValid(Workbench))
	{
		return;
	}

	LevelsBox->ClearChildren();
	LevelRows.Reset();
	for (int32 Level = 1; Level <= 5; ++Level)
	{
		UBotanicusWorkbenchLevelRowWidget* Row =
			CreateWidget<UBotanicusWorkbenchLevelRowWidget>(
				GetOwningPlayer(),
				UBotanicusWorkbenchLevelRowWidget::StaticClass());
		if (!Row)
		{
			continue;
		}
		Row->InitializeRow(
			BotanicusController,
			Workbench,
			Level);
		UVerticalBoxSlot* RowSlot =
			LevelsBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(
			FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		LevelRows.Add(Row);
	}
}

void UBotanicusWorkbenchUpgradeWidget::Refresh()
{
	if (!IsValid(Workbench) || !BotanicusController)
	{
		if (BotanicusController)
		{
			BotanicusController->ClosePreparationWorkbenchUpgrade();
		}
		return;
	}

	if (SummaryLabel)
	{
		SummaryLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("NIVEAU ACTUEL : %d     CREDITS : %d"),
					Workbench->GetWorkbenchLevel(),
					BotanicusController->GetAvailableFunds())));
	}
	for (UBotanicusWorkbenchLevelRowWidget* Row : LevelRows)
	{
		if (Row)
		{
			Row->Refresh();
		}
	}
}

void UBotanicusWorkbenchUpgradeWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ClosePreparationWorkbenchUpgrade();
	}
}
