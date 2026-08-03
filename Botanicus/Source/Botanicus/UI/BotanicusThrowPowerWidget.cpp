// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusThrowPowerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"

void UBotanicusThrowPowerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(360.0f);
		SizeBox->SetHeightOverride(82.0f);

		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetPadding(FMargin(14.0f, 8.0f));
		Border->SetBrushColor(
			FLinearColor(0.025f, 0.07f, 0.09f, 0.92f));
		SizeBox->SetContent(Border);

		UVerticalBox* Container =
			WidgetTree->ConstructWidget<UVerticalBox>();
		Border->SetContent(Container);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
		Title->SetText(
			FText::FromString(TEXT("PUISSANCE DU LANCER")));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(
			FSlateColor(
				FLinearColor(0.35f, 0.8f, 1.0f, 1.0f)));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 15;
		Title->SetFont(TitleFont);
		if (UVerticalBoxSlot* TitleSlot =
			Container->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		PowerBar = WidgetTree->ConstructWidget<UProgressBar>();
		PowerBar->SetFillColorAndOpacity(
			FLinearColor(0.15f, 0.7f, 1.0f, 1.0f));
		if (UVerticalBoxSlot* BarSlot =
			Container->AddChildToVerticalBox(PowerBar))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 3.0f));
		}

		PercentageText =
			WidgetTree->ConstructWidget<UTextBlock>();
		PercentageText->SetJustification(ETextJustify::Center);
		PercentageText->SetColorAndOpacity(
			FSlateColor(FLinearColor::White));
		FSlateFontInfo PercentageFont =
			PercentageText->GetFont();
		PercentageFont.Size = 18;
		PercentageText->SetFont(PercentageFont);
		if (UVerticalBoxSlot* PercentageSlot =
			Container->AddChildToVerticalBox(PercentageText))
		{
			PercentageSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		WidgetTree->RootWidget = SizeBox;
	}

	SetThrowPower(0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusThrowPowerWidget::SetThrowPower(
	float NormalizedPower)
{
	const float ClampedPower =
		FMath::Clamp(NormalizedPower, 0.0f, 1.0f);
	if (PowerBar)
	{
		PowerBar->SetPercent(ClampedPower);
		PowerBar->SetFillColorAndOpacity(
			FLinearColor::LerpUsingHSV(
				FLinearColor(0.15f, 0.7f, 1.0f, 1.0f),
				FLinearColor(1.0f, 0.55f, 0.08f, 1.0f),
				ClampedPower));
	}
	if (PercentageText)
	{
		PercentageText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("%d %%"),
					FMath::RoundToInt(ClampedPower * 100.0f))));
	}
}
