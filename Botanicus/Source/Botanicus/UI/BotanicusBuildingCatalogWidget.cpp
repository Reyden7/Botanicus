// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusBuildingCatalogWidget.h"

#include "BotanicusPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Catalog/BotanicusBuildingCatalogSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

namespace
{
void SetBuildingCatalogTextSize(UTextBlock* Text, int32 Size)
{
	if (!Text)
	{
		return;
	}
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
}
}

void UBotanicusBuildingCatalogRowWidget::InitializeRow(
	ABotanicusPlayerController* InController,
	const FBotanicusBuildingDefinition& InDefinition)
{
	BotanicusController = InController;
	BuildingKey = InDefinition.BuildingKey;
	Price = FMath::Max(0, InDefinition.Price);
	RequiredDevelopmentLevel =
		FMath::Max(1, InDefinition.RequiredDevelopmentLevel);
	bUnlocked = InDefinition.bUnlockedByDefault;

	if (NameLabel)
	{
		NameLabel->SetText(
			InDefinition.DisplayName.IsEmpty()
				? FText::FromName(BuildingKey)
				: InDefinition.DisplayName);
	}
	if (DescriptionLabel)
	{
		DescriptionLabel->SetText(InDefinition.Description);
	}
	if (PriceLabel)
	{
		PriceLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusBuildings",
					"BuildingPrice",
					"{0} crédits"),
				FText::AsNumber(Price)));
	}
	RefreshAvailability(
		BotanicusController
			? BotanicusController->GetAvailableFunds()
			: 0,
		BotanicusController
			? BotanicusController->GetBuildingProgressionLevel()
			: 1);
}

void UBotanicusBuildingCatalogRowWidget::RefreshAvailability(
	int32 AvailableFunds,
	int32 DevelopmentLevel)
{
	const bool bLevelUnlocked =
		DevelopmentLevel >= RequiredDevelopmentLevel;
	if (PurchaseButton)
	{
		PurchaseButton->SetIsEnabled(
			BotanicusController &&
			bUnlocked &&
			bLevelUnlocked &&
			!BuildingKey.IsNone() &&
			AvailableFunds >= Price);
	}
	if (DescriptionLabel)
	{
		const FText BaseDescription = DescriptionLabel->GetText();
		// InitializeRow supplies the base text once. Avoid recursively
		// appending the lock line during refreshes.
		FString Description = BaseDescription.ToString();
		const int32 LockMarker = Description.Find(TEXT("\nNIVEAU "));
		if (LockMarker != INDEX_NONE)
		{
			Description.LeftInline(LockMarker);
		}
		if (!bLevelUnlocked)
		{
			Description += FString::Printf(
				TEXT("\nNIVEAU %d REQUIS — VERROUILLÉ"),
				RequiredDevelopmentLevel);
		}
		else
		{
			Description += FString::Printf(
				TEXT("\nNIVEAU %d — DÉVERROUILLÉ"),
				RequiredDevelopmentLevel);
		}
		DescriptionLabel->SetText(FText::FromString(Description));
		DescriptionLabel->SetColorAndOpacity(
			FSlateColor(
				bLevelUnlocked
					? FLinearColor(0.65f, 0.76f, 0.66f, 1.0f)
					: FLinearColor(0.86f, 0.38f, 0.22f, 1.0f)));
	}
}

void UBotanicusBuildingCatalogRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusBuildingCatalogRowWidget::BuildLayout()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.055f, 0.075f, 0.06f, 0.98f));
	Root->SetPadding(FMargin(14.0f, 12.0f));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChild(Row);

	UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* DetailsSlot =
		Row->AddChildToHorizontalBox(Details);
	DetailsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DetailsSlot->SetVerticalAlignment(VAlign_Center);

	NameLabel = WidgetTree->ConstructWidget<UTextBlock>();
	NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetBuildingCatalogTextSize(NameLabel, 20);
	Details->AddChildToVerticalBox(NameLabel);

	DescriptionLabel = WidgetTree->ConstructWidget<UTextBlock>();
	DescriptionLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.65f, 0.76f, 0.66f, 1.0f)));
	DescriptionLabel->SetAutoWrapText(true);
	SetBuildingCatalogTextSize(DescriptionLabel, 13);
	Details->AddChildToVerticalBox(DescriptionLabel);

	PriceLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PriceLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.96f, 0.78f, 0.25f, 1.0f)));
	PriceLabel->SetMargin(FMargin(14.0f));
	SetBuildingCatalogTextSize(PriceLabel, 17);
	UHorizontalBoxSlot* PriceSlot =
		Row->AddChildToHorizontalBox(PriceLabel);
	PriceSlot->SetVerticalAlignment(VAlign_Center);

	PurchaseButton = WidgetTree->ConstructWidget<UButton>();
	PurchaseButton->SetBackgroundColor(
		FLinearColor(0.12f, 0.42f, 0.19f, 1.0f));
	UTextBlock* PurchaseText = WidgetTree->ConstructWidget<UTextBlock>();
	PurchaseText->SetText(
		NSLOCTEXT("BotanicusBuildings", "BuyButton", "ACHETER"));
	PurchaseText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PurchaseText->SetMargin(FMargin(14.0f, 8.0f));
	SetBuildingCatalogTextSize(PurchaseText, 13);
	PurchaseButton->AddChild(PurchaseText);
	UHorizontalBoxSlot* ButtonSlot =
		Row->AddChildToHorizontalBox(PurchaseButton);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	PurchaseButton->OnClicked.AddDynamic(
		this,
		&UBotanicusBuildingCatalogRowWidget::HandlePurchaseClicked);
}

