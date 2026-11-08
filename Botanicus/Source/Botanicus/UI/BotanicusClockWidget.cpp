// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusClockWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusGameState.h"
#include "BotanicusPlayerController.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
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
UTextBlock* AddClockText(
	UWidgetTree* WidgetTree,
	UCanvasPanel* Canvas,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 FontSize)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetColorAndOpacity(
		FSlateColor(BotanicusHudStyle::PrimaryText()));
	Text->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), FontSize, TEXT("Bold")));
	Text->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Text);
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetSize(Size);
	return Text;
}
}

void UBotanicusClockWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
    EnsureScheduleBackdrop();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshClock();
}

void UBotanicusClockWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshClock(InDeltaTime);
}

void UBotanicusClockWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UOverlay* ClockOverlay = WidgetTree->ConstructWidget<UOverlay>();
	UCanvasPanelSlot* ClockSlot = Root->AddChildToCanvas(ClockOverlay);
	ClockSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	ClockSlot->SetPosition(FVector2D::ZeroVector);
	ClockSlot->SetSize(FVector2D(420.0f, 143.0f));

	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetBrushFromTexture(
		BotanicusHudStyle::LoadTexture(TEXT("T_HUD_Calendar")), true);
	if (UOverlaySlot* BackgroundSlot =
			ClockOverlay->AddChildToOverlay(Background))
	{
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UCanvasPanel* TextCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	if (UOverlaySlot* TextCanvasSlot =
			ClockOverlay->AddChildToOverlay(TextCanvas))
	{
		TextCanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		TextCanvasSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ClockLabel = AddClockText(
		WidgetTree, TextCanvas,
		FVector2D(98.0f, 21.0f), FVector2D(128.0f, 42.0f), 20);
	TimeLabel = AddClockText(
		WidgetTree, TextCanvas,
		FVector2D(269.0f, 20.0f), FVector2D(116.0f, 43.0f), 21);

	ScheduleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ScheduleLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.86f, 0.86f, 0.72f, 1.0f)));
	ScheduleLabel->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 11, TEXT("Bold")));
	ScheduleLabel->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* ScheduleSlot =
		TextCanvas->AddChildToCanvas(ScheduleLabel);
	ScheduleSlot->SetPosition(FVector2D(95.0f, 90.0f));
	ScheduleSlot->SetSize(FVector2D(290.0f, 30.0f));

	FurnitureModeLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FurnitureModeLabel->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 12, TEXT("Bold")));
	FurnitureModeLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.82f, 0.20f, 1.0f)));
	FurnitureModeLabel->SetText(
		FText::FromString(TEXT("MODE MEUBLES [B] : ACTIF")));
	FurnitureModeLabel->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* FurnitureSlot =
		Root->AddChildToCanvas(FurnitureModeLabel);
	FurnitureSlot->SetPosition(FVector2D(116.0f, 151.0f));
	FurnitureSlot->SetAutoSize(true);
}

void UBotanicusClockWidget::EnsureScheduleBackdrop()
{
    if(!ScheduleLabel || ScheduleBackdrop || !WidgetTree)return;
    auto* Canvas=Cast<UCanvasPanel>(ScheduleLabel->GetParent());auto* OldSlot=Cast<UCanvasPanelSlot>(ScheduleLabel->Slot);
    if(!Canvas || !OldSlot)return;
    const FAnchorData Layout=OldSlot->GetLayout();const bool bAutoSize=OldSlot->GetAutoSize();const int32 Z=OldSlot->GetZOrder();
    ScheduleLabel->RemoveFromParent();ScheduleBackdrop=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("RuntimeScheduleBackdrop"));
    ScheduleBackdrop->SetPadding(FMargin(0));ScheduleBackdrop->SetHorizontalAlignment(HAlign_Fill);ScheduleBackdrop->SetVerticalAlignment(VAlign_Center);
    ScheduleBackdrop->SetContent(ScheduleLabel);auto* NewSlot=Canvas->AddChildToCanvas(ScheduleBackdrop);NewSlot->SetLayout(Layout);NewSlot->SetAutoSize(bAutoSize);NewSlot->SetZOrder(Z);
    ScheduleBackdrop->SetBrushColor(FLinearColor::Transparent);
}
void UBotanicusClockWidget::RefreshClock(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	const int32 TotalMinutes =
		FMath::FloorToInt(GameState->GetDayTimeMinutes()) % 1440;
	const int32 Hour = TotalMinutes / 60;
	const int32 Minute = TotalMinutes % 60;
	if (ClockLabel)
	{
		ClockLabel->SetText(FText::FromString(FString::Printf(
			TEXT("JOUR %d"), GameState->GetCurrentDayNumber())));
	}
	if (TimeLabel)
	{
		TimeLabel->SetText(FText::FromString(FString::Printf(
			TEXT("%02d:%02d"), Hour, Minute)));
	}
    const int32 Status=GameState->IsIllegalTradeActive() ? 2:GameState->IsMainShopOpen() ? 1:0;
    if(ScheduleLabel && Status!=LastStatus)
    {
        ScheduleLabel->SetText(FText::FromString(Status==2 ? TEXT("BOUTIQUE CLANDESTINE OUVERTE") : Status==1 ? TEXT("BOUTIQUE OUVERTE") : TEXT("BOUTIQUE FERMÉE")));
        ScheduleLabel->SetColorAndOpacity(FSlateColor(FLinearColor(.96f,.94f,.85f,1)));
        StatusFrom=StatusTint;StatusTarget=Status==2 ? FLinearColor(.24f,.018f,.05f,.96f) : Status==1 ? FLinearColor::Transparent : FLinearColor(.025f,.032f,.035f,.96f);
        StatusBlend=LastStatus<0 ? 1.f:0.f;LastStatus=Status;
    }
    StatusBlend=FMath::Clamp(StatusBlend+FMath::Max(0.f,DeltaSeconds)/.25f,0.f,1.f);StatusTint=FMath::Lerp(StatusFrom,StatusTarget,StatusBlend);
    if(ScheduleBackdrop)ScheduleBackdrop->SetBrushColor(StatusTint);
    if(ScheduleLabel)ScheduleLabel->SetRenderOpacity(1.f-.10f*FMath::Sin(PI*StatusBlend));

	if (FurnitureModeLabel)
	{
		const ABotanicusPlayerController* Controller =
			Cast<ABotanicusPlayerController>(GetOwningPlayer());
		FurnitureModeLabel->SetVisibility(
			Controller && Controller->IsFurnitureMoveModeActive()
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}
