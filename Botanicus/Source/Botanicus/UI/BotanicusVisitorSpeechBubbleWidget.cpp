// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusVisitorSpeechBubbleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UBotanicusVisitorSpeechBubbleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
}

void UBotanicusVisitorSpeechBubbleWidget::BuildLayout()
{
	USizeBox* BubbleSize = WidgetTree->ConstructWidget<USizeBox>();
	BubbleSize->SetWidthOverride(300.0f);
	BubbleSize->SetMinDesiredHeight(74.0f);
	WidgetTree->RootWidget = BubbleSize;

	UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>();
	Bubble->SetBrushColor(FLinearColor(1.0f, 0.97f, 0.86f, 0.98f));
	Bubble->SetPadding(FMargin(16.0f, 10.0f));
	BubbleSize->AddChild(Bubble);

	SpeechLabel = WidgetTree->ConstructWidget<UTextBlock>();
	SpeechLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.08f, 0.055f, 0.025f, 1.0f)));
	SpeechLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 17));
	SpeechLabel->SetAutoWrapText(true);
	SpeechLabel->SetJustification(ETextJustify::Center);
	Bubble->SetContent(SpeechLabel);
}

void UBotanicusVisitorSpeechBubbleWidget::SetSpeech(
	const FString& NewSpeech)
{
	if (SpeechLabel)
	{
		SpeechLabel->SetText(FText::FromString(NewSpeech));
	}
}
