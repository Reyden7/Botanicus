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
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusBuildingCatalogWidget.h"
#include "UI/BotanicusCommandPanelSettings.h"

namespace
{
const FLinearColor CommandGold(0.97f, 0.76f, 0.27f, 1.0f);
const FLinearColor CommandCream(1.0f, 0.88f, 0.60f, 1.0f);
const FLinearColor CommandGreen(0.055f, 0.18f, 0.105f, 0.98f);
const FLinearColor CommandPanelGreen(0.075f, 0.24f, 0.14f, 0.96f);
const FLinearColor CommandRowGreen(0.16f, 0.27f, 0.16f, 0.97f);

void MakeButtonChromeInvisible(UButton* Button)
{
	if (!Button)
	{
		return;
	}
	FButtonStyle Style = Button->GetStyle();
	FSlateBrush Invisible;
	Invisible.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.SetNormal(Invisible);
	Style.SetHovered(Invisible);
	Style.SetPressed(Invisible);
	Style.SetDisabled(Invisible);
	Style.NormalPadding = FMargin(0.0f);
	Style.PressedPadding = FMargin(0.0f);
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
}

FSlateBrush MakeTextureBrush(
	const TCHAR* AssetName,
	const FVector2D& Size,
	const FLinearColor& Tint = FLinearColor::White)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = Size;
	Brush.TintColor = FSlateColor(Tint);
	Brush.SetResourceObject(LoadObject<UTexture2D>(
		nullptr,
		*FString::Printf(
			TEXT("/Game/Botanicus/UI/Command/Textures/%s.%s"),
			AssetName,
			AssetName)));
	return Brush;
}

void ApplyTextureButtonStyle(
	UButton* Button,
	const TCHAR* NormalAsset,
	const TCHAR* PressedAsset,
	const FVector2D& Size)
{
	if (!Button)
	{
		return;
	}
	FButtonStyle Style = Button->GetStyle();
	const FSlateBrush Normal = MakeTextureBrush(NormalAsset, Size);
	const FSlateBrush Pressed = MakeTextureBrush(PressedAsset, Size);
	Style.SetNormal(Normal);
	Style.SetHovered(Normal);
	Style.SetPressed(Pressed);
	Style.SetDisabled(MakeTextureBrush(
		NormalAsset,
		Size,
		FLinearColor(0.45f, 0.45f, 0.45f, 0.65f)));
	Style.NormalPadding = FMargin(0.0f);
	Style.PressedPadding = FMargin(1.0f, 2.0f, 0.0f, 0.0f);
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
}

void SetTextSize(UTextBlock* Text, int32 Size)
{
	if (!Text)
	{
		return;
	}
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
}

UTexture2D* LoadCommandTexture(const TCHAR* AssetName)
{
	return LoadObject<UTexture2D>(
		nullptr,
		*FString::Printf(
			TEXT("/Game/Botanicus/UI/Command/Textures/%s.%s"),
			AssetName,
			AssetName));
}

UImage* MakeCommandImage(
	UWidgetTree* Tree,
	const TCHAR* AssetName,
	const FVector2D& Size)
{
	UImage* Image = Tree->ConstructWidget<UImage>();
	if (UTexture2D* Texture = LoadCommandTexture(AssetName))
	{
		Image->SetBrushFromTexture(Texture, true);
	}
	Image->SetDesiredSizeOverride(Size);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Image;
}

USizeBox* WrapAtSize(
	UWidgetTree* Tree,
	UWidget* Child,
	float Width,
	float Height)
{
	USizeBox* Box = Tree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Width);
	Box->SetHeightOverride(Height);
	Box->AddChild(Child);
	return Box;
}

void ApplyLayoutOffset(UWidget* Widget, const FVector2D& Offset)
{
	if (Widget)
	{
		Widget->SetRenderTranslation(Offset);
	}
}

void AddIconAndText(
	UWidgetTree* Tree,
	UButton* Button,
	const TCHAR* IconAsset,
	const FText& Label,
	float IconSize,
	int32 TextSize,
	const FLinearColor& TextColor = CommandCream)
{
	UHorizontalBox* Content = Tree->ConstructWidget<UHorizontalBox>();
	Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UImage* Icon = MakeCommandImage(
		Tree, IconAsset, FVector2D(IconSize, IconSize));
	UHorizontalBoxSlot* IconSlot =
		Content->AddChildToHorizontalBox(
			WrapAtSize(Tree, Icon, IconSize, IconSize));
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
	Text->SetText(Label);
	Text->SetColorAndOpacity(FSlateColor(TextColor));
	Text->SetJustification(ETextJustify::Center);
	SetTextSize(Text, TextSize);
	UHorizontalBoxSlot* TextSlot = Content->AddChildToHorizontalBox(Text);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	Button->AddChild(Content);
}

struct FCommandElementStyle
{
	const TCHAR* Background;
	const TCHAR* Icon;
	FText Label;
};

FCommandElementStyle ElementStyleForDefinition(
	const FBotanicusItemDefinition& Definition)
{
	const FString Search =
		(Definition.ItemKey.ToString() + TEXT(" ") +
		 Definition.DisplayName.ToString()).ToLower();
	if (Search.Contains(TEXT("feu")) || Search.Contains(TEXT("braise")) ||
		Search.Contains(TEXT("flamme")))
	{
		return {TEXT("T_Command_BadgeFire"), TEXT("T_Command_Fire"),
			NSLOCTEXT("BotanicusOrders", "FireBadge", "FEU")};
	}
	if (Search.Contains(TEXT("glace")) || Search.Contains(TEXT("givre")) ||
		Search.Contains(TEXT("cristal")))
	{
		return {TEXT("T_Command_BadgeIce"), TEXT("T_Command_Ice"),
			NSLOCTEXT("BotanicusOrders", "IceBadge", "GLACE")};
	}
	if (Search.Contains(TEXT("eau")) || Search.Contains(TEXT("hyd")) ||
		Search.Contains(TEXT("onde")))
	{
		return {TEXT("T_Command_BadgeWater"), TEXT("T_Command_Water"),
			NSLOCTEXT("BotanicusOrders", "WaterBadge", "EAU")};
	}
	if (Search.Contains(TEXT("tenebre")) || Search.Contains(TEXT("ténèbre")) ||
		Search.Contains(TEXT("ombra")) || Search.Contains(TEXT("noct")))
	{
		return {TEXT("T_Command_BadgeShadow"), TEXT("T_Command_Level"),
			NSLOCTEXT("BotanicusOrders", "ShadowBadge", "RARE")};
	}
	return {TEXT("T_Command_BadgeNature"), TEXT("T_Command_Nature"),
		NSLOCTEXT("BotanicusOrders", "NatureBadge", "NATURE")};
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

void UBotanicusWorkbenchUpgradeRowWidget::InitializeRow(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	RefreshProgress();
}

void UBotanicusWorkbenchUpgradeRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusWorkbenchUpgradeRowWidget::BuildLayout()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.04f, 0.20f, 0.34f, 0.99f));
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
		&UBotanicusWorkbenchUpgradeRowWidget::HandleUpgradeClicked);
}

