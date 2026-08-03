// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusInteractionTargetWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UBotanicusInteractionTargetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(340.0f);
		SizeBox->SetHeightOverride(40.0f);

		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(12.0f, 5.0f));
		Background->SetBrushColor(
			FLinearColor(0.015f, 0.06f, 0.11f, 0.88f));
		SizeBox->SetContent(Background);

		TargetNameText =
			WidgetTree->ConstructWidget<UTextBlock>();
		TargetNameText->SetJustification(ETextJustify::Center);
		TargetNameText->SetColorAndOpacity(
			FLinearColor(0.22f, 0.70f, 1.0f, 1.0f));
		TargetNameText->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			16,
			TEXT("Bold")));
		Background->SetContent(TargetNameText);
		WidgetTree->RootWidget = SizeBox;
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusInteractionTargetWidget::SetTargetName(
	const FText& TargetName)
{
	if (!TargetNameText || TargetName.IsEmpty())
	{
		ClearTarget();
		return;
	}
	TargetNameText->SetText(TargetName);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusInteractionTargetWidget::ClearTarget()
{
	if (TargetNameText)
	{
		TargetNameText->SetText(FText::GetEmpty());
	}
	SetVisibility(ESlateVisibility::Collapsed);
}
