// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusCrosshairWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "UI/BotanicusHudStyle.h"

void UBotanicusCrosshairWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(256.0f);
		SizeBox->SetHeightOverride(256.0f);
		CrosshairImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("CrosshairImage"));
		CrosshairImage->SetBrushFromTexture(
			BotanicusHudStyle::LoadTexture(TEXT("T_HUD_Crosshair")), true);
		SizeBox->SetContent(CrosshairImage);
		WidgetTree->RootWidget = SizeBox;
	}
	else if (WidgetTree)
	{
		CrosshairImage = Cast<UImage>(
			WidgetTree->FindWidget(TEXT("CrosshairImage")));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}