void UBotanicusWorkbenchUpgradeRowWidget::RefreshProgress()
{
	if (!BotanicusController)
	{
		return;
	}
	const int32 Level =
		BotanicusController->GetPreparationWorkbenchLevel();
	const int32 Cost =
		BotanicusController->GetPreparationWorkbenchUpgradeCost();
	const bool bMaximumLevel = Level >= 5;

	if (NameLabel)
	{
		NameLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"ATELIER DE PREPARATION - NIVEAU %d\n%d EMPLACEMENT%s POUR POT DE VENTE"),
					Level,
					Level,
					Level > 1 ? TEXT("S") : TEXT(""))));
	}
	if (CostLabel)
	{
		CostLabel->SetText(
			bMaximumLevel
				? FText::FromString(TEXT("NIVEAU MAXIMUM"))
				: FText::Format(
					NSLOCTEXT(
						"BotanicusOrders",
						"WorkbenchUpgradeCost",
						"{0} credits"),
					FText::AsNumber(Cost)));
	}
	if (UpgradeButtonLabel)
	{
		UpgradeButtonLabel->SetText(
			bMaximumLevel
				? FText::FromString(TEXT("MAXIMUM"))
				: FText::Format(
					NSLOCTEXT(
						"BotanicusOrders",
						"WorkbenchUpgradeButton",
						"PASSER AU NIVEAU {0}"),
					FText::AsNumber(Level + 1)));
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(
			BotanicusController->
				CanUpgradePreparationWorkbench());
	}
}

void UBotanicusWorkbenchUpgradeRowWidget::HandleUpgradeClicked()
{
	if (BotanicusController)
	{
		BotanicusController->UpgradePreparationWorkbench();
	}
}

void UBotanicusOrderItemRowWidget::InitializeRow(
	ABotanicusPlayerController* InController,
	const FBotanicusItemDefinition& InDefinition,
	bool bInShowSeedElement)
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
			ItemKey == TEXT("SelfCheckout")
				? NSLOCTEXT(
					"BotanicusOrders",
					"SelfCheckoutDetails",
					"1 par commande - boutique niveau 3 - livraison 5 s")
				: FText::Format(
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
	if (ItemIcon)
	{
		UTexture2D* Texture = InDefinition.Icon.LoadSynchronous();
		if (Texture)
		{
			ItemIcon->SetBrushFromTexture(Texture, true);
		}
		if (ItemIconArea)
		{
			ItemIconArea->SetVisibility(
				!bInShowSeedElement && Texture
					? ESlateVisibility::HitTestInvisible
					: ESlateVisibility::Collapsed);
		}
	}
	if (ElementBadgeArea)
	{
		ElementBadgeArea->SetVisibility(
			bInShowSeedElement ? ESlateVisibility::HitTestInvisible
							   : ESlateVisibility::Collapsed);
	}
	const FCommandElementStyle ElementStyle =
		ElementStyleForDefinition(InDefinition);
	if (bInShowSeedElement && ElementBadgeBackground)
	{
		if (UTexture2D* Texture =
			LoadCommandTexture(ElementStyle.Background))
		{
			ElementBadgeBackground->SetBrushFromTexture(Texture, true);
		}
	}
	if (bInShowSeedElement && ElementBadgeIcon)
	{
		if (UTexture2D* Texture =
			LoadCommandTexture(ElementStyle.Icon))
		{
			ElementBadgeIcon->SetBrushFromTexture(Texture, true);
		}
	}
	if (bInShowSeedElement && ElementBadgeLabel)
	{
		ElementBadgeLabel->SetText(ElementStyle.Label);
	}
	RefreshAvailability(
		BotanicusController
			? BotanicusController->GetAvailableFunds()
			: 0);
}

void UBotanicusOrderItemRowWidget::RefreshAvailability(
	int32 AvailableFunds)
{
	if (DetailsLabel &&
		ItemKey == TEXT("SelfCheckout") &&
		BotanicusController)
	{
		const int32 Level =
			BotanicusController->GetMainShopLevel();
		const int32 Limit =
			BotanicusController->GetSelfCheckoutLimit();
		const int32 Count =
			BotanicusController->
				GetSelfCheckoutOwnedOrOrderedCount();
		DetailsLabel->SetText(
			Level < 3
				? FText::FromString(
					TEXT(
						"VERROUILLEE - boutique niveau 3 - 1 par commande"))
				: FText::FromString(
					FString::Printf(
						TEXT(
							"1 par commande - limite %d/%d - livraison 5 s"),
						Count,
						Limit)));
	}
	if (OrderButton)
	{
		OrderButton->SetIsEnabled(
			BotanicusController &&
			!ItemKey.IsNone() &&
			AvailableFunds >= Price &&
			BotanicusController->
				CanOrderCatalogItem(ItemKey));
	}
}

void UBotanicusOrderItemRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusOrderItemRowWidget::BuildLayout()
{
	const UBotanicusCommandPanelSettings* Layout =
		GetDefault<UBotanicusCommandPanelSettings>();
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("OrderItemBorder"));
	Root->SetBrushColor(FLinearColor::White);
	if (UTexture2D* ItemBackground =
		LoadCommandTexture(TEXT("T_Command_ItemBackground")))
	{
		Root->SetBrushFromTexture(ItemBackground);
	}
	Root->SetPadding(FMargin(26.0f, 6.0f, 10.0f, 6.0f));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* Row =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChild(Row);

	ItemIcon = WidgetTree->ConstructWidget<UImage>();
	ItemIcon->SetDesiredSizeOverride(Layout->ItemIcon.Size);
	ApplyLayoutOffset(ItemIcon, Layout->ItemIcon.Offset);
	ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	ItemIconArea = WrapAtSize(
		WidgetTree,
		ItemIcon,
		Layout->ItemIcon.Size.X,
		Layout->ItemIcon.Size.Y);
	UHorizontalBoxSlot* ItemIconSlot =
		Row->AddChildToHorizontalBox(ItemIconArea);
	ItemIconSlot->SetVerticalAlignment(VAlign_Center);
	ItemIconSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));

	UVerticalBox* Description =
		WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* DescriptionSlot =
		Row->AddChildToHorizontalBox(Description);
	DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DescriptionSlot->SetVerticalAlignment(VAlign_Center);

	NameLabel = WidgetTree->ConstructWidget<UTextBlock>();
	NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameLabel->SetColorAndOpacity(FSlateColor(CommandCream));
	SetTextSize(NameLabel, 16);
	Description->AddChildToVerticalBox(NameLabel);

	DetailsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	DetailsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.83f, 0.76f, 0.58f, 1.0f)));
	SetTextSize(DetailsLabel, 11);
	Description->AddChildToVerticalBox(DetailsLabel);

	UOverlay* ElementBadge = WidgetTree->ConstructWidget<UOverlay>();
	ElementBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
	ElementBadgeBackground = WidgetTree->ConstructWidget<UImage>();
	ElementBadgeBackground->SetDesiredSizeOverride(Layout->ElementBadge.Size);
	ElementBadge->AddChildToOverlay(ElementBadgeBackground);
	UHorizontalBox* ElementBadgeContent =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	ElementBadgeContent->SetVisibility(ESlateVisibility::HitTestInvisible);
	ElementBadgeIcon = WidgetTree->ConstructWidget<UImage>();
	ElementBadgeIcon->SetDesiredSizeOverride(FVector2D(17.0f, 17.0f));
	UHorizontalBoxSlot* ElementIconSlot =
		ElementBadgeContent->AddChildToHorizontalBox(
			WrapAtSize(WidgetTree, ElementBadgeIcon, 17.0f, 17.0f));
	ElementIconSlot->SetVerticalAlignment(VAlign_Center);
	ElementIconSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
	ElementBadgeLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ElementBadgeLabel->SetColorAndOpacity(FSlateColor(CommandCream));
	ElementBadgeLabel->SetJustification(ETextJustify::Center);
	SetTextSize(ElementBadgeLabel, 9);
	ElementBadgeContent->AddChildToHorizontalBox(ElementBadgeLabel)
		->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* ElementContentSlot =
		ElementBadge->AddChildToOverlay(ElementBadgeContent);
	ElementContentSlot->SetHorizontalAlignment(HAlign_Center);
	ElementContentSlot->SetVerticalAlignment(VAlign_Center);
	ElementBadgeArea = WrapAtSize(
		WidgetTree,
		ElementBadge,
		Layout->ElementBadge.Size.X,
		Layout->ElementBadge.Size.Y);
	UHorizontalBoxSlot* ElementBadgeSlot =
		Row->AddChildToHorizontalBox(ElementBadgeArea);
	ApplyLayoutOffset(ElementBadge, Layout->ElementBadge.Offset);
	ElementBadgeSlot->SetVerticalAlignment(VAlign_Center);
	ElementBadgeSlot->SetPadding(FMargin(8.0f, 0.0f, 10.0f, 0.0f));

	PriceLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PriceLabel->SetColorAndOpacity(
		FSlateColor(CommandGold));
	PriceLabel->SetJustification(ETextJustify::Right);
	PriceLabel->SetMargin(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
	SetTextSize(PriceLabel, 15);
	UHorizontalBox* PriceContent =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	PriceContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UImage* CreditCoin = MakeCommandImage(
		WidgetTree, TEXT("T_Command_CreditCoin"), FVector2D(24.0f, 24.0f));
	UHorizontalBoxSlot* CoinSlot =
		PriceContent->AddChildToHorizontalBox(
			WrapAtSize(WidgetTree, CreditCoin, 24.0f, 24.0f));
	CoinSlot->SetVerticalAlignment(VAlign_Center);
	PriceContent->AddChildToHorizontalBox(PriceLabel)
		->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* PriceSlot =
		Row->AddChildToHorizontalBox(
			WrapAtSize(
				WidgetTree,
				PriceContent,
				Layout->PriceArea.Size.X,
				Layout->PriceArea.Size.Y));
	ApplyLayoutOffset(PriceContent, Layout->PriceArea.Offset);
	PriceSlot->SetVerticalAlignment(VAlign_Center);

	OrderButton = WidgetTree->ConstructWidget<UButton>();
	ApplyTextureButtonStyle(
		OrderButton,
		TEXT("T_Command_OrderNormal"),
		TEXT("T_Command_OrderPressed"),
		Layout->OrderButton.Size);
	ApplyLayoutOffset(OrderButton, Layout->OrderButton.Offset);
	AddIconAndText(
		WidgetTree,
		OrderButton,
		TEXT("T_Command_Cart"),
		NSLOCTEXT("BotanicusOrders", "OrderButton", "COMMANDER"),
		22.0f,
		12);
	UHorizontalBoxSlot* ButtonSlot =
		Row->AddChildToHorizontalBox(
			WrapAtSize(
				WidgetTree,
				OrderButton,
				Layout->OrderButton.Size.X,
				Layout->OrderButton.Size.Y));
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
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
	SetIsFocusable(true);
	const bool bDesignerLayoutBound = BindDesignerLayout();
	if (!bDesignerLayoutBound && GetClass() == StaticClass())
	{
		BuildLayout();
	}
	else if (!bDesignerLayoutBound)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("WBP_CommandComputer is active, but one or more required Designer widgets are missing. The Blueprint layout is preserved."));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("WBP_CommandComputer Designer layout successfully bound (%s)."),
			*GetClass()->GetPathName());
	}
}

