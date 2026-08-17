// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusStorageQuantityWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

namespace
{
	UCanvasPanelSlot* AddStorageQuantityToCanvas(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetZOrder(ZOrder);
		return Slot;
	}
}

void UBotanicusStorageQuantityWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(310.0f);
		SizeBox->SetHeightOverride(108.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		SizeBox->SetContent(Canvas);

		UImage* Background = WidgetTree->ConstructWidget<UImage>();
		Background->SetBrushFromTexture(
			BotanicusHudStyle::LoadTexture(TEXT("T_HUD_ActionBackground")),
			true);
		AddStorageQuantityToCanvas(Canvas, Background, FVector2D::ZeroVector,
			FVector2D(310.0f, 108.0f));

		InputIcon = WidgetTree->ConstructWidget<UImage>();
		AddStorageQuantityToCanvas(Canvas, InputIcon, FVector2D(22.0f, 32.0f),
			FVector2D(48.0f, 43.0f), 2);

		QuantityText = WidgetTree->ConstructWidget<UTextBlock>();
		QuantityText->SetColorAndOpacity(
			FSlateColor(BotanicusHudStyle::PrimaryText()));
		QuantityText->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 14, TEXT("Bold")));
		AddStorageQuantityToCanvas(Canvas, QuantityText, FVector2D(84.0f, 28.0f),
			FVector2D(190.0f, 28.0f), 2);

		InstructionText = WidgetTree->ConstructWidget<UTextBlock>();
		InstructionText->SetAutoWrapText(true);
		InstructionText->SetColorAndOpacity(FSlateColor(
			FLinearColor(0.75f, 0.80f, 0.69f, 1.0f)));
		InstructionText->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 12));
		AddStorageQuantityToCanvas(Canvas, InstructionText, FVector2D(84.0f, 54.0f),
			FVector2D(190.0f, 38.0f), 2);

		WidgetTree->RootWidget = SizeBox;
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusStorageQuantityWidget::SetQuantitySelection(
	const FText& ItemName,
	int32 Quantity,
	int32 MaximumQuantity,
	bool bStoring)
{
	(void)ItemName;
	if (QuantityText)
	{
		QuantityText->SetText(FText::FromString(FString::Printf(
			TEXT("%d/%d"),
			FMath::Max(1, Quantity),
			FMath::Max(1, MaximumQuantity))));
	}
	if (InputIcon)
	{
		InputIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			bStoring
				? TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_MouseLeftClick.T_MouseLeftClick")
				: TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_MouseScroll.T_MouseScroll")),
			true);
	}
	if (InstructionText)
	{
		InstructionText->SetText(FText::FromString(
			bStoring
				? TEXT("CTRL + molette : quantité\nClic : poser au sol")
				: TEXT("pour choisir la quantité à prendre")));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
