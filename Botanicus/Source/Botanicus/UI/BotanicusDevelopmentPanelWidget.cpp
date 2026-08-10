// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusDevelopmentPanelWidget.h"

#include "BotanicusPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace
{
void SetDevelopmentTextSize(UTextBlock* Text, int32 Size)
{
	if (Text)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}
}
}

void UBotanicusDevelopmentPanelWidget::InitializeWithController(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	Refresh();
}

void UBotanicusDevelopmentPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildLayout();
}

void UBotanicusDevelopmentPanelWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

FReply UBotanicusDevelopmentPanelWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::N ||
		InKeyEvent.GetKey() == EKeys::F1 ||
		InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (BotanicusController)
		{
			BotanicusController->ToggleDevelopmentPanel();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UBotanicusDevelopmentPanelWidget::
	NativeOnPreviewMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnPreviewMouseButtonDown(
			InGeometry,
			InMouseEvent);
	}

	const FVector2D ScreenPosition =
		InMouseEvent.GetScreenSpacePosition();
	const auto IsButtonUnderPointer =
		[&ScreenPosition](const UButton* Button)
		{
			return Button &&
				Button->GetVisibility() == ESlateVisibility::Visible &&
				Button->GetIsEnabled() &&
				Button->GetCachedGeometry().IsUnderLocation(
					ScreenPosition);
		};

	if (IsButtonUnderPointer(DecreaseLevelButton))
	{
		HandleDecreaseLevelClicked();
		return FReply::Handled();
	}
	if (IsButtonUnderPointer(IncreaseLevelButton))
	{
		HandleIncreaseLevelClicked();
		return FReply::Handled();
	}
	if (IsButtonUnderPointer(AddCreditsButton))
	{
		HandleAddCreditsClicked();
		return FReply::Handled();
	}
	if (IsButtonUnderPointer(TimeScaleButton))
	{
		HandleTimeScaleClicked();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(
		InGeometry,
		InMouseEvent);
}

void UBotanicusDevelopmentPanelWidget::BuildLayout()
{
	UCanvasPanel* Root =
		WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));
	UCanvasPanelSlot* BackdropSlot =
		Root->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(
		FLinearColor(0.075f, 0.035f, 0.015f, 0.99f));
	Panel->SetPadding(FMargin(24.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(560.0f, 440.0f));

	UVerticalBox* Column =
		WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	UHorizontalBox* Header =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("COMMANDES DEVELOPPEUR")));
	Title->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.58f, 0.16f, 1.0f)));
	SetDevelopmentTextSize(Title, 25);
	UHorizontalBoxSlot* TitleSlot =
		Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* CloseButton =
		WidgetTree->ConstructWidget<UButton>();
	CloseButton->SetBackgroundColor(
		FLinearColor(0.58f, 0.12f, 0.08f, 1.0f));
	UTextBlock* CloseText =
		WidgetTree->ConstructWidget<UTextBlock>();
	CloseText->SetText(FText::FromString(TEXT("FERMER [TAB]")));
	CloseText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CloseText->SetMargin(FMargin(12.0f, 7.0f));
	SetDevelopmentTextSize(CloseText, 13);
	CloseButton->AddChild(CloseText);
	Header->AddChildToHorizontalBox(CloseButton);
	CloseButton->OnClicked.AddDynamic(
		this,
		&UBotanicusDevelopmentPanelWidget::HandleCloseClicked);

	ShopLevelLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ShopLevelLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.45f, 0.82f, 1.0f, 1.0f)));
	ShopLevelLabel->SetMargin(FMargin(0.0f, 24.0f, 0.0f, 8.0f));
	SetDevelopmentTextSize(ShopLevelLabel, 19);
	Column->AddChildToVerticalBox(ShopLevelLabel);

	UHorizontalBox* LevelButtons =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(LevelButtons);
	auto AddButton =
		[this](UHorizontalBox* Row,
			const FString& Label,
			const FLinearColor& Color)
		{
			UButton* Button =
				WidgetTree->ConstructWidget<UButton>();
			Button->SetBackgroundColor(Color);
			UTextBlock* Text =
				WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetText(FText::FromString(Label));
			Text->SetColorAndOpacity(
				FSlateColor(FLinearColor::White));
			Text->SetMargin(FMargin(14.0f, 9.0f));
			Text->SetJustification(ETextJustify::Center);
			SetDevelopmentTextSize(Text, 14);
			Button->AddChild(Text);
			UHorizontalBoxSlot* Slot =
				Row->AddChildToHorizontalBox(Button);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(3.0f));
			return Button;
		};
	DecreaseLevelButton = AddButton(
		LevelButtons,
		TEXT("NIVEAU BOUTIQUE -1"),
		FLinearColor(0.55f, 0.24f, 0.08f, 1.0f));
	DecreaseLevelButton->OnPressed.AddDynamic(
		this,
		&UBotanicusDevelopmentPanelWidget::HandleDecreaseLevelClicked);
	IncreaseLevelButton = AddButton(
		LevelButtons,
		TEXT("NIVEAU BOUTIQUE +1"),
		FLinearColor(0.10f, 0.42f, 0.72f, 1.0f));
	IncreaseLevelButton->OnPressed.AddDynamic(
		this,
		&UBotanicusDevelopmentPanelWidget::HandleIncreaseLevelClicked);

	FundsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FundsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.82f, 0.28f, 1.0f)));
	FundsLabel->SetMargin(FMargin(0.0f, 20.0f, 0.0f, 8.0f));
	SetDevelopmentTextSize(FundsLabel, 18);
	Column->AddChildToVerticalBox(FundsLabel);
	UHorizontalBox* CreditRow =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(CreditRow);
	AddCreditsButton = AddButton(
		CreditRow,
		TEXT("AJOUTER 100 CREDITS"),
		FLinearColor(0.82f, 0.36f, 0.06f, 1.0f));
	AddCreditsButton->OnPressed.AddDynamic(
		this,
		&UBotanicusDevelopmentPanelWidget::HandleAddCreditsClicked);

	TimeScaleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	TimeScaleLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.80f, 0.58f, 1.0f, 1.0f)));
	TimeScaleLabel->SetMargin(FMargin(0.0f, 20.0f, 0.0f, 8.0f));
	SetDevelopmentTextSize(TimeScaleLabel, 18);
	Column->AddChildToVerticalBox(TimeScaleLabel);
	UHorizontalBox* TimeRow =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(TimeRow);
	TimeScaleButton = AddButton(
		TimeRow,
		TEXT("CHANGER LA VITESSE DU TEMPS"),
		FLinearColor(0.48f, 0.20f, 0.72f, 1.0f));
	TimeScaleButton->OnPressed.AddDynamic(
		this,
		&UBotanicusDevelopmentPanelWidget::HandleTimeScaleClicked);

	ActionFeedbackLabel =
		WidgetTree->ConstructWidget<UTextBlock>();
	ActionFeedbackLabel->SetText(
		FText::FromString(
			TEXT("CLIQUEZ SUR UNE COMMANDE DE TEST")));
	ActionFeedbackLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.55f, 1.0f, 0.68f, 1.0f)));
	ActionFeedbackLabel->SetMargin(
		FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	ActionFeedbackLabel->SetJustification(ETextJustify::Center);
	SetDevelopmentTextSize(ActionFeedbackLabel, 12);
	Column->AddChildToVerticalBox(ActionFeedbackLabel);
}

