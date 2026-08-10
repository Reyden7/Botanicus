// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusHudMessageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

void UBotanicusHudMessageWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusHudMessageWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() != ESlateVisibility::Collapsed &&
		HideAtTime > 0.0 &&
		FPlatformTime::Seconds() >= HideAtTime)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBotanicusHudMessageWidget::BuildLayout()
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(460.0f);
	Size->SetHeightOverride(108.0f);
	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
	Size->SetContent(Overlay);
	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
		TEXT("T_HUD_ActionBackground")), true);
	Overlay->AddChildToOverlay(Background);
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
	Overlay->AddChildToOverlay(Content);
	UTextBlock* NoticeIcon = WidgetTree->ConstructWidget<UTextBlock>();
	NoticeIcon->SetText(FText::FromString(TEXT("!")));
	NoticeIcon->SetJustification(ETextJustify::Center);
	NoticeIcon->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.12f, 0.14f, 0.11f, 1.0f)));
	NoticeIcon->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 20, TEXT("Bold")));
	UCanvasPanelSlot* IconSlot = Content->AddChildToCanvas(NoticeIcon);
	IconSlot->SetPosition(FVector2D(31.0f, 31.0f));
	IconSlot->SetSize(FVector2D(66.0f, 45.0f));

	MessageLabel = WidgetTree->ConstructWidget<UTextBlock>();
	MessageLabel->SetJustification(ETextJustify::Center);
	MessageLabel->SetAutoWrapText(true);
	MessageLabel->SetColorAndOpacity(
		FSlateColor(BotanicusHudStyle::PrimaryText()));
	MessageLabel->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 15, TEXT("Bold")));
	UCanvasPanelSlot* MessageSlot = Content->AddChildToCanvas(MessageLabel);
	MessageSlot->SetPosition(FVector2D(112.0f, 23.0f));
	MessageSlot->SetSize(FVector2D(310.0f, 64.0f));
	WidgetTree->RootWidget = Size;
}

void UBotanicusHudMessageWidget::ShowMessage(
	const FText& Message,
	float Lifetime)
{
	if (!MessageLabel || Message.IsEmpty())
	{
		return;
	}
	MessageLabel->SetText(Message);
	HideAtTime = FPlatformTime::Seconds() +
		FMath::Max(Lifetime, 1.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
