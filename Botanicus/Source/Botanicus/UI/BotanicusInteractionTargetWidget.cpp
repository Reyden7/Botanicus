// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusInteractionTargetWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

void UBotanicusInteractionTargetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(310.0f);
		Size->SetHeightOverride(108.0f);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		Size->SetContent(Overlay);
		UImage* Background = WidgetTree->ConstructWidget<UImage>();
		Background->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
			TEXT("T_HUD_ActionBackground")), true);
		if (UOverlaySlot* BackgroundOverlaySlot =
				Overlay->AddChildToOverlay(Background))
		{
			BackgroundOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}

		UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
		Overlay->AddChildToOverlay(Content);

		UTextBlock* KeyLabel = WidgetTree->ConstructWidget<UTextBlock>();
		KeyLabel->SetText(FText::FromString(TEXT("E")));
		KeyLabel->SetJustification(ETextJustify::Center);
		KeyLabel->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.12f, 0.14f, 0.11f, 1.0f)));
		KeyLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 19, TEXT("Bold")));
		UCanvasPanelSlot* KeySlot = Content->AddChildToCanvas(KeyLabel);
		KeySlot->SetPosition(FVector2D(22.0f, 32.0f));
		KeySlot->SetSize(FVector2D(48.0f, 43.0f));

		UTextBlock* ActionLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ActionLabel->SetText(FText::FromString(TEXT("INTERAGIR")));
		ActionLabel->SetColorAndOpacity(
			FSlateColor(BotanicusHudStyle::PrimaryText()));
		ActionLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 14, TEXT("Bold")));
		UCanvasPanelSlot* ActionSlot = Content->AddChildToCanvas(ActionLabel);
		ActionSlot->SetPosition(FVector2D(84.0f, 28.0f));
		ActionSlot->SetSize(FVector2D(190.0f, 28.0f));

		TargetNameText = WidgetTree->ConstructWidget<UTextBlock>();
		TargetNameText->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.75f, 0.80f, 0.69f, 1.0f)));
		TargetNameText->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 12));
		TargetNameText->SetAutoWrapText(true);
		UCanvasPanelSlot* NameSlot = Content->AddChildToCanvas(TargetNameText);
		NameSlot->SetPosition(FVector2D(84.0f, 54.0f));
		NameSlot->SetSize(FVector2D(190.0f, 38.0f));
		WidgetTree->RootWidget = Size;
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusInteractionTargetWidget::SetTargetName(const FText& TargetName)
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
