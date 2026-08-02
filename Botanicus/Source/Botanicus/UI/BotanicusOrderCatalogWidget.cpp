// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusOrderCatalogWidget.h"

#include "BotanicusPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Catalog/BotanicusBuildingCatalogSubsystem.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "UI/BotanicusBuildingCatalogWidget.h"

namespace
{
void SetTextSize(UTextBlock* Text, int32 Size)
{
	if (!Text)
	{
		return;
	}
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
}

FText WeightClassText(EBotanicusItemWeightClass WeightClass)
{
	switch (WeightClass)
	{
	case EBotanicusItemWeightClass::Hotbar:
		return NSLOCTEXT("BotanicusOrders", "HotbarWeight", "Petit colis");
	case EBotanicusItemWeightClass::Handheld:
		return NSLOCTEXT("BotanicusOrders", "HandheldWeight", "Outil en main");
	case EBotanicusItemWeightClass::OnePlayerCarry:
		return NSLOCTEXT("BotanicusOrders", "SoloWeight", "Portage solo");
	case EBotanicusItemWeightClass::TwoPlayerCarry:
		return NSLOCTEXT("BotanicusOrders", "CoopWeight", "Portage à deux");
	default:
		return FText::GetEmpty();
	}
}
}

void UBotanicusMainShopUpgradeRowWidget::InitializeRow(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	RefreshProgress();
}

void UBotanicusMainShopUpgradeRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusMainShopUpgradeRowWidget::BuildLayout()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.035f, 0.16f, 0.34f, 0.99f));
	Root->SetPadding(FMargin(16.0f, 13.0f));
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
	SetTextSize(NameLabel, 20);
	Details->AddChildToVerticalBox(NameLabel);

	CostLabel = WidgetTree->ConstructWidget<UTextBlock>();
	CostLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.96f, 0.82f, 0.28f, 1.0f)));
	CostLabel->SetMargin(FMargin(14.0f));
	SetTextSize(CostLabel, 16);
	UHorizontalBoxSlot* CostSlot =
		Row->AddChildToHorizontalBox(CostLabel);
	CostSlot->SetVerticalAlignment(VAlign_Center);

	UpgradeButton = WidgetTree->ConstructWidget<UButton>();
	UpgradeButton->SetBackgroundColor(
		FLinearColor(0.08f, 0.42f, 0.88f, 1.0f));
	UpgradeButtonLabel = WidgetTree->ConstructWidget<UTextBlock>();
	UpgradeButtonLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	UpgradeButtonLabel->SetMargin(FMargin(14.0f, 9.0f));
	SetTextSize(UpgradeButtonLabel, 13);
	UpgradeButton->AddChild(UpgradeButtonLabel);
	UHorizontalBoxSlot* ButtonSlot =
		Row->AddChildToHorizontalBox(UpgradeButton);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	UpgradeButton->OnClicked.AddDynamic(
		this,
		&UBotanicusMainShopUpgradeRowWidget::HandleUpgradeClicked);
}

void UBotanicusMainShopUpgradeRowWidget::RefreshProgress()
{
	if (!BotanicusController)
	{
		return;
	}

	const int32 Level = BotanicusController->GetMainShopLevel();
	const int32 Capacity =
		BotanicusController->GetMainShopVisitorCapacity();
	const int32 TotalPopulation =
		BotanicusController->GetTargetVisitorPopulation();
	const int32 Cost =
		BotanicusController->GetMainShopUpgradeCost();

	if (NameLabel)
	{
		NameLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusOrders",
					"MainShopName",
					"BOUTIQUE PRINCIPALE  -  NIVEAU {0}\n"
					"{1} DANS LA BOUTIQUE  •  {2} PNJ DANS LE NIVEAU"),
				FText::AsNumber(Level),
				FText::AsNumber(Capacity),
				FText::AsNumber(TotalPopulation)));
	}
	if (CostLabel)
	{
		CostLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusOrders",
					"MainShopCost",
					"{0} credits"),
				FText::AsNumber(Cost)));
	}
	if (UpgradeButtonLabel)
	{
		UpgradeButtonLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusOrders",
					"MainShopUpgradeButton",
					"PASSER AU NIVEAU {0}"),
				FText::AsNumber(Level + 1)));
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(
			BotanicusController->CanUpgradeMainShop());
	}
}

