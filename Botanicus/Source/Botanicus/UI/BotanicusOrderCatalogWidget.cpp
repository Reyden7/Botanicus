// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusOrderCatalogWidget.h"

#include "BotanicusPlayerController.h"
#include "Blueprint/WidgetTree.h"
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
	case EBotanicusItemWeightClass::OnePlayerCarry:
		return NSLOCTEXT("BotanicusOrders", "SoloWeight", "Portage solo");
	case EBotanicusItemWeightClass::TwoPlayerCarry:
		return NSLOCTEXT("BotanicusOrders", "CoopWeight", "Portage à deux");
	default:
		return FText::GetEmpty();
	}
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
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(820.0f, 650.0f));

	UVerticalBox* Column =
		WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	UHorizontalBox* Header =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(
		NSLOCTEXT("BotanicusOrders", "CatalogTitle", "CATALOGUE DE COMMANDES"));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetTextSize(Title, 27);
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	FundsLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FundsLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.96f, 0.78f, 0.25f, 1.0f)));
	FundsLabel->SetMargin(FMargin(10.0f));
	SetTextSize(FundsLabel, 20);
	Header->AddChildToHorizontalBox(FundsLabel);

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

	PendingOrdersLabel = WidgetTree->ConstructWidget<UTextBlock>();
	PendingOrdersLabel->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.66f, 0.82f, 0.68f, 1.0f)));
	PendingOrdersLabel->SetMargin(FMargin(0.0f, 12.0f, 0.0f, 10.0f));
	SetTextSize(PendingOrdersLabel, 14);
	Column->AddChildToVerticalBox(PendingOrdersLabel);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ItemsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(ItemsBox);
}

void UBotanicusOrderCatalogWidget::RebuildItemRows()
{
	if (!ItemsBox || !BotanicusController)
	{
		return;
	}

	ItemsBox->ClearChildren();
	ItemRows.Reset();
	const UGameInstance* GameInstance =
		BotanicusController->GetGameInstance();
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
		if (Definition.ItemKey.IsNone())
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
	if (FundsLabel)
	{
		FundsLabel->SetText(
			FText::Format(
				NSLOCTEXT("BotanicusOrders", "Funds", "{0} crédits"),
				FText::AsNumber(Funds)));
	}
	for (UBotanicusOrderItemRowWidget* Row : ItemRows)
	{
		if (Row)
		{
			Row->RefreshAvailability(Funds);
		}
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

void UBotanicusOrderCatalogWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleOrderCatalog();
	}
}