void UBotanicusBuildingCatalogRowWidget::HandlePurchaseClicked()
{
	if (BotanicusController && !BuildingKey.IsNone())
	{
		BotanicusController->PurchaseCatalogBuilding(BuildingKey);
	}
}

void UBotanicusBuildingCatalogWidget::InitializeWithController(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	RebuildRows();
	Refresh();
}

void UBotanicusBuildingCatalogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusBuildingCatalogWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));
	UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.018f, 0.028f, 0.02f, 0.99f));
	Panel->SetPadding(FMargin(22.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(820.0f, 610.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(
		NSLOCTEXT(
			"BotanicusBuildings",
			"BuildingCatalogTitle",
			"CATALOGUE DE BÂTIMENTS"));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetBuildingCatalogTextSize(Title, 27);
	UHorizontalBoxSlot* TitleSlot =
		Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	FundsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FundsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.96f, 0.78f, 0.25f, 1.0f)));
	FundsLabel->SetMargin(FMargin(10.0f));
	SetBuildingCatalogTextSize(FundsLabel, 20);
	Header->AddChildToHorizontalBox(FundsLabel);

	LevelLabel = WidgetTree->ConstructWidget<UTextBlock>();
	LevelLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.4f, 0.78f, 0.96f, 1.0f)));
	LevelLabel->SetMargin(FMargin(10.0f));
	SetBuildingCatalogTextSize(LevelLabel, 17);
	Header->AddChildToHorizontalBox(LevelLabel);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>();
	CloseButton->SetBackgroundColor(
		FLinearColor(0.36f, 0.08f, 0.06f, 1.0f));
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>();
	CloseText->SetText(
		NSLOCTEXT("BotanicusBuildings", "Close", "FERMER"));
	CloseText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CloseText->SetMargin(FMargin(12.0f, 7.0f));
	CloseButton->AddChild(CloseText);
	Header->AddChildToHorizontalBox(CloseButton);
	CloseButton->OnClicked.AddDynamic(
		this,
		&UBotanicusBuildingCatalogWidget::HandleCloseClicked);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
	Hint->SetText(
		NSLOCTEXT(
			"BotanicusBuildings",
			"BuildingCatalogHint",
			"Le prix est débité à l'achat et remboursé si le placement est annulé."));
	Hint->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.66f, 0.82f, 0.68f, 1.0f)));
	Hint->SetMargin(FMargin(0.0f, 12.0f, 0.0f, 10.0f));
	SetBuildingCatalogTextSize(Hint, 14);
	Column->AddChildToVerticalBox(Hint);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* ScrollSlot =
		Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BuildingsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(BuildingsBox);
}

void UBotanicusBuildingCatalogWidget::RebuildRows()
{
	if (!BuildingsBox || !BotanicusController)
	{
		return;
	}

	BuildingsBox->ClearChildren();
	Rows.Reset();
	const UGameInstance* GameInstance =
		BotanicusController->GetGameInstance();
	const UBotanicusBuildingCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusBuildingCatalogSubsystem>()
			: nullptr;
	if (!Catalog)
	{
		return;
	}

	TArray<FBotanicusBuildingDefinition> Definitions =
		Catalog->GetAllBuildings();
	Definitions.Sort(
		[](const FBotanicusBuildingDefinition& A,
		   const FBotanicusBuildingDefinition& B)
		{
			return A.Price < B.Price;
		});

	for (const FBotanicusBuildingDefinition& Definition : Definitions)
	{
		if (Definition.BuildingKey.IsNone())
		{
			continue;
		}
		UBotanicusBuildingCatalogRowWidget* Row =
			CreateWidget<UBotanicusBuildingCatalogRowWidget>(
				GetOwningPlayer(),
				UBotanicusBuildingCatalogRowWidget::StaticClass());
		if (!Row)
		{
			continue;
		}
		Row->InitializeRow(BotanicusController, Definition);
		UVerticalBoxSlot* RowSlot =
			BuildingsBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		Rows.Add(Row);
	}
}

void UBotanicusBuildingCatalogWidget::Refresh()
{
	if (!BotanicusController)
	{
		return;
	}

	const int32 Funds = BotanicusController->GetAvailableFunds();
	const int32 DevelopmentLevel =
		BotanicusController->GetBuildingProgressionLevel();
	if (FundsLabel)
	{
		FundsLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusBuildings",
					"Funds",
					"{0} crédits"),
				FText::AsNumber(Funds)));
	}
	if (LevelLabel)
	{
		LevelLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusBuildings",
					"DevelopmentLevel",
					"NIVEAU {0}"),
				FText::AsNumber(DevelopmentLevel)));
	}
	for (UBotanicusBuildingCatalogRowWidget* Row : Rows)
	{
		if (Row)
		{
			Row->RefreshAvailability(Funds, DevelopmentLevel);
		}
	}
}

void UBotanicusBuildingCatalogWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleBuildingCatalog();
	}
}