void UBotanicusMainShopUpgradeRowWidget::HandleUpgradeClicked()
{
	if (BotanicusController)
	{
		BotanicusController->UpgradeMainShop();
	}
}

void UBotanicusOrderItemRowWidget::InitializeRow(
	ABotanicusPlayerController* InController,
	const FBotanicusItemDefinition& InDefinition)
{
	BotanicusController = InController;
	ItemKey = InDefinition.ItemKey;
	Price = FMath::Max(0, InDefinition.Price);

	if (NameLabel)
	{
		NameLabel->SetText(
			InDefinition.DisplayName.IsEmpty()
				? FText::FromName(InDefinition.ItemKey)
				: InDefinition.DisplayName);
	}
	if (DetailsLabel)
	{
		DetailsLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusOrders",
					"ItemDetails",
					"{0}  •  x{1}  •  livraison {2} s"),
				WeightClassText(InDefinition.WeightClass),
				FText::AsNumber(FMath::Max(1, InDefinition.DeliveryQuantity)),
				FText::AsNumber(
					FMath::Max(0.1f, InDefinition.DeliveryDelaySeconds))));
	}
	if (PriceLabel)
	{
		PriceLabel->SetText(
			FText::Format(
				NSLOCTEXT("BotanicusOrders", "Price", "{0} crédits"),
				FText::AsNumber(Price)));
	}
	RefreshAvailability(
		BotanicusController
			? BotanicusController->GetAvailableFunds()
			: 0);
}

void UBotanicusOrderItemRowWidget::RefreshAvailability(
	int32 AvailableFunds)
{
	if (OrderButton)
	{
		OrderButton->SetIsEnabled(
			BotanicusController &&
			!ItemKey.IsNone() &&
			AvailableFunds >= Price);
	}
}

void UBotanicusOrderItemRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusOrderItemRowWidget::BuildLayout()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("OrderItemBorder"));
	Root->SetBrushColor(FLinearColor(0.055f, 0.075f, 0.06f, 0.98f));
	Root->SetPadding(FMargin(14.0f, 11.0f));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* Row =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChild(Row);

	UVerticalBox* Description =
		WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* DescriptionSlot =
		Row->AddChildToHorizontalBox(Description);
	DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DescriptionSlot->SetVerticalAlignment(VAlign_Center);

	NameLabel = WidgetTree->ConstructWidget<UTextBlock>();
	NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetTextSize(NameLabel, 18);
	Description->AddChildToVerticalBox(NameLabel);

	DetailsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	DetailsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.65f, 0.76f, 0.66f, 1.0f)));
	SetTextSize(DetailsLabel, 12);
	Description->AddChildToVerticalBox(DetailsLabel);

	PriceLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PriceLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.96f, 0.78f, 0.25f, 1.0f)));
	PriceLabel->SetJustification(ETextJustify::Right);
	PriceLabel->SetMargin(FMargin(12.0f));
	SetTextSize(PriceLabel, 16);
	UHorizontalBoxSlot* PriceSlot =
		Row->AddChildToHorizontalBox(PriceLabel);
	PriceSlot->SetVerticalAlignment(VAlign_Center);

	OrderButton = WidgetTree->ConstructWidget<UButton>();
	OrderButton->SetBackgroundColor(
		FLinearColor(0.12f, 0.42f, 0.19f, 1.0f));
	UTextBlock* OrderText = WidgetTree->ConstructWidget<UTextBlock>();
	OrderText->SetText(
		NSLOCTEXT("BotanicusOrders", "OrderButton", "COMMANDER"));
	OrderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	OrderText->SetMargin(FMargin(14.0f, 8.0f));
	SetTextSize(OrderText, 13);
	OrderButton->AddChild(OrderText);
	UHorizontalBoxSlot* ButtonSlot =
		Row->AddChildToHorizontalBox(OrderButton);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	OrderButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderItemRowWidget::HandleOrderClicked);
}