bool UBotanicusOrderCatalogWidget::BindDesignerLayout()
{
	auto FindDesignerWidget = [this](const TCHAR* BaseName) -> UWidget*
	{
		if (UWidget* Widget = WidgetTree->FindWidget(BaseName))
		{
			return Widget;
		}
		return WidgetTree->FindWidget(
			*FString::Printf(TEXT("Designer_%s"), BaseName));
	};
	FundsLabel = Cast<UTextBlock>(FindDesignerWidget(TEXT("FundsLabel")));
	LevelLabel = Cast<UTextBlock>(FindDesignerWidget(TEXT("LevelLabel")));
	PendingOrdersLabel = Cast<UTextBlock>(FindDesignerWidget(TEXT("PendingOrdersLabel")));
	PendingOrdersBox = Cast<UVerticalBox>(FindDesignerWidget(TEXT("PendingOrdersBox")));
	ItemsScroll = Cast<UScrollBox>(FindDesignerWidget(TEXT("ItemsScroll")));
	ItemsBox = Cast<UVerticalBox>(FindDesignerWidget(TEXT("ItemsBox")));
	CatalogScrollSlider = Cast<USlider>(FindDesignerWidget(TEXT("CatalogScrollSlider")));
	ShopOpenButton = Cast<UButton>(FindDesignerWidget(TEXT("ShopOpenButton")));
	ShopOpenButtonLabel = Cast<UTextBlock>(FindDesignerWidget(TEXT("ShopOpenButtonLabel")));
	ShopStateIcon = Cast<UImage>(FindDesignerWidget(TEXT("ShopStateIcon")));
	ShopStateBackground = Cast<UImage>(FindDesignerWidget(TEXT("ShopStateBackground")));

	// The editable Blueprint uses three independent visual layers for the shop
	// state. If no Button was kept in the asset, place a transparent functional
	// button over the background while preserving the Designer geometry.
	if (!ShopOpenButton && ShopStateBackground)
	{
		if (UCanvasPanel* ShopCanvas =
			Cast<UCanvasPanel>(ShopStateBackground->GetParent()))
		{
			ShopOpenButton = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), TEXT("RuntimeShopOpenButton"));
			MakeButtonChromeInvisible(ShopOpenButton);
			if (UCanvasPanelSlot* RuntimeSlot =
				ShopCanvas->AddChildToCanvas(ShopOpenButton))
			{
				if (const UCanvasPanelSlot* VisualSlot =
					Cast<UCanvasPanelSlot>(ShopStateBackground->Slot))
				{
					RuntimeSlot->SetLayout(VisualSlot->GetLayout());
					RuntimeSlot->SetAutoSize(VisualSlot->GetAutoSize());
					RuntimeSlot->SetZOrder(VisualSlot->GetZOrder() + 10);
				}
			}
		}
	}
	if (ShopStateBackground)
	{
		ShopStateBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (ShopStateIcon)
	{
		ShopStateIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (ShopOpenButtonLabel)
	{
		ShopOpenButtonLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (ShopOpenButton &&
		(!ShopOpenButtonLabel ||
		 ShopOpenButtonLabel->GetVisibility() == ESlateVisibility::Collapsed))
	{
		if (UTextBlock* VisibleButtonLabel =
			Cast<UTextBlock>(ShopOpenButton->GetContent()))
		{
			ShopOpenButtonLabel = VisibleButtonLabel;
		}
	}

	if (ItemsScroll)
	{
		ItemsScroll->OnUserScrolled.AddUniqueDynamic(
			this, &UBotanicusOrderCatalogWidget::HandleCatalogScrollChanged);
	}
	if (CatalogScrollSlider)
	{
		CatalogScrollSlider->OnValueChanged.AddUniqueDynamic(
			this, &UBotanicusOrderCatalogWidget::HandleCatalogSliderChanged);
	}
	if (ShopOpenButton)
	{
		ShopOpenButton->SetVisibility(ESlateVisibility::Visible);
		ShopOpenButton->OnClicked.AddUniqueDynamic(
			this, &UBotanicusOrderCatalogWidget::HandleShopOpenClicked);
	}

	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("CloseButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleCloseClicked);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("SeedsTab"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleSeedsTabClicked);
		TabButtons.Add(Button);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("ToolsTab"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleToolsTabClicked);
		TabButtons.Add(Button);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("PreparationTab"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandlePreparationTabClicked);
		TabButtons.Add(Button);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("SalesTab"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleSalesTabClicked);
		TabButtons.Add(Button);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("BuildingsTab"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleBuildingsTabClicked);
		TabButtons.Add(Button);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("ScrollUpButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleCatalogScrollUpClicked);
	}
	if (UButton* Button = Cast<UButton>(FindDesignerWidget(TEXT("ScrollDownButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UBotanicusOrderCatalogWidget::HandleCatalogScrollDownClicked);
	}
	const bool bHasCoreDesignerLayout =
		FundsLabel && LevelLabel && ItemsScroll && ItemsBox && PendingOrdersBox;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Command Designer binding: Funds=%d Level=%d ItemsScroll=%d ItemsBox=%d PendingBox=%d ShopButton=%d Tabs=%d Close=%d"),
		FundsLabel != nullptr,
		LevelLabel != nullptr,
		ItemsScroll != nullptr,
		ItemsBox != nullptr,
		PendingOrdersBox != nullptr,
		ShopOpenButton != nullptr,
		TabButtons.Num(),
		FindDesignerWidget(TEXT("CloseButton")) != nullptr);
	return bHasCoreDesignerLayout;
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
	SyncCatalogScrollbar();
}

void UBotanicusOrderCatalogWidget::BuildLayout()
{
	const UBotanicusCommandPanelSettings* Layout =
		GetDefault<UBotanicusCommandPanelSettings>();
	UCanvasPanel* StyledRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("OrderCatalogRoot"));
	WidgetTree->RootWidget = StyledRoot;

	UImage* Frame = MakeCommandImage(
		WidgetTree,
		TEXT("T_Command_Frame"),
		FVector2D(1672.0f, 941.0f));
	UCanvasPanelSlot* FrameSlot = StyledRoot->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	FrameSlot->SetOffsets(FMargin(0.0f));

	UBorder* StyledPanel = WidgetTree->ConstructWidget<UBorder>();
	StyledPanel->SetBrushColor(FLinearColor::White);
	if (UTexture2D* ShopPanel =
		LoadCommandTexture(TEXT("T_Command_ShopPanel")))
	{
		StyledPanel->SetBrushFromTexture(ShopPanel);
	}
	StyledPanel->SetPadding(FMargin(14.0f, 10.0f));
	UCanvasPanelSlot* StyledPanelSlot =
		StyledRoot->AddChildToCanvas(StyledPanel);
	StyledPanelSlot->SetAnchors(FAnchors(
		Layout->ScreenMinAnchor.X,
		Layout->ScreenMinAnchor.Y,
		Layout->ScreenMaxAnchor.X,
		Layout->ScreenMaxAnchor.Y));
	StyledPanelSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* StyledColumn =
		WidgetTree->ConstructWidget<UVerticalBox>();
	StyledPanel->AddChild(StyledColumn);

	UBorder* HeaderSurface = WidgetTree->ConstructWidget<UBorder>();
	HeaderSurface->SetBrushColor(
		FLinearColor(0.025f, 0.125f, 0.075f, 0.98f));
	HeaderSurface->SetPadding(FMargin(10.0f, 5.0f));
	UVerticalBoxSlot* StyledHeaderSlot =
		StyledColumn->AddChildToVerticalBox(HeaderSurface);
	StyledHeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	UHorizontalBox* StyledHeader =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	HeaderSurface->AddChild(StyledHeader);

	UImage* Logo = MakeCommandImage(
		WidgetTree, TEXT("T_Command_Nature"), Layout->Logo.Size);
	ApplyLayoutOffset(Logo, Layout->Logo.Offset);
	UHorizontalBoxSlot* LogoSlot =
		StyledHeader->AddChildToHorizontalBox(
			WrapAtSize(
				WidgetTree,
				Logo,
				Layout->Logo.Size.X,
				Layout->Logo.Size.Y));
	LogoSlot->SetVerticalAlignment(VAlign_Center);
	LogoSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));

	UVerticalBox* TitleColumn =
		WidgetTree->ConstructWidget<UVerticalBox>();
	ApplyLayoutOffset(TitleColumn, Layout->TitleOffset);
	UTextBlock* StyledTitle = WidgetTree->ConstructWidget<UTextBlock>();
	StyledTitle->SetText(
		NSLOCTEXT("BotanicusOrders", "CatalogTitle", "PANNEAU DE COMMANDE"));
	StyledTitle->SetColorAndOpacity(FSlateColor(CommandCream));
	SetTextSize(StyledTitle, 25);
	TitleColumn->AddChildToVerticalBox(StyledTitle);
	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>();
	Subtitle->SetText(NSLOCTEXT(
		"BotanicusOrders",
		"CatalogSubtitle",
		"TERMINAL DE GESTION - BOTANICUS"));
	Subtitle->SetColorAndOpacity(FSlateColor(CommandGold));
	SetTextSize(Subtitle, 10);
	TitleColumn->AddChildToVerticalBox(Subtitle);
	UHorizontalBoxSlot* StyledTitleSlot =
		StyledHeader->AddChildToHorizontalBox(TitleColumn);
	StyledTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StyledTitleSlot->SetVerticalAlignment(VAlign_Center);

	auto MakeHeaderCard = [this, StyledHeader](
		const TCHAR* IconAsset,
		const TCHAR* BackgroundAsset,
		TObjectPtr<UTextBlock>& ValueLabel,
		const FText& Caption,
		const FBotanicusCommandElementLayout& ElementLayout)
	{
		UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>();
		ApplyLayoutOffset(Card, ElementLayout.Offset);
		UImage* CardBackground = MakeCommandImage(
			WidgetTree, BackgroundAsset, ElementLayout.Size);
		Card->AddChildToOverlay(CardBackground);
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		CardRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UImage* CardIcon = MakeCommandImage(
			WidgetTree, IconAsset, FVector2D(28.0f, 28.0f));
		UHorizontalBoxSlot* CardIconSlot =
			CardRow->AddChildToHorizontalBox(
				WrapAtSize(WidgetTree, CardIcon, 28.0f, 28.0f));
		CardIconSlot->SetVerticalAlignment(VAlign_Center);
		CardIconSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));
		UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>();
		ValueLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ValueLabel->SetColorAndOpacity(FSlateColor(CommandCream));
		SetTextSize(ValueLabel, 13);
		Copy->AddChildToVerticalBox(ValueLabel);
		UTextBlock* CardCaption = WidgetTree->ConstructWidget<UTextBlock>();
		CardCaption->SetText(Caption);
		CardCaption->SetColorAndOpacity(FSlateColor(CommandGold));
		SetTextSize(CardCaption, 8);
		Copy->AddChildToVerticalBox(CardCaption);
		CardRow->AddChildToHorizontalBox(Copy)->SetVerticalAlignment(VAlign_Center);
		UOverlaySlot* CardRowSlot = Card->AddChildToOverlay(CardRow);
		CardRowSlot->SetHorizontalAlignment(HAlign_Center);
		CardRowSlot->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* CardSlot =
			StyledHeader->AddChildToHorizontalBox(
				WrapAtSize(
					WidgetTree,
					Card,
					ElementLayout.Size.X,
					ElementLayout.Size.Y));
		CardSlot->SetVerticalAlignment(VAlign_Center);
		CardSlot->SetPadding(FMargin(5.0f, 0.0f));
	};

	MakeHeaderCard(
		TEXT("T_Command_CreditCoin"),
		TEXT("T_Command_CreditsBackground"),
		FundsLabel,
		NSLOCTEXT("BotanicusOrders", "CreditsCaption", "CREDITS"),
		Layout->Credits);
	MakeHeaderCard(
		TEXT("T_Command_Level"),
		TEXT("T_Command_LevelBackground"),
		LevelLabel,
		NSLOCTEXT("BotanicusOrders", "LevelCaption", "BOUTIQUE"),
		Layout->Level);

	ShopOpenButton = WidgetTree->ConstructWidget<UButton>();
	MakeButtonChromeInvisible(ShopOpenButton);
	UOverlay* ShopOverlay = WidgetTree->ConstructWidget<UOverlay>();
	ShopStateBackground = MakeCommandImage(
		WidgetTree,
		TEXT("T_Command_ShopClosedBackground"),
		Layout->ShopState.Size);
	ShopStateBackground->SetColorAndOpacity(FLinearColor::White);
	ShopOverlay->AddChildToOverlay(ShopStateBackground);
	UHorizontalBox* ShopContent = WidgetTree->ConstructWidget<UHorizontalBox>();
	ShopContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ShopStateIcon = MakeCommandImage(
		WidgetTree,
		TEXT("T_Command_ShopClosedIcon"),
		FVector2D(24.0f, 24.0f));
	UHorizontalBoxSlot* ShopIconSlot =
		ShopContent->AddChildToHorizontalBox(
			WrapAtSize(WidgetTree, ShopStateIcon, 24.0f, 24.0f));
	ShopIconSlot->SetVerticalAlignment(VAlign_Center);
	ShopIconSlot->SetPadding(FMargin(10.0f, 0.0f, 7.0f, 0.0f));
	UVerticalBox* ShopCopy = WidgetTree->ConstructWidget<UVerticalBox>();
	ShopOpenButtonLabel = WidgetTree->ConstructWidget<UTextBlock>();
	ShopOpenButtonLabel->SetColorAndOpacity(FSlateColor(CommandCream));
	ShopOpenButtonLabel->SetJustification(ETextJustify::Center);
	SetTextSize(ShopOpenButtonLabel, 10);
	ShopCopy->AddChildToVerticalBox(ShopOpenButtonLabel);
	ShopContent->AddChildToHorizontalBox(ShopCopy)->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* ShopContentSlot = ShopOverlay->AddChildToOverlay(ShopContent);
	ShopContentSlot->SetHorizontalAlignment(HAlign_Center);
	ShopContentSlot->SetVerticalAlignment(VAlign_Center);
	ShopOpenButton->AddChild(ShopOverlay);
	ApplyLayoutOffset(ShopOpenButton, Layout->ShopState.Offset);
	UHorizontalBoxSlot* StyledShopSlot =
		StyledHeader->AddChildToHorizontalBox(
			WrapAtSize(
				WidgetTree,
				ShopOpenButton,
				Layout->ShopState.Size.X,
				Layout->ShopState.Size.Y));
	StyledShopSlot->SetVerticalAlignment(VAlign_Center);
	StyledShopSlot->SetPadding(FMargin(5.0f, 0.0f));
	ShopOpenButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleShopOpenClicked);

	UButton* StyledCloseButton = WidgetTree->ConstructWidget<UButton>();
	ApplyTextureButtonStyle(
		StyledCloseButton,
		TEXT("T_Command_CloseNormal"),
		TEXT("T_Command_ClosePressed"),
		Layout->CloseButton.Size);
	ApplyLayoutOffset(StyledCloseButton, Layout->CloseButton.Offset);
	AddIconAndText(
		WidgetTree,
		StyledCloseButton,
		TEXT("T_Command_Close"),
		NSLOCTEXT("BotanicusOrders", "Close", "FERMER"),
		25.0f,
		13);
	UHorizontalBoxSlot* StyledCloseSlot =
		StyledHeader->AddChildToHorizontalBox(
			WrapAtSize(
				WidgetTree,
				StyledCloseButton,
				Layout->CloseButton.Size.X,
				Layout->CloseButton.Size.Y));
	StyledCloseSlot->SetVerticalAlignment(VAlign_Center);
	StyledCloseSlot->SetPadding(FMargin(5.0f, 0.0f, 0.0f, 0.0f));
	StyledCloseButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleCloseClicked);

	UBorder* TabsSurface = WidgetTree->ConstructWidget<UBorder>();
	TabsSurface->SetBrushColor(
		FLinearColor(0.020f, 0.105f, 0.060f, 0.99f));
	TabsSurface->SetPadding(FMargin(5.0f, 5.0f));
	UVerticalBoxSlot* StyledTabsSlot =
		StyledColumn->AddChildToVerticalBox(TabsSurface);
	StyledTabsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	UHorizontalBox* StyledTabs =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	TabsSurface->AddChild(StyledTabs);

	auto AddStyledTab = [this, StyledTabs, Layout](
		const TCHAR* IconAsset, const FText& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		MakeButtonChromeInvisible(Button);
		ApplyLayoutOffset(Button, Layout->Tab.Offset);
		UOverlay* TabOverlay = WidgetTree->ConstructWidget<UOverlay>();
		UImage* TabBackground = MakeCommandImage(
			WidgetTree,
			TEXT("T_Command_TabBackground"),
			Layout->Tab.Size);
		TabBackground->SetColorAndOpacity(FLinearColor::White);
		TabOverlay->AddChildToOverlay(TabBackground);
		UHorizontalBox* TabContent = WidgetTree->ConstructWidget<UHorizontalBox>();
		TabContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UImage* TabIcon = MakeCommandImage(
			WidgetTree,
			IconAsset,
			FVector2D(Layout->TabIconSize, Layout->TabIconSize));
		UHorizontalBoxSlot* TabIconSlot =
			TabContent->AddChildToHorizontalBox(
				WrapAtSize(
					WidgetTree,
					TabIcon,
					Layout->TabIconSize,
					Layout->TabIconSize));
		TabIconSlot->SetVerticalAlignment(VAlign_Center);
		TabIconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		UTextBlock* TabText = WidgetTree->ConstructWidget<UTextBlock>();
		TabText->SetText(Label);
		TabText->SetColorAndOpacity(FSlateColor(CommandCream));
		SetTextSize(TabText, 12);
		TabContent->AddChildToHorizontalBox(TabText)->SetVerticalAlignment(VAlign_Center);
		UOverlaySlot* TabContentSlot = TabOverlay->AddChildToOverlay(TabContent);
		TabContentSlot->SetHorizontalAlignment(HAlign_Center);
		TabContentSlot->SetVerticalAlignment(VAlign_Center);
		Button->AddChild(WrapAtSize(
			WidgetTree,
			TabOverlay,
			Layout->Tab.Size.X,
			Layout->Tab.Size.Y));
		UHorizontalBoxSlot* Slot =
			StyledTabs->AddChildToHorizontalBox(Button);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(2.0f));
		TabButtons.Add(Button);
		return Button;
	};

	UButton* StyledSeedsButton = AddStyledTab(
		TEXT("T_Command_Seeds"),
		NSLOCTEXT("BotanicusOrders", "SeedsTab", "GRAINES"));
	StyledSeedsButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleSeedsTabClicked);
	UButton* StyledToolsButton = AddStyledTab(
		TEXT("T_Command_WateringCan"),
		NSLOCTEXT("BotanicusOrders", "ToolsTab", "OUTILS DE JARDINAGE"));
	StyledToolsButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleToolsTabClicked);
	UButton* StyledPreparationButton = AddStyledTab(
		TEXT("T_Command_Preparation"),
		NSLOCTEXT("BotanicusOrders", "PreparationTab", "PREPARATION"));
	StyledPreparationButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandlePreparationTabClicked);
	UButton* StyledSalesButton = AddStyledTab(
		TEXT("T_Command_Sales"),
		NSLOCTEXT("BotanicusOrders", "SalesTab", "VENTE"));
	StyledSalesButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleSalesTabClicked);
	UButton* StyledBuildingsButton = AddStyledTab(
		TEXT("T_Command_Buildings"),
		NSLOCTEXT("BotanicusOrders", "BuildingsTab", "BATIMENTS"));
	StyledBuildingsButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleBuildingsTabClicked);

	UHorizontalBox* MainArea = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* MainAreaSlot =
		StyledColumn->AddChildToVerticalBox(MainArea);
	MainAreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UBorder* CatalogBorder = WidgetTree->ConstructWidget<UBorder>();
	CatalogBorder->SetBrushColor(FLinearColor(0.018f, 0.085f, 0.048f, 0.99f));
	CatalogBorder->SetPadding(FMargin(8.0f));
	UHorizontalBoxSlot* CatalogBorderSlot =
		MainArea->AddChildToHorizontalBox(CatalogBorder);
	CatalogBorderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CatalogBorderSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	UOverlay* ItemsOverlay = WidgetTree->ConstructWidget<UOverlay>();
	CatalogBorder->AddChild(ItemsOverlay);
	ItemsScroll = WidgetTree->ConstructWidget<UScrollBox>();
	ItemsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	ItemsScroll->OnUserScrolled.AddDynamic(
		this,
		&UBotanicusOrderCatalogWidget::HandleCatalogScrollChanged);
	ItemsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	ItemsScroll->AddChild(ItemsBox);
	UOverlaySlot* ItemsScrollSlot =
		ItemsOverlay->AddChildToOverlay(ItemsScroll);
	ItemsScrollSlot->SetHorizontalAlignment(HAlign_Fill);
	ItemsScrollSlot->SetVerticalAlignment(VAlign_Fill);
	ItemsScrollSlot->SetPadding(FMargin(0.0f, 0.0f, 31.0f, 0.0f));
	UVerticalBox* CatalogScrollControls =
		WidgetTree->ConstructWidget<UVerticalBox>();
	UButton* ScrollUpButton = WidgetTree->ConstructWidget<UButton>();
	ScrollUpButton->SetBackgroundColor(FLinearColor::Transparent);
	UImage* ScrollUpImage = MakeCommandImage(
		WidgetTree, TEXT("T_Command_ScrollArrow"), FVector2D(22.0f, 22.0f));
	ScrollUpImage->SetRenderTransformAngle(180.0f);
	ScrollUpButton->AddChild(ScrollUpImage);
	CatalogScrollControls->AddChildToVerticalBox(
		WrapAtSize(WidgetTree, ScrollUpButton, 24.0f, 24.0f));
	ScrollUpButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleCatalogScrollUpClicked);

	CatalogScrollSlider = WidgetTree->ConstructWidget<USlider>();
	CatalogScrollSlider->SetOrientation(Orient_Vertical);
	CatalogScrollSlider->SetMinValue(0.0f);
	CatalogScrollSlider->SetMaxValue(1.0f);
	CatalogScrollSlider->SetStepSize(0.01f);
	CatalogScrollSlider->SetIndentHandle(false);
	CatalogScrollSlider->SetSliderBarColor(FLinearColor(0.03f, 0.13f, 0.07f, 1.0f));
	CatalogScrollSlider->SetSliderHandleColor(FLinearColor::White);
	FSliderStyle SliderStyle = CatalogScrollSlider->GetWidgetStyle();
	if (UTexture2D* ThumbTexture = LoadCommandTexture(TEXT("T_Command_ScrollThumb")))
	{
		FSlateBrush ThumbBrush;
		ThumbBrush.SetResourceObject(ThumbTexture);
		// Slate rotates a vertical slider's thumb by 90 degrees.
		ThumbBrush.ImageSize = FVector2D(76.0f, 22.0f);
		ThumbBrush.DrawAs = ESlateBrushDrawType::Image;
		SliderStyle.SetNormalThumbImage(ThumbBrush);
		SliderStyle.SetHoveredThumbImage(ThumbBrush);
		SliderStyle.SetDisabledThumbImage(ThumbBrush);
	}
	FSlateBrush InvisibleBar;
	InvisibleBar.DrawAs = ESlateBrushDrawType::NoDrawType;
	InvisibleBar.ImageSize = FVector2D(1.0f, 1.0f);
	SliderStyle.SetNormalBarImage(InvisibleBar);
	SliderStyle.SetHoveredBarImage(InvisibleBar);
	SliderStyle.SetDisabledBarImage(InvisibleBar);
	SliderStyle.BarThickness = 2.0f;
	CatalogScrollSlider->SetWidgetStyle(SliderStyle);
	CatalogScrollSlider->OnValueChanged.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleCatalogSliderChanged);
	UOverlay* SliderOverlay = WidgetTree->ConstructWidget<UOverlay>();
	UImage* SliderTrack = MakeCommandImage(
		WidgetTree,
		TEXT("T_Command_ScrollTrack"),
		FVector2D(24.0f, 366.0f));
	SliderOverlay->AddChildToOverlay(SliderTrack);
	UOverlaySlot* FunctionalSliderSlot =
		SliderOverlay->AddChildToOverlay(CatalogScrollSlider);
	FunctionalSliderSlot->SetHorizontalAlignment(HAlign_Fill);
	FunctionalSliderSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBoxSlot* SliderSlot =
		CatalogScrollControls->AddChildToVerticalBox(
			WrapAtSize(WidgetTree, SliderOverlay, 24.0f, 366.0f));
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* ScrollDownButton = WidgetTree->ConstructWidget<UButton>();
	ScrollDownButton->SetBackgroundColor(FLinearColor::Transparent);
	ScrollDownButton->AddChild(MakeCommandImage(
		WidgetTree, TEXT("T_Command_ScrollArrow"), FVector2D(22.0f, 22.0f)));
	CatalogScrollControls->AddChildToVerticalBox(
		WrapAtSize(WidgetTree, ScrollDownButton, 24.0f, 24.0f));
	ScrollDownButton->OnClicked.AddDynamic(
		this, &UBotanicusOrderCatalogWidget::HandleCatalogScrollDownClicked);
	UOverlaySlot* CatalogScrollSlot =
		ItemsOverlay->AddChildToOverlay(
			WrapAtSize(
				WidgetTree,
				CatalogScrollControls,
				Layout->Scrollbar.Size.X,
				Layout->Scrollbar.Size.Y));
	ApplyLayoutOffset(CatalogScrollControls, Layout->Scrollbar.Offset);
	CatalogScrollSlot->SetHorizontalAlignment(HAlign_Right);
	CatalogScrollSlot->SetVerticalAlignment(VAlign_Center);
	CatalogScrollSlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));

	UBorder* DeliveryBorder = WidgetTree->ConstructWidget<UBorder>();
	DeliveryBorder->SetBrushColor(FLinearColor(0.025f, 0.115f, 0.066f, 0.99f));
	DeliveryBorder->SetPadding(FMargin(9.0f));
	USizeBox* DeliverySize = WrapAtSize(
		WidgetTree, DeliveryBorder, Layout->DeliveryPanelWidth, 1.0f);
	ApplyLayoutOffset(DeliveryBorder, Layout->DeliveryPanelOffset);
	DeliverySize->ClearHeightOverride();
	UHorizontalBoxSlot* DeliverySlot =
		MainArea->AddChildToHorizontalBox(DeliverySize);
	DeliverySlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* DeliveryColumn =
		WidgetTree->ConstructWidget<UVerticalBox>();
	DeliveryBorder->AddChild(DeliveryColumn);
	UHorizontalBox* DeliveryHeader =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	UImage* DeliveryIcon = MakeCommandImage(
		WidgetTree, TEXT("T_Command_Sales"), FVector2D(28.0f, 28.0f));
	DeliveryHeader->AddChildToHorizontalBox(
		WrapAtSize(WidgetTree, DeliveryIcon, 28.0f, 28.0f));
	UTextBlock* DeliveryTitle = WidgetTree->ConstructWidget<UTextBlock>();
	DeliveryTitle->SetText(NSLOCTEXT(
		"BotanicusOrders", "DeliveryTitle", "LIVRAISONS EN COURS"));
	DeliveryTitle->SetColorAndOpacity(FSlateColor(CommandCream));
	DeliveryTitle->SetMargin(FMargin(7.0f, 0.0f));
	SetTextSize(DeliveryTitle, 10);
	DeliveryHeader->AddChildToHorizontalBox(DeliveryTitle)->SetVerticalAlignment(VAlign_Center);
	DeliveryColumn->AddChildToVerticalBox(DeliveryHeader);

	PendingOrdersLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PendingOrdersLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.70f, 0.79f, 0.60f, 1.0f)));
	PendingOrdersLabel->SetAutoWrapText(true);
	PendingOrdersLabel->SetMargin(FMargin(0.0f, 8.0f));
	SetTextSize(PendingOrdersLabel, 9);
	DeliveryColumn->AddChildToVerticalBox(PendingOrdersLabel);
	UScrollBox* DeliveryScroll = WidgetTree->ConstructWidget<UScrollBox>();
	DeliveryScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* DeliveryScrollSlot =
		DeliveryColumn->AddChildToVerticalBox(DeliveryScroll);
	DeliveryScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PendingOrdersBox = WidgetTree->ConstructWidget<UVerticalBox>();
	DeliveryScroll->AddChild(PendingOrdersBox);

	RefreshTabButtons();
	if (StyledRoot)
	{
		return;
	}

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
	WorkbenchUpgradeRow = nullptr;
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
		Row->InitializeRow(
			BotanicusController,
			Definition,
			ActiveTab == EBotanicusCommandPanelTab::Seeds);
		UVerticalBoxSlot* RowSlot = ItemsBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		ItemRows.Add(Row);
	}
}

