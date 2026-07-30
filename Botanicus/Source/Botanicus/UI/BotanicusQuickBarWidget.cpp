// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusQuickBarWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "ItemDataAsset.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float SlotSize = 68.0f;
	const FLinearColor EmptySlotColor(0.015f, 0.02f, 0.015f, 0.82f);
	const FLinearColor OccupiedSlotColor(0.04f, 0.10f, 0.055f, 0.92f);
	const FLinearColor SelectedSlotColor(0.95f, 0.62f, 0.04f, 0.95f);
	const FLinearColor PrimaryTextColor(0.93f, 0.96f, 0.91f, 1.0f);
	const FLinearColor SecondaryTextColor(1.0f, 0.80f, 0.15f, 1.0f);
}

void UBotanicusQuickBarWidget::InitializeWithQuickBar(
	UBotanicusQuickBarComponent* InQuickBar)
{
	if (QuickBar == InQuickBar)
	{
		Refresh();
		return;
	}

	UnbindQuickBar();
	QuickBar = InQuickBar;

	if (QuickBar)
	{
		QuickBar->OnQuickBarChanged.AddDynamic(
			this,
			&UBotanicusQuickBarWidget::HandleQuickBarChanged);
		QuickBar->OnSelectedSlotChanged.AddDynamic(
			this,
			&UBotanicusQuickBarWidget::HandleSelectedSlotChanged);
	}

	Refresh();
}

void UBotanicusQuickBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildPrototypeLayout();
}

void UBotanicusQuickBarWidget::NativeDestruct()
{
	UnbindQuickBar();
	Super::NativeDestruct();
}

void UBotanicusQuickBarWidget::BuildPrototypeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("InventoryRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	UHorizontalBox* SlotRow =
		WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("SlotRow"));
	UCanvasPanelSlot* RowCanvasSlot =
		RootCanvas->AddChildToCanvas(SlotRow);
	RowCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	RowCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	// Kept above EBS' temporary three-tool strip during the prototype phase.
	RowCanvasSlot->SetPosition(FVector2D(0.0f, -112.0f));
	RowCanvasSlot->SetAutoSize(true);

	SlotBackgrounds.Reserve(UBotanicusQuickBarComponent::SlotCount);
	ItemLabels.Reserve(UBotanicusQuickBarComponent::SlotCount);
	QuantityLabels.Reserve(UBotanicusQuickBarComponent::SlotCount);

	for (int32 SlotIndex = 0;
		 SlotIndex < UBotanicusQuickBarComponent::SlotCount;
		 ++SlotIndex)
	{
		USizeBox* SlotSizeBox =
			WidgetTree->ConstructWidget<USizeBox>();
		SlotSizeBox->SetWidthOverride(SlotSize);
		SlotSizeBox->SetHeightOverride(SlotSize);
		UHorizontalBoxSlot* HorizontalSlot =
			SlotRow->AddChildToHorizontalBox(SlotSizeBox);
		HorizontalSlot->SetPadding(FMargin(3.0f));

		UBorder* Background =
			WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(6.0f, 4.0f));
		Background->SetBrushColor(EmptySlotColor);
		SlotSizeBox->SetContent(Background);

		UVerticalBox* Content =
			WidgetTree->ConstructWidget<UVerticalBox>();
		Background->SetContent(Content);

		UTextBlock* KeyLabel =
			WidgetTree->ConstructWidget<UTextBlock>();
		KeyLabel->SetText(FText::AsNumber(
			SlotIndex == UBotanicusQuickBarComponent::SlotCount - 1
				? 0
				: SlotIndex + 1));
		KeyLabel->SetColorAndOpacity(SecondaryTextColor);
		KeyLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			12,
			TEXT("Bold")));
		Content->AddChildToVerticalBox(KeyLabel);

		UTextBlock* ItemLabel =
			WidgetTree->ConstructWidget<UTextBlock>();
		ItemLabel->SetText(FText::FromString(TEXT("—")));
		ItemLabel->SetColorAndOpacity(PrimaryTextColor);
		ItemLabel->SetJustification(ETextJustify::Center);
		ItemLabel->SetAutoWrapText(true);
		ItemLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			9));
		UVerticalBoxSlot* ItemSlot =
			Content->AddChildToVerticalBox(ItemLabel);
		ItemSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ItemSlot->SetHorizontalAlignment(HAlign_Fill);
		ItemSlot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* QuantityLabel =
			WidgetTree->ConstructWidget<UTextBlock>();
		QuantityLabel->SetColorAndOpacity(SecondaryTextColor);
		QuantityLabel->SetJustification(ETextJustify::Right);
		QuantityLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			11,
			TEXT("Bold")));
		Content->AddChildToVerticalBox(QuantityLabel);

		SlotBackgrounds.Add(Background);
		ItemLabels.Add(ItemLabel);
		QuantityLabels.Add(QuantityLabel);
	}

	Refresh();
}

void UBotanicusQuickBarWidget::Refresh()
{
	if (!QuickBar ||
		SlotBackgrounds.Num() != UBotanicusQuickBarComponent::SlotCount)
	{
		return;
	}

	const int32 SelectedSlotIndex = QuickBar->GetSelectedSlotIndex();
	for (int32 SlotIndex = 0;
		 SlotIndex < UBotanicusQuickBarComponent::SlotCount;
		 ++SlotIndex)
	{
		const FBotanicusQuickBarSlot InventorySlot =
			QuickBar->GetSlot(SlotIndex);
		UBorder* Background = SlotBackgrounds[SlotIndex];
		UTextBlock* ItemLabel = ItemLabels[SlotIndex];
		UTextBlock* QuantityLabel = QuantityLabels[SlotIndex];

		Background->SetBrushColor(
			SlotIndex == SelectedSlotIndex
				? SelectedSlotColor
				: (InventorySlot.IsEmpty()
					? EmptySlotColor
					: OccupiedSlotColor));

		if (InventorySlot.IsEmpty())
		{
			ItemLabel->SetText(FText::FromString(TEXT("—")));
			QuantityLabel->SetText(FText::GetEmpty());
			continue;
		}

		if (UItemDataAsset* ItemData =
			QuickBar->GetItemDataForSlot(SlotIndex))
		{
			ItemLabel->SetText(ItemData->GetItemName());
		}
		else
		{
			ItemLabel->SetText(FText::FromName(InventorySlot.ItemKey));
		}

		QuantityLabel->SetText(FText::Format(
			NSLOCTEXT("Botanicus", "QuickBarQuantity", "x{0}"),
			FText::AsNumber(InventorySlot.Quantity)));
	}
}

void UBotanicusQuickBarWidget::UnbindQuickBar()
{
	if (!QuickBar)
	{
		return;
	}

	QuickBar->OnQuickBarChanged.RemoveDynamic(
		this,
		&UBotanicusQuickBarWidget::HandleQuickBarChanged);
	QuickBar->OnSelectedSlotChanged.RemoveDynamic(
		this,
		&UBotanicusQuickBarWidget::HandleSelectedSlotChanged);
	QuickBar = nullptr;
}

void UBotanicusQuickBarWidget::HandleQuickBarChanged()
{
	Refresh();
}

void UBotanicusQuickBarWidget::HandleSelectedSlotChanged(
	int32 SelectedSlotIndex,
	FBotanicusQuickBarSlot SelectedSlot)
{
	Refresh();
}
