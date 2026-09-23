// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusIllegalOrderWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	UTextBlock* AddOrderLine(
		UWidgetTree* Tree,
		UVerticalBox* Column,
		const TCHAR* Name,
		int32 FontSize,
		const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(Name));
		Text->SetJustification(ETextJustify::Center);
		Text->SetColorAndOpacity(Color);
		Text->SetFont(FSlateFontInfo(
			LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
			FontSize,
			TEXT("Bold")));
		UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Text);
		Slot->SetPadding(FMargin(2.0f, 3.0f));
		return Text;
	}
}

void UBotanicusIllegalOrderWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A newly created Widget Blueprint may contain an empty root panel. Build
	// the native fallback whenever it has no bound order labels yet.
	if (WidgetTree && !HeadingText && !ProductText && !RewardText &&
		!TimerText && !ActionText)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(360.0f);
		Root->SetHeightOverride(190.0f);
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(FLinearColor(0.08f, 0.12f, 0.11f, 0.91f));
		Background->SetPadding(FMargin(12.0f, 8.0f));
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Background->SetContent(Column);
		Root->SetContent(Background);
		HeadingText = AddOrderLine(WidgetTree, Column, TEXT("HeadingText"), 19,
			FLinearColor(0.80f, 0.95f, 0.65f));
		ProductText = AddOrderLine(WidgetTree, Column, TEXT("ProductText"), 18,
			FLinearColor::White);
		RewardText = AddOrderLine(WidgetTree, Column, TEXT("RewardText"), 17,
			FLinearColor(0.98f, 0.85f, 0.47f));
		TimerText = AddOrderLine(WidgetTree, Column, TEXT("TimerText"), 17,
			FLinearColor::White);
		ActionText = AddOrderLine(WidgetTree, Column, TEXT("ActionText"), 18,
			FLinearColor(0.72f, 0.98f, 0.51f));
		WidgetTree->RootWidget = Root;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusIllegalOrderWidget::SetOrderData(
	const FText& InProductName,
	int32 Quantity,
	int32 RewardCredits,
	float RemainingSeconds,
	bool bWaiting,
	bool bLeaving)
{
	if (HeadingText)
	{
		HeadingText->SetText(NSLOCTEXT(
			"BotanicusIllegalTrade", "OrderHeading", "CLIENT CLANDESTIN"));
	}
	if (ProductText)
	{
		ProductText->SetText(FText::Format(
			NSLOCTEXT("BotanicusIllegalTrade", "OrderProduct", "Commande : {0} x{1}"),
			InProductName,
			FText::AsNumber(Quantity)));
	}
	if (RewardText)
	{
		RewardText->SetText(FText::Format(
			NSLOCTEXT("BotanicusIllegalTrade", "OrderReward", "Récompense : {0} crédits"),
			FText::AsNumber(RewardCredits)));
	}
	if (TimerText)
	{
		const int32 Seconds = FMath::Max(0, FMath::CeilToInt(RemainingSeconds));
		TimerText->SetText(FText::FromString(FString::Printf(
			TEXT("Temps restant : %02d:%02d"), Seconds / 60, Seconds % 60)));
	}
	if (ActionText)
	{
		ActionText->SetText(bLeaving
			? NSLOCTEXT("BotanicusIllegalTrade", "OrderLeaving", "Le client repart")
			: bWaiting
				? NSLOCTEXT("BotanicusIllegalTrade", "OrderSellAction", "[E] VENDRE")
				: NSLOCTEXT("BotanicusIllegalTrade", "OrderApproaching", "Le client arrive"));
	}
}
