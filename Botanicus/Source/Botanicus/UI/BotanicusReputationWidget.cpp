// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusReputationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusGameState.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

void UBotanicusReputationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshReputation();
}

void UBotanicusReputationWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshReputation();
}

void UBotanicusReputationWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
	OverlaySlot->SetPosition(FVector2D::ZeroVector);
	OverlaySlot->SetSize(FVector2D(232.0f, 54.0f));

	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
		TEXT("T_HUD_ReputationBackground")), true);
	if (UOverlaySlot* BackgroundOverlaySlot =
		Overlay->AddChildToOverlay(Background))
	{
		BackgroundOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
	if (UOverlaySlot* ContentOverlaySlot =
		Overlay->AddChildToOverlay(Content))
	{
		ContentOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		ContentOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	UImage* Icon = WidgetTree->ConstructWidget<UImage>();
	Icon->SetBrushFromTexture(
		BotanicusHudStyle::LoadTexture(TEXT("T_HUD_Star")), true);
	UCanvasPanelSlot* IconSlot = Content->AddChildToCanvas(Icon);
	IconSlot->SetPosition(FVector2D(9.0f, 8.0f));
	IconSlot->SetSize(FVector2D(38.0f, 38.0f));

	ReputationLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ReputationLabel->SetColorAndOpacity(FSlateColor(
		FLinearColor(0.96f, 0.93f, 0.82f, 1.0f)));
	ReputationLabel->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 19, TEXT("Bold")));
	ReputationLabel->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* ValueSlot =
		Content->AddChildToCanvas(ReputationLabel);
	ValueSlot->SetPosition(FVector2D(48.0f, 10.0f));
	ValueSlot->SetSize(FVector2D(173.0f, 34.0f));
}

void UBotanicusReputationWidget::RefreshReputation()
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!ReputationLabel || !GameState)
	{
		return;
	}

	const int32 Reputation = GameState->GetShopReputationPoints();
	if (Reputation == LastDisplayedReputation)
	{
		return;
	}
	LastDisplayedReputation = Reputation;
	ReputationLabel->SetText(FText::FromString(FString::Printf(
		TEXT("%.2f/5"), Reputation / 100.0f)));
}