void UBotanicusOrderItemRowWidget::HandleOrderClicked()
{
	if (BotanicusController && !ItemKey.IsNone())
	{
		BotanicusController->PlaceCatalogOrder(ItemKey);
	}
}

void UBotanicusOrderCatalogWidget::InitializeWithController(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	RebuildItemRows();
	Refresh();
}

void UBotanicusOrderCatalogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusOrderCatalogWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.2f)
	{
		RefreshAccumulator = 0.0f;
		Refresh();
	}
}

void UBotanicusOrderCatalogWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("OrderCatalogRoot"));
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
	PanelSlot->SetAnchors(FAnchors(0.025f, 0.035f, 0.975f, 0.965f));
	PanelSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* Column =
		WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	UHorizontalBox* Header =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(
		NSLOCTEXT("BotanicusOrders", "CatalogTitle", "PANNEAU DE COMMANDE"));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetTextSize(Title, 27);
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* FundsColumn =
		WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* FundsColumnSlot =
		Header->AddChildToHorizontalBox(FundsColumn);
	FundsColumnSlot->SetVerticalAlignment(VAlign_Center);

	FundsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FundsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.96f, 0.78f, 0.25f, 1.0f)));
	FundsLabel->SetMargin(FMargin(10.0f, 0.0f, 10.0f, 2.0f));
	FundsLabel->SetJustification(ETextJustify::Center);
	SetTextSize(FundsLabel, 20);
	FundsColumn->AddChildToVerticalBox(FundsLabel);

	UButton* AddTestCreditsButton =
		WidgetTree->ConstructWidget<UButton>();
	AddTestCreditsButton->SetBackgroundColor(
		FLinearColor(0.82f, 0.36f, 0.06f, 1.0f));
	UTextBlock* AddTestCreditsText =
		WidgetTree->ConstructWidget<UTextBlock>();
	AddTestCreditsText->SetText(
		NSLOCTEXT(
			"BotanicusOrders",
			"AddTestCredits",
			"TEST +100 CREDITS"));
	AddTestCreditsText->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	AddTestCreditsText->SetMargin(FMargin(9.0f, 4.0f));
	AddTestCreditsText->SetJustification(ETextJustify::Center);
	SetTextSize(AddTestCreditsText, 11);
	AddTestCreditsButton->AddChild(AddTestCreditsText);
	FundsColumn->AddChildToVerticalBox(AddTestCreditsButton);
	AddTestCreditsButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleAddTestCreditsClicked);

	ShopOpenButton = WidgetTree->ConstructWidget<UButton>();
	ShopOpenButtonLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ShopOpenButtonLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor::White));
	ShopOpenButtonLabel->SetMargin(FMargin(13.0f, 8.0f));
	ShopOpenButtonLabel->SetJustification(ETextJustify::Center);
	SetTextSize(ShopOpenButtonLabel, 13);
	ShopOpenButton->AddChild(ShopOpenButtonLabel);
	UHorizontalBoxSlot* ShopOpenSlot =
		Header->AddChildToHorizontalBox(ShopOpenButton);
	ShopOpenSlot->SetVerticalAlignment(VAlign_Center);
	ShopOpenSlot->SetPadding(FMargin(10.0f, 0.0f));
	ShopOpenButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleShopOpenClicked);

	LevelLabel = WidgetTree->ConstructWidget<UTextBlock>();
	LevelLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.4f, 0.78f, 0.96f, 1.0f)));
	LevelLabel->SetMargin(FMargin(10.0f));
	SetTextSize(LevelLabel, 16);
	Header->AddChildToHorizontalBox(LevelLabel);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>();
	CloseButton->SetBackgroundColor(
		FLinearColor(0.36f, 0.08f, 0.06f, 1.0f));
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>();
	CloseText->SetText(FText::FromString(TEXT("FERMER")));
	CloseText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CloseText->SetMargin(FMargin(12.0f, 7.0f));
	CloseButton->AddChild(CloseText);
	Header->AddChildToHorizontalBox(CloseButton);
	CloseButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleCloseClicked);

	UHorizontalBox* Tabs =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* TabsSlot =
		Column->AddChildToVerticalBox(Tabs);
	TabsSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 4.0f));

	auto AddTabButton =
		[this, Tabs](const FText& Label) -> UButton*
		{
			UButton* Button =
				WidgetTree->ConstructWidget<UButton>();
			UTextBlock* Text =
				WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetText(Label);
			Text->SetColorAndOpacity(
				FSlateColor(FLinearColor::White));
			Text->SetMargin(FMargin(13.0f, 9.0f));
			Text->SetJustification(ETextJustify::Center);
			SetTextSize(Text, 13);
			Button->AddChild(Text);
			UHorizontalBoxSlot* Slot =
				Tabs->AddChildToHorizontalBox(Button);
			Slot->SetSize(
				FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(2.0f));
			TabButtons.Add(Button);
			return Button;
		};

	UButton* SeedsButton = AddTabButton(
		NSLOCTEXT("BotanicusOrders", "SeedsTab", "GRAINES"));
	SeedsButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleSeedsTabClicked);
	UButton* ToolsButton = AddTabButton(
		NSLOCTEXT(
			"BotanicusOrders",
			"ToolsTab",
			"OUTILS DE JARDINAGE"));
	ToolsButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleToolsTabClicked);
	UButton* PreparationButton = AddTabButton(
		NSLOCTEXT(
			"BotanicusOrders",
			"PreparationTab",
			"PREPARATION"));
	PreparationButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandlePreparationTabClicked);
	UButton* SalesButton = AddTabButton(
		NSLOCTEXT("BotanicusOrders", "SalesTab", "VENTE"));
	SalesButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleSalesTabClicked);
	UButton* BuildingsButton = AddTabButton(
		NSLOCTEXT(
			"BotanicusOrders",
			"BuildingsTab",
			"BATIMENTS"));
	BuildingsButton->OnClicked.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleBuildingsTabClicked);

	PendingOrdersLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PendingOrdersLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.66f, 0.82f, 0.68f, 1.0f)));
	PendingOrdersLabel->SetMargin(FMargin(0.0f, 12.0f, 0.0f, 10.0f));
	PendingOrdersLabel->SetAutoWrapText(true);
	SetTextSize(PendingOrdersLabel, 14);
	UScrollBox* PendingOrdersScroll =
		WidgetTree->ConstructWidget<UScrollBox>();
	PendingOrdersScroll->SetOrientation(Orient_Vertical);
	PendingOrdersScroll->AddChild(PendingOrdersLabel);
	USizeBox* PendingOrdersArea =
		WidgetTree->ConstructWidget<USizeBox>();
	PendingOrdersArea->SetHeightOverride(105.0f);
	PendingOrdersArea->AddChild(PendingOrdersScroll);
	Column->AddChildToVerticalBox(PendingOrdersArea);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ItemsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(ItemsBox);
	RefreshTabButtons();
}

