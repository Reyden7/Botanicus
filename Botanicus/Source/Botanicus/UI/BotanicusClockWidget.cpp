// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusClockWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusPlayerController.h"
#include "BotanicusGameState.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

void UBotanicusClockWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	RefreshClock();
}

void UBotanicusClockWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshClock();
}

void UBotanicusClockWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.02f, 0.055f, 0.035f, 0.90f));
	Panel->SetPadding(FMargin(14.0f, 9.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	PanelSlot->SetPosition(FVector2D(24.0f, 24.0f));
	PanelSlot->SetSize(FVector2D(300.0f, 108.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	ClockLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ClockLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 25));
	ClockLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.82f, 0.28f, 1.0f)));
	Column->AddChildToVerticalBox(ClockLabel);

	ScheduleLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ScheduleLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 13));
	ScheduleLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.70f, 0.86f, 0.74f, 1.0f)));
	Column->AddChildToVerticalBox(ScheduleLabel);

	FurnitureModeLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FurnitureModeLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
	FurnitureModeLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.82f, 0.12f, 1.0f)));
	FurnitureModeLabel->SetText(
		FText::FromString(TEXT("MODE MEUBLES [B] : ACTIF")));
	FurnitureModeLabel->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(FurnitureModeLabel);
}

void UBotanicusClockWidget::RefreshClock()
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
		ClockLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("JOUR %d   %02d:%02d"),
					GameState->GetCurrentDayNumber(),
					Hour,
					Minute)));
	}
	if (ScheduleLabel)
	{
		ScheduleLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("%s  |  08:00 - 19:00  |  DEBUG x%.0f"),
					GameState->IsMainShopOpen()
						? TEXT("MAGASIN OUVERT")
						: TEXT("MAGASIN FERME"),
					GameState->GetDevelopmentTimeScale())));
		ScheduleLabel->SetColorAndOpacity(
			FSlateColor(
				GameState->IsMainShopOpen()
					? FLinearColor(0.30f, 1.0f, 0.45f, 1.0f)
					: FLinearColor(1.0f, 0.48f, 0.28f, 1.0f)));
	}
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
