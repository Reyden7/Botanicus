// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusPlantEnvironmentDebugWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

namespace
{
	UTextBlock* AddDebugText(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const TCHAR* Name,
		const TCHAR* Preview,
		float VerticalPosition)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(Name));
		Text->SetText(FText::FromString(Preview));
		Text->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.25f, 1.0f));
		Text->SetFont(FSlateFontInfo(
			LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
			18,
			TEXT("Bold")));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Text);
		Slot->SetPosition(FVector2D(4.0f, VerticalPosition));
		Slot->SetSize(FVector2D(252.0f, 34.0f));
		return Text;
	}
}

void UBotanicusPlantEnvironmentDebugWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(260.0f);
		Root->SetHeightOverride(108.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetContent(Canvas);
		TemperatureText = AddDebugText(
			WidgetTree, Canvas, TEXT("TemperatureText"), TEXT("TEMP : 20.0 C"), 2.0f);
		AirHumidityText = AddDebugText(
			WidgetTree, Canvas, TEXT("AirHumidityText"), TEXT("HUMIDITE : 50%"), 37.0f);
		LuminosityText = AddDebugText(
			WidgetTree, Canvas, TEXT("LuminosityText"), TEXT("LUMINOSITE : 50%"), 72.0f);
		WidgetTree->RootWidget = Root;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusPlantEnvironmentDebugWidget::SetEnvironmentState(
	const FBotanicusPlantEnvironmentState& EnvironmentState)
{
	if (!EnvironmentState.bEnvironmentAvailable)
	{
		SetDiagnosticLine(
			TemperatureText,
			TEXT("TEMP : --"),
			EBotanicusPlantEnvironmentCondition::Unavailable);
		SetDiagnosticLine(
			AirHumidityText,
			TEXT("HUMIDITE : --"),
			EBotanicusPlantEnvironmentCondition::Unavailable);
		SetDiagnosticLine(
			LuminosityText,
			TEXT("LUMINOSITE : --"),
			EBotanicusPlantEnvironmentCondition::Unavailable);
		return;
	}

	SetDiagnosticLine(
		TemperatureText,
		FString::Printf(
			TEXT("TEMP : %.1f C"), EnvironmentState.TemperatureCelsius),
		EnvironmentState.TemperatureCondition);
	SetDiagnosticLine(
		AirHumidityText,
		FString::Printf(
			TEXT("HUMIDITE : %.0f%%"), EnvironmentState.AirHumidityPercent),
		EnvironmentState.AirHumidityCondition);
	SetDiagnosticLine(
		LuminosityText,
		FString::Printf(
			TEXT("LUMINOSITE : %.0f%%"), EnvironmentState.LuminosityPercent),
		EnvironmentState.LuminosityCondition);
}

void UBotanicusPlantEnvironmentDebugWidget::SetDiagnosticLine(
	UTextBlock* TextBlock,
	const FString& Value,
	EBotanicusPlantEnvironmentCondition Condition)
{
	if (!TextBlock)
	{
		return;
	}
	TextBlock->SetText(FText::FromString(Value));
	TextBlock->SetColorAndOpacity(
		Condition == EBotanicusPlantEnvironmentCondition::Ideal
			? FLinearColor(0.15f, 1.0f, 0.2f, 1.0f)
			: FLinearColor(1.0f, 0.08f, 0.05f, 1.0f));
}