void UBotanicusOrderCatalogWidget::RebuildItemRows()
{
	if (!ItemsBox || !BotanicusController)
	{
		return;
	}

	ItemsBox->ClearChildren();
	ItemRows.Reset();
	BuildingRows.Reset();
	MainShopUpgradeRow = nullptr;
	const UGameInstance* GameInstance =
		BotanicusController->GetGameInstance();
	if (ActiveTab == EBotanicusCommandPanelTab::Buildings)
	{
		MainShopUpgradeRow =
			CreateWidget<UBotanicusMainShopUpgradeRowWidget>(
				GetOwningPlayer(),
				UBotanicusMainShopUpgradeRowWidget::StaticClass());
		if (MainShopUpgradeRow)
		{
			MainShopUpgradeRow->InitializeRow(BotanicusController);
			UVerticalBoxSlot* ShopRowSlot =
				ItemsBox->AddChildToVerticalBox(MainShopUpgradeRow);
			ShopRowSlot->SetPadding(
				FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
		const UBotanicusBuildingCatalogSubsystem* BuildingCatalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusBuildingCatalogSubsystem>()
				: nullptr;
		if (!BuildingCatalog)
		{
			return;
		}
		TArray<FBotanicusBuildingDefinition> Buildings =
			BuildingCatalog->GetAllBuildings();
		Buildings.Sort(
			[](const FBotanicusBuildingDefinition& A,
			   const FBotanicusBuildingDefinition& B)
			{
				return A.Price < B.Price;
			});
		for (const FBotanicusBuildingDefinition& Definition :
			 Buildings)
		{
			if (Definition.BuildingKey.IsNone())
			{
				continue;
			}
			UBotanicusBuildingCatalogRowWidget* Row =
				CreateWidget<
					UBotanicusBuildingCatalogRowWidget>(
					GetOwningPlayer(),
					UBotanicusBuildingCatalogRowWidget::
						StaticClass());
			if (!Row)
			{
				continue;
			}
			Row->InitializeRow(
				BotanicusController,
				Definition);
			UVerticalBoxSlot* RowSlot =
				ItemsBox->AddChildToVerticalBox(Row);
			RowSlot->SetPadding(
				FMargin(0.0f, 0.0f, 0.0f, 7.0f));
			BuildingRows.Add(Row);
		}
		return;
	}

	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusItemCatalogSubsystem>()
			: nullptr;
	if (!Catalog)
	{
		return;
	}

	TArray<FBotanicusItemDefinition> Definitions =
		Catalog->GetAllItems();
	Definitions.Sort(
		[](const FBotanicusItemDefinition& A,
		   const FBotanicusItemDefinition& B)
		{
			if (A.Category != B.Category)
			{
				return static_cast<uint8>(A.Category) <
					static_cast<uint8>(B.Category);
			}
			return A.DisplayName.ToString() < B.DisplayName.ToString();
		});

	for (const FBotanicusItemDefinition& Definition : Definitions)
	{
		const EBotanicusCatalogTab CatalogTab =
			ActiveTab == EBotanicusCommandPanelTab::Seeds
				? EBotanicusCatalogTab::Seeds
				: ActiveTab ==
						EBotanicusCommandPanelTab::GardeningTools
					? EBotanicusCatalogTab::GardeningTools
					: ActiveTab ==
							EBotanicusCommandPanelTab::Preparation
						? EBotanicusCatalogTab::Preparation
						: EBotanicusCatalogTab::Sales;
		if (Definition.ItemKey.IsNone() ||
			!Definition.bPurchasable ||
			(Definition.CatalogTabs &
			 static_cast<int32>(CatalogTab)) == 0)
		{
			continue;
		}
		UBotanicusOrderItemRowWidget* Row =
			CreateWidget<UBotanicusOrderItemRowWidget>(
				GetOwningPlayer(),
				UBotanicusOrderItemRowWidget::StaticClass());
		if (!Row)
		{
			continue;
		}
		Row->InitializeRow(BotanicusController, Definition);
		UVerticalBoxSlot* RowSlot = ItemsBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		ItemRows.Add(Row);
	}
}

void UBotanicusOrderCatalogWidget::Refresh()
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
				NSLOCTEXT("BotanicusOrders", "Funds", "{0} crédits"),
				FText::AsNumber(Funds)));
	}
	if (ShopOpenButton && ShopOpenButtonLabel)
	{
		const bool bShopOpen =
			BotanicusController->IsMainShopOpen();
		ShopOpenButton->SetBackgroundColor(
			bShopOpen
				? FLinearColor(0.08f, 0.55f, 0.20f, 1.0f)
				: FLinearColor(0.68f, 0.12f, 0.08f, 1.0f));
		ShopOpenButtonLabel->SetText(
			bShopOpen
				? NSLOCTEXT(
					"BotanicusOrders",
					"ShopCurrentlyOpen",
					"MAGASIN OUVERT")
				: NSLOCTEXT(
					"BotanicusOrders",
					"ShopCurrentlyClosed",
					"MAGASIN FERME"));
	}
	for (UBotanicusOrderItemRowWidget* Row : ItemRows)
	{
		if (Row)
		{
			Row->RefreshAvailability(Funds);
		}
	}
	for (UBotanicusBuildingCatalogRowWidget* Row : BuildingRows)
	{
		if (Row)
		{
			Row->RefreshAvailability(
				Funds,
				DevelopmentLevel);
		}
	}
	if (MainShopUpgradeRow)
	{
		MainShopUpgradeRow->RefreshProgress();
	}
	if (LevelLabel)
	{
		LevelLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusOrders",
					"UnifiedDevelopmentLevel",
					"NIVEAU {0}"),
				FText::AsNumber(DevelopmentLevel)));
		LevelLabel->SetVisibility(
			ActiveTab ==
					EBotanicusCommandPanelTab::Buildings
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (!PendingOrdersLabel)
	{
		return;
	}
	const TArray<FBotanicusPendingOrder>& Orders =
		BotanicusController->GetPendingOrders();
	if (Orders.Num() == 0)
	{
		PendingOrdersLabel->SetText(
			NSLOCTEXT(
				"BotanicusOrders",
				"NoPendingOrders",
				"Aucune livraison en attente."));
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState =
		World ? World->GetGameState() : nullptr;
	const float ServerTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);
	FString Summary = TEXT("EN LIVRAISON : ");
	for (int32 Index = 0; Index < Orders.Num(); ++Index)
	{
		const FBotanicusPendingOrder& Order = Orders[Index];
		const float Remaining =
			FMath::Max(0.0f, Order.DeliveryServerTime - ServerTime);
		if (Index > 0)
		{
			Summary += TEXT("   |   ");
		}
		Summary += FString::Printf(
			TEXT("%s x%d — %.1f s"),
			*Order.DisplayName.ToString(),
			Order.Quantity,
			Remaining);
	}
	PendingOrdersLabel->SetText(FText::FromString(Summary));
}

