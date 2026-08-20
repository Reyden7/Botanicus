// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusOutdoorEnvironmentWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusGameState.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

namespace
{
void AddFallbackEnvironmentEntry(
	UWidgetTree* Tree,
	UCanvasPanel* Root,
	const TCHAR* TextureName,
	const TCHAR* TextName,
	float X,
	UTextBlock*& OutText)
{
	UImage* Icon = Tree->ConstructWidget<UImage>();
	Icon->SetBrushFromTexture(
		BotanicusHudStyle::LoadTexture(TextureName), true);
	UCanvasPanelSlot* IconSlot = Root->AddChildToCanvas(Icon);
	IconSlot->SetPosition(FVector2D(X, 7.0f));
	IconSlot->SetSize(FVector2D(52.0f, 52.0f));

	OutText = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(TextName));
	OutText->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(), 17, TEXT("Bold")));
	OutText->SetColorAndOpacity(
		FSlateColor(BotanicusHudStyle::PrimaryText()));
	OutText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* TextSlot = Root->AddChildToCanvas(OutText);
	TextSlot->SetPosition(FVector2D(X + 45.0f, 17.0f));
	TextSlot->SetSize(FVector2D(91.0f, 36.0f));
}
}

void UBotanicusOutdoorEnvironmentWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshEnvironment();
}

void UBotanicusOutdoorEnvironmentWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
	if (RefreshAccumulator >= 0.1f)
	{
		RefreshAccumulator = 0.0f;
		RefreshEnvironment();
	}
}

void UBotanicusOutdoorEnvironmentWidget::BuildFallbackLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	UTextBlock* Temperature = nullptr;
	UTextBlock* Humidity = nullptr;
	UTextBlock* Luminosity = nullptr;
	AddFallbackEnvironmentEntry(
		WidgetTree, Root, TEXT("T_HUD_Temperature"),
		TEXT("TemperatureText"), 0.0f, Temperature);
	AddFallbackEnvironmentEntry(
		WidgetTree, Root, TEXT("T_HUD_AirHumidity"),
		TEXT("AirHumidityText"), 140.0f, Humidity);
	AddFallbackEnvironmentEntry(
		WidgetTree, Root, TEXT("T_HUD_Luminosity"),
		TEXT("LuminosityText"), 280.0f, Luminosity);
	TemperatureText = Temperature;
	AirHumidityText = Humidity;
	LuminosityText = Luminosity;
}

void UBotanicusOutdoorEnvironmentWidget::RefreshEnvironment()
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}
	if (TemperatureText)
	{
		TemperatureText->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f C"), GameState->GetOutdoorTemperatureCelsius())));
	}
	if (AirHumidityText)
	{
		AirHumidityText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f%%"), GameState->GetOutdoorAirHumidityPercent())));
	}
	if (LuminosityText)
	{
		LuminosityText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f%%"), GameState->GetOutdoorLuminosityPercent())));
	}
}