void UBotanicusOrderCatalogWidget::Refresh()
{
	const UBotanicusCommandPanelSettings* Layout =
		GetDefault<UBotanicusCommandPanelSettings>();
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
		ShopOpenButton->SetBackgroundColor(FLinearColor::White);
		ShopOpenButton->SetVisibility(ESlateVisibility::Visible);
		if (!ShopStateBackground ||
			ShopStateBackground->GetVisibility() == ESlateVisibility::Collapsed)
		{
			FButtonStyle ShopStyle = ShopOpenButton->GetStyle();
			const TCHAR* ShopTexture = bShopOpen
				? TEXT("T_Command_ShopOpenBackground")
				: TEXT("T_Command_ShopClosedBackground");
			const FVector2D ButtonSize = ShopOpenButton->GetDesiredSize().IsNearlyZero()
				? FVector2D(190.0f, 60.0f)
				: ShopOpenButton->GetDesiredSize();
			const FSlateBrush ShopBrush = MakeTextureBrush(ShopTexture, ButtonSize);
			ShopStyle.SetNormal(ShopBrush);
			ShopStyle.SetHovered(ShopBrush);
			ShopStyle.SetPressed(ShopBrush);
			ShopOpenButton->SetStyle(ShopStyle);
		}
		if (ShopStateBackground)
		{
			if (UTexture2D* Background = LoadCommandTexture(
				bShopOpen
					? TEXT("T_Command_ShopOpenBackground")
					: TEXT("T_Command_ShopClosedBackground")))
			{
				ShopStateBackground->SetBrushFromTexture(Background, true);
			}
		}
		if (ShopStateIcon)
		{
			if (UTexture2D* Icon = LoadCommandTexture(
				bShopOpen
					? TEXT("T_Command_ShopOpenIcon")
					: TEXT("T_Command_ShopClosedIcon")))
			{
				ShopStateIcon->SetBrushFromTexture(Icon, true);
			}
		}
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
	if (WorkbenchUpgradeRow)
	{
		WorkbenchUpgradeRow->RefreshProgress();
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
		LevelLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (!PendingOrdersLabel)
	{
		return;
	}
	const TArray<FBotanicusPendingOrder>& Orders =
		BotanicusController->GetPendingOrders();
	if (Orders.Num() == 0)
	{
		if (PendingOrdersBox && PendingOrderCardIds.Num() > 0)
		{
			PendingOrdersBox->ClearChildren();
			PendingOrderCardIds.Reset();
			PendingOrderTimeLabels.Reset();
			PendingOrderProgressBars.Reset();
		}
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
	if (PendingOrdersBox)
	{
		PendingOrdersLabel->SetText(
			FText::Format(
				NSLOCTEXT(
					"BotanicusOrders",
					"PendingCount",
					"{0} LIVRAISON(S) EN ROUTE"),
				FText::AsNumber(Orders.Num())));
		const UGameInstance* DeliveryGameInstance =
			BotanicusController->GetGameInstance();
		const UBotanicusItemCatalogSubsystem* DeliveryCatalog =
			DeliveryGameInstance
				? DeliveryGameInstance->GetSubsystem<
					UBotanicusItemCatalogSubsystem>()
				: nullptr;
		bool bMustRebuildCards =
			PendingOrderCardIds.Num() != Orders.Num();
		if (!bMustRebuildCards)
		{
			for (int32 Index = 0; Index < Orders.Num(); ++Index)
			{
				if (PendingOrderCardIds[Index] != Orders[Index].OrderId)
				{
					bMustRebuildCards = true;
					break;
				}
			}
		}

		if (bMustRebuildCards)
		{
			PendingOrdersBox->ClearChildren();
			PendingOrderCardIds.Reset();
			PendingOrderTimeLabels.Reset();
			PendingOrderProgressBars.Reset();

			for (const FBotanicusPendingOrder& Order : Orders)
			{
				UBorder* OrderCard = WidgetTree->ConstructWidget<UBorder>();
				OrderCard->SetBrushColor(
					FLinearColor(0.07f, 0.23f, 0.13f, 1.0f));
				OrderCard->SetPadding(FMargin(7.0f));
				UVerticalBoxSlot* CardSlot =
					PendingOrdersBox->AddChildToVerticalBox(OrderCard);
				CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));

				UHorizontalBox* CardRow =
					WidgetTree->ConstructWidget<UHorizontalBox>();
				OrderCard->AddChild(CardRow);
				UVerticalBox* CardColumn =
					WidgetTree->ConstructWidget<UVerticalBox>();
				UHorizontalBoxSlot* CardCopySlot =
					CardRow->AddChildToHorizontalBox(CardColumn);
				CardCopySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				CardCopySlot->SetVerticalAlignment(VAlign_Center);
				UTextBlock* OrderName =
					WidgetTree->ConstructWidget<UTextBlock>();
				OrderName->SetText(Order.DisplayName);
				OrderName->SetColorAndOpacity(FSlateColor(CommandCream));
				OrderName->SetAutoWrapText(true);
				SetTextSize(OrderName, 10);
				CardColumn->AddChildToVerticalBox(OrderName);

				UTextBlock* OrderTime =
					WidgetTree->ConstructWidget<UTextBlock>();
				OrderTime->SetColorAndOpacity(FSlateColor(CommandGold));
				SetTextSize(OrderTime, 9);
				CardColumn->AddChildToVerticalBox(OrderTime);

				UProgressBar* Progress =
					WidgetTree->ConstructWidget<UProgressBar>();
				Progress->SetFillColorAndOpacity(
					FLinearColor(0.58f, 0.78f, 0.13f, 1.0f));
				UVerticalBoxSlot* ProgressSlot =
					CardColumn->AddChildToVerticalBox(
						WrapAtSize(WidgetTree, Progress, 146.0f, 7.0f));
				ProgressSlot->SetPadding(
					FMargin(0.0f, 5.0f, 0.0f, 0.0f));

				UImage* ParcelIcon = MakeCommandImage(
					WidgetTree,
					TEXT("T_Command_Parcel"),
					Layout->ParcelIcon.Size);
				ApplyLayoutOffset(ParcelIcon, Layout->ParcelIcon.Offset);
				UHorizontalBoxSlot* ParcelSlot =
					CardRow->AddChildToHorizontalBox(
						WrapAtSize(
							WidgetTree,
							ParcelIcon,
							Layout->ParcelIcon.Size.X,
							Layout->ParcelIcon.Size.Y));
				ParcelSlot->SetVerticalAlignment(VAlign_Center);
				ParcelSlot->SetPadding(
					FMargin(7.0f, 0.0f, 0.0f, 0.0f));

				PendingOrderCardIds.Add(Order.OrderId);
				PendingOrderTimeLabels.Add(OrderTime);
				PendingOrderProgressBars.Add(Progress);
			}
		}

		for (int32 Index = 0; Index < Orders.Num(); ++Index)
		{
			const FBotanicusPendingOrder& Order = Orders[Index];
			const float Remaining = FMath::Max(
				0.0f, Order.DeliveryServerTime - ServerTime);
			float TotalDuration = FMath::Max(0.1f, Remaining);
			if (DeliveryCatalog)
			{
				if (const FBotanicusItemDefinition* Definition =
					DeliveryCatalog->FindItem(Order.ItemKey))
				{
					TotalDuration = FMath::Max(
						0.1f, Definition->DeliveryDelaySeconds);
				}
			}
			if (PendingOrderTimeLabels.IsValidIndex(Index) &&
				PendingOrderTimeLabels[Index])
			{
				PendingOrderTimeLabels[Index]->SetText(
					FText::FromString(FString::Printf(
						TEXT("x%d  -  arrivée dans %.1f s"),
						Order.Quantity,
						Remaining)));
			}
			if (PendingOrderProgressBars.IsValidIndex(Index) &&
				PendingOrderProgressBars[Index])
			{
				PendingOrderProgressBars[Index]->SetPercent(FMath::Clamp(
					1.0f - Remaining / TotalDuration, 0.03f, 1.0f));
			}
		}
		return;
	}
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
			Button->SetBackgroundColor(FLinearColor::White);
			Button->SetColorAndOpacity(
				Index == static_cast<int32>(ActiveTab)
					? FLinearColor(1.25f, 1.16f, 0.76f, 1.0f)
					: FLinearColor(0.78f, 0.78f, 0.74f, 1.0f));
		}
	}
}