void UBotanicusOrderCatalogWidget::ShowBuildingTab()
{
	SelectTab(EBotanicusCommandPanelTab::Buildings);
}

void UBotanicusOrderCatalogWidget::SelectTab(
	EBotanicusCommandPanelTab NewTab)
{
	ActiveTab = NewTab;
	RefreshTabButtons();
	RebuildItemRows();
	Refresh();
}

void UBotanicusOrderCatalogWidget::RefreshTabButtons()
{
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		if (UButton* Button = TabButtons[Index])
		{
			Button->SetBackgroundColor(
				Index == static_cast<int32>(ActiveTab)
					? FLinearColor(0.12f, 0.46f, 0.21f, 1.0f)
					: FLinearColor(0.07f, 0.12f, 0.08f, 1.0f));
		}
	}
}

void UBotanicusOrderCatalogWidget::HandleSeedsTabClicked()
{
	SelectTab(EBotanicusCommandPanelTab::Seeds);
}

void UBotanicusOrderCatalogWidget::HandleToolsTabClicked()
{
	SelectTab(EBotanicusCommandPanelTab::GardeningTools);
}

void UBotanicusOrderCatalogWidget::
	HandlePreparationTabClicked()
{
	SelectTab(EBotanicusCommandPanelTab::Preparation);
}

void UBotanicusOrderCatalogWidget::HandleSalesTabClicked()
{
	SelectTab(EBotanicusCommandPanelTab::Sales);
}

void UBotanicusOrderCatalogWidget::
	HandleBuildingsTabClicked()
{
	SelectTab(EBotanicusCommandPanelTab::Buildings);
}

void UBotanicusOrderCatalogWidget::HandleAddTestCreditsClicked()
{
	if (BotanicusController)
	{
		BotanicusController->AddTestCredits();
	}
}

void UBotanicusOrderCatalogWidget::HandleShopOpenClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleMainShopOpen();
	}
}

void UBotanicusOrderCatalogWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleOrderCatalog();
	}
}
