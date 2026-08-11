// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusCarryProgressWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

void UBotanicusCarryProgressWidget::SetCarryProgress(float InProgress)
{
	CarryProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	InvalidateLayoutAndVolatility();
}

void UBotanicusCarryProgressWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(68.0f);
		SizeBox->SetHeightOverride(68.0f);
		WidgetTree->RootWidget = SizeBox;
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

int32 UBotanicusCarryProgressWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// The progress is rendered around the key inside WBP_HUD_Interaction.
	// This legacy widget remains as the shared progress source for existing
	// held actions, but no longer paints a second indicator near the reticle.
	return Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
}