void UBotanicusOrderCatalogWidget::SyncCatalogScrollbar()
{
	if (!ItemsScroll || !CatalogScrollSlider || bSyncingCatalogScrollbar)
	{
		return;
	}
	const float EndOffset = ItemsScroll->GetScrollOffsetOfEnd();
	const float NormalizedOffset = EndOffset > KINDA_SMALL_NUMBER
		? FMath::Clamp(ItemsScroll->GetScrollOffset() / EndOffset, 0.0f, 1.0f)
		: 0.0f;
	bSyncingCatalogScrollbar = true;
	CatalogScrollSlider->SetValue(NormalizedOffset);
	CatalogScrollSlider->SetIsEnabled(EndOffset > KINDA_SMALL_NUMBER);
	bSyncingCatalogScrollbar = false;
}

void UBotanicusOrderCatalogWidget::HandleCatalogScrollChanged(float CurrentOffset)
{
	SyncCatalogScrollbar();
}

void UBotanicusOrderCatalogWidget::HandleCatalogSliderChanged(float Value)
{
	if (!ItemsScroll || bSyncingCatalogScrollbar)
	{
		return;
	}
	bSyncingCatalogScrollbar = true;
	ItemsScroll->SetScrollOffset(
		FMath::Clamp(Value, 0.0f, 1.0f) *
		ItemsScroll->GetScrollOffsetOfEnd());
	bSyncingCatalogScrollbar = false;
}

void UBotanicusOrderCatalogWidget::HandleCatalogScrollUpClicked()
{
	if (ItemsScroll)
	{
		ItemsScroll->SetScrollOffset(
			FMath::Max(0.0f, ItemsScroll->GetScrollOffset() - 160.0f));
	}
}

void UBotanicusOrderCatalogWidget::HandleCatalogScrollDownClicked()
{
	if (ItemsScroll)
	{
		ItemsScroll->SetScrollOffset(FMath::Min(
			ItemsScroll->GetScrollOffsetOfEnd(),
			ItemsScroll->GetScrollOffset() + 160.0f));
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

