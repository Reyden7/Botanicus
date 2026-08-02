// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusSharedFundsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusGameState.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"

namespace
{
FString TrendColorLabel(FName Tag)
{
	if (Tag == TEXT("Pink"))
	{
		return TEXT("ROSE");
	}
	if (Tag == TEXT("Purple"))
	{
		return TEXT("VIOLETTE");
	}
	return TEXT("VERTE");
}

FString TrendTypeLabel(FName Tag)
{
	if (Tag == TEXT("Flowering"))
	{
		return TEXT("PLANTE FLEURIE");
	}
	if (Tag == TEXT("Foliage"))
	{
		return TEXT("FEUILLAGE DECORATIF");
	}
	return TEXT("PLANTE AROMATIQUE");
}

FString TrendQualityLabel(FName Tag)
{
	if (Tag == TEXT("Exceptional"))
	{
		return TEXT("QUALITE EXCEPTIONNELLE");
	}
	if (Tag == TEXT("Beautiful"))
	{
		return TEXT("BELLE QUALITE");
	}
	return TEXT("QUALITE STANDARD");
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

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.025f, 0.075f, 0.045f, 0.92f));
	Background->SetPadding(FMargin(18.0f, 10.0f));
	UCanvasPanelSlot* BackgroundSlot =
		Root->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	BackgroundSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	BackgroundSlot->SetPosition(FVector2D(-24.0f, 24.0f));
	BackgroundSlot->SetSize(FVector2D(540.0f, 176.0f));

	UVerticalBox* Content =
		WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Content);
	FundsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FundsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.95f, 0.84f, 0.28f, 1.0f)));
	FundsLabel->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 22));
	FundsLabel->SetJustification(ETextJustify::Center);
	Content->AddChildToVerticalBox(FundsLabel);

	ShopLevelLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ShopLevelLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.40f, 0.78f, 1.0f, 1.0f)));
	ShopLevelLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 17));
	ShopLevelLabel->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ShopLevelSlot =
			Content->AddChildToVerticalBox(ShopLevelLabel))
	{
		ShopLevelSlot->SetPadding(
			FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}

	ReputationLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ReputationLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.66f, 0.22f, 1.0f)));
	ReputationLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 16));
	ReputationLabel->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ReputationSlot =
			Content->AddChildToVerticalBox(ReputationLabel))
	{
		ReputationSlot->SetPadding(
			FMargin(0.0f, 5.0f, 0.0f, 0.0f));
	}

	TrendsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	TrendsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.45f, 0.95f, 0.72f, 1.0f)));
	TrendsLabel->SetFont(
		FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
	TrendsLabel->SetJustification(ETextJustify::Center);
	TrendsLabel->SetAutoWrapText(true);
	if (UVerticalBoxSlot* TrendsSlot =
			Content->AddChildToVerticalBox(TrendsLabel))
	{
		TrendsSlot->SetPadding(
			FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
}

void UBotanicusSharedFundsWidget::RefreshFunds()
{
	const UWorld* World = GetWorld();
	const ABotanicusGameState* GameState =
		World ? World->GetGameState<ABotanicusGameState>() : nullptr;
	if (!FundsLabel || !ShopLevelLabel ||
		!ReputationLabel || !TrendsLabel || !GameState)
	{
		return;
	}

	const int32 Funds = GameState->GetSharedFunds();
	const int32 ShopLevel = GameState->GetMainShopLevel();
	const int32 Reputation = GameState->GetShopReputationPoints();
	const int32 Satisfaction =
		GameState->GetLastVisitorSatisfaction();
	const int32 Reviews = GameState->GetTotalVisitorReviews();
	const int32 TrendSeconds =
		FMath::CeilToInt(GameState->GetTrendRemainingSeconds());
	const FName TrendColor = GameState->GetTrendColorTag();
	const FName TrendType = GameState->GetTrendTypeTag();
	const FName TrendQuality = GameState->GetTrendQualityTag();
	if (Funds == LastDisplayedFunds &&
		ShopLevel == LastDisplayedShopLevel &&
		Reputation == LastDisplayedReputation &&
		Satisfaction == LastDisplayedSatisfaction &&
		Reviews == LastDisplayedReviews &&
		TrendSeconds == LastDisplayedTrendSeconds &&
		TrendColor == LastDisplayedTrendColor &&
		TrendType == LastDisplayedTrendType &&
		TrendQuality == LastDisplayedTrendQuality)
	{
		return;
	}
	LastDisplayedFunds = Funds;
	LastDisplayedShopLevel = ShopLevel;
	LastDisplayedReputation = Reputation;
	LastDisplayedSatisfaction = Satisfaction;
	LastDisplayedReviews = Reviews;
	LastDisplayedTrendSeconds = TrendSeconds;
	LastDisplayedTrendColor = TrendColor;
	LastDisplayedTrendType = TrendType;
	LastDisplayedTrendQuality = TrendQuality;
	FundsLabel->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("CAISSE COMMUNE  •  %d CRÉDITS"),
				Funds)));
	ShopLevelLabel->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("BOUTIQUE  -  NIVEAU %d"),
				ShopLevel)));

	const int32 FilledStars =
		FMath::Clamp(
			FMath::RoundToInt(Reputation / 100.0f),
			1,
			5);
	FString Stars;
	for (int32 StarIndex = 1; StarIndex <= 5; ++StarIndex)
	{
		Stars.AppendChar(
			StarIndex <= FilledStars ? 0x2605 : 0x2606);
	}
	const FString LastReview =
		Reviews > 0
			? FString::Printf(
				TEXT("  -  DERNIER VISITEUR %d%%"),
				Satisfaction)
			: TEXT("  -  AUCUN AVIS");
	ReputationLabel->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("REPUTATION  %s  %.2f/5%s"),
				*Stars,
				Reputation / 100.0f,
				*LastReview)));

	TrendsLabel->SetText(
		FText::FromString(
			FString::Printf(
				TEXT(
					"TENDANCES %02d:%02d  -  %s  -  %s  -  %s  (+10%% chacune)"),
				TrendSeconds / 60,
				TrendSeconds % 60,
				*TrendColorLabel(TrendColor),
				*TrendTypeLabel(TrendType),
				*TrendQualityLabel(TrendQuality))));
}
