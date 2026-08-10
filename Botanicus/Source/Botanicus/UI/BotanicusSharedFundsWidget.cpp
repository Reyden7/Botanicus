// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusSharedFundsWidget.h"

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

namespace
{
UTextBlock* CreateStatusPlate(
	UWidgetTree* WidgetTree,
	UCanvasPanel* Root,
	const TCHAR* BackgroundAsset,
	const TCHAR* IconAsset,
	const FVector2D& Size,
	float IconSize,
	const FLinearColor& TextColor)
{
	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
	OverlaySlot->SetPosition(FVector2D::ZeroVector);
	OverlaySlot->SetSize(Size);

	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(
		BotanicusHudStyle::LoadTexture(BackgroundAsset), true);
	if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Background))
	{
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
	if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Content))
	{
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}

	UImage* Icon = WidgetTree->ConstructWidget<UImage>();
	Icon->SetBrushFromTexture(
		BotanicusHudStyle::LoadTexture(IconAsset), true);
	UCanvasPanelSlot* IconSlot = Content->AddChildToCanvas(Icon);
	IconSlot->SetPosition(FVector2D(9.0f, (Size.Y - IconSize) * 0.5f));
	IconSlot->SetSize(FVector2D(IconSize, IconSize));

	UTextBlock* Value = WidgetTree->ConstructWidget<UTextBlock>();
	Value->SetColorAndOpacity(FSlateColor(TextColor));
	Value->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 19, TEXT("Bold")));
	Value->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* ValueSlot = Content->AddChildToCanvas(Value);
	ValueSlot->SetPosition(FVector2D(48.0f, (Size.Y - 34.0f) * 0.5f));
	ValueSlot->SetSize(FVector2D(Size.X - 59.0f, 34.0f));
	return Value;
}
}

void UBotanicusSharedFundsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshFunds();
}

void UBotanicusSharedFundsWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFunds();
}

void UBotanicusSharedFundsWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	FundsLabel = CreateStatusPlate(
		WidgetTree,
		Root,
		TEXT("T_HUD_CreditsBackground"),
		TEXT("T_HUD_Credit"),
		FVector2D(232.0f, 72.0f),
		42.0f,
		FLinearColor(0.96f, 0.93f, 0.82f, 1.0f));
}

void UBotanicusSharedFundsWidget::RefreshFunds()
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!FundsLabel || !GameState)
	{
		return;
	}

	const int32 Funds = GameState->GetSharedFunds();
	if (Funds == LastDisplayedFunds)
	{
		return;
	}
	LastDisplayedFunds = Funds;
	FundsLabel->SetText(FText::AsNumber(Funds));
}