void UBotanicusDevelopmentPanelWidget::Refresh()
{
	if (!BotanicusController)
	{
		return;
	}
	if (ShopLevelLabel)
	{
		ShopLevelLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"BOUTIQUE NIVEAU %d  |  CAISSES AUTO %d / %d"),
					BotanicusController->GetMainShopLevel(),
					BotanicusController->
						GetSelfCheckoutOwnedOrOrderedCount(),
					BotanicusController->GetSelfCheckoutLimit())));
	}
	if (FundsLabel)
	{
		FundsLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("CAISSE COMMUNE : %d CREDITS"),
					BotanicusController->GetAvailableFunds())));
	}
	if (TimeScaleLabel)
	{
		TimeScaleLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("VITESSE DE SIMULATION : x%.0f"),
					BotanicusController->
						GetDevelopmentTimeScale())));
	}
}

void UBotanicusDevelopmentPanelWidget::HandleAddCreditsClicked()
{
	if (BotanicusController)
	{
		if (ActionFeedbackLabel)
		{
			ActionFeedbackLabel->SetText(
				FText::FromString(
					TEXT("COMMANDE RECUE : +100 CREDITS")));
		}
		BotanicusController->AddTestCredits();
	}
}

void UBotanicusDevelopmentPanelWidget::HandleTimeScaleClicked()
{
	if (BotanicusController)
	{
		if (ActionFeedbackLabel)
		{
			ActionFeedbackLabel->SetText(
				FText::FromString(
					TEXT("COMMANDE RECUE : VITESSE DU TEMPS")));
		}
		BotanicusController->CycleDevelopmentTimeScale();
	}
}

void UBotanicusDevelopmentPanelWidget::HandleDecreaseLevelClicked()
{
	if (BotanicusController)
	{
		if (ActionFeedbackLabel)
		{
			ActionFeedbackLabel->SetText(
				FText::FromString(
					TEXT("COMMANDE RECUE : NIVEAU -1")));
		}
		BotanicusController->
			AdjustMainShopLevelForDevelopment(-1);
	}
}

void UBotanicusDevelopmentPanelWidget::HandleIncreaseLevelClicked()
{
	if (BotanicusController)
	{
		if (ActionFeedbackLabel)
		{
			ActionFeedbackLabel->SetText(
				FText::FromString(
					TEXT("COMMANDE RECUE : NIVEAU +1")));
		}
		BotanicusController->
			AdjustMainShopLevelForDevelopment(1);
	}
}

void UBotanicusDevelopmentPanelWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleDevelopmentPanel();
	}
}
