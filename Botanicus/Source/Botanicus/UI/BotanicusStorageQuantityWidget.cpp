// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusStorageQuantityWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"

namespace
{
	UTextBlock* AddQuantityLine(
		UWidgetTree* WidgetTree,
		UVerticalBox* Container,
		int32 FontSize,
		const FLinearColor& Color)
	{
		UTextBlock* Text =
			WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetJustification(ETextJustify::Center);
		Text->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		if (UVerticalBoxSlot* Slot =
			Container->AddChildToVerticalBox(Text))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Text;
	}
}

void UBotanicusStorageQuantityWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(390.0f);
		SizeBox->SetHeightOverride(138.0f);

		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetPadding(FMargin(14.0f, 9.0f));
		Border->SetBrushColor(
			FLinearColor(0.025f, 0.09f, 0.12f, 0.94f));
		SizeBox->SetContent(Border);

		UVerticalBox* Lines =
			WidgetTree->ConstructWidget<UVerticalBox>();
		Border->SetContent(Lines);

		ActionText = AddQuantityLine(
			WidgetTree,
			Lines,
			18,
			FLinearColor(0.25f, 0.72f, 1.0f, 1.0f));
		ItemText = AddQuantityLine(
			WidgetTree,
			Lines,
			17,
			FLinearColor::White);
		QuantityText = AddQuantityLine(
			WidgetTree,
			Lines,
			22,
			FLinearColor(1.0f, 0.82f, 0.18f, 1.0f));

		UTextBlock* HelpText = AddQuantityLine(
			WidgetTree,
			Lines,
			12,
			FLinearColor(0.76f, 0.82f, 0.84f, 1.0f));
		HelpText->SetText(
			FText::FromString(
				TEXT(
					"MOLETTE : QUANTITE  |  CLIC GAUCHE : VALIDER  |  CLIC DROIT : ANNULER")));

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
	if (ActionText)
	{
		ActionText->SetText(
			FText::FromString(
				bStoring
					? TEXT("RANGER DANS L'ETAGERE")
					: TEXT("PRENDRE L'OBJET")));
	}
	if (ItemText)
	{
		ItemText->SetText(ItemName);
	}
	if (QuantityText)
	{
		QuantityText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("QUANTITE : %d / %d"),
					FMath::Max(1, Quantity),
					FMath::Max(1, MaximumQuantity))));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
