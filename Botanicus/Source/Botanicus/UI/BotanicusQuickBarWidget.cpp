// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusQuickBarWidget.h"

#include "BotanicusCharacter.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Input/Reply.h"
#include "ItemDataAsset.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"

namespace
{
	constexpr float SlotSize = 76.0f;
	const FLinearColor EmptySlotColor = FLinearColor::Transparent;
	const FLinearColor OccupiedSlotColor = FLinearColor::Transparent;
	const FLinearColor SelectedSlotColor = FLinearColor::Transparent;
	const FLinearColor PrimaryTextColor(0.93f, 0.96f, 0.91f, 1.0f);
	const FLinearColor SecondaryTextColor(1.0f, 0.80f, 0.15f, 1.0f);
}

void UBotanicusQuickBarSlotWidget::InitializeDragSlot(
	UBotanicusQuickBarWidget* InOwnerWidget,
	int32 InSlotIndex)
{
	OwnerWidget = InOwnerWidget;
	SlotIndex = InSlotIndex;
}

void UBotanicusQuickBarSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(0.0f));
		Background->SetBrushColor(FLinearColor::Transparent);

		UOverlay* Layers =
			WidgetTree->ConstructWidget<UOverlay>();
		FrameIcon = WidgetTree->ConstructWidget<UImage>();
		FrameIcon->SetBrushFromTexture(
			BotanicusHudStyle::LoadTexture(TEXT("T_HUD_SlotNormal")), true);
		if (UOverlaySlot* FrameSlot = Layers->AddChildToOverlay(FrameIcon))
		{
			FrameSlot->SetHorizontalAlignment(HAlign_Fill);
			FrameSlot->SetVerticalAlignment(VAlign_Fill);
		}
		BackgroundIcon =
			WidgetTree->ConstructWidget<UImage>();
		BackgroundIcon->SetColorAndOpacity(
			FLinearColor(1.0f, 1.0f, 1.0f, 0.82f));
		BackgroundIcon->SetVisibility(
			ESlateVisibility::Collapsed);
		UOverlaySlot* BackgroundIconSlot =
			Layers->AddChildToOverlay(BackgroundIcon);
		BackgroundIconSlot->SetPadding(FMargin(11.0f));
		BackgroundIconSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundIconSlot->SetVerticalAlignment(VAlign_Fill);

		ContentContainer =
			WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* ContentSlot =
			Layers->AddChildToOverlay(ContentContainer);
		ContentSlot->SetPadding(FMargin(6.0f, 4.0f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);

		Background->SetContent(Layers);
		WidgetTree->RootWidget = Background;
	}
}

FReply UBotanicusQuickBarSlotWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() ==
			EKeys::LeftMouseButton &&
		OwnerWidget &&
		OwnerWidget->CanDragSlot(SlotIndex))
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(
			InMouseEvent,
			this,
			EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(
		InGeometry,
		InMouseEvent);
}

void UBotanicusQuickBarSlotWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(
		InGeometry,
		InMouseEvent,
		OutOperation);
	if (!OwnerWidget ||
		!OwnerWidget->CanDragSlot(SlotIndex))
	{
		return;
	}

	UBotanicusQuickBarDragOperation* Operation =
		NewObject<UBotanicusQuickBarDragOperation>(this);
	Operation->SourceSlotIndex = SlotIndex;
	Operation->Pivot = EDragPivot::CenterCenter;

	UTextBlock* DragLabel = NewObject<UTextBlock>(Operation);
	DragLabel->SetText(
		FText::FromString(TEXT("DEPLACER")));
	DragLabel->SetColorAndOpacity(
		FLinearColor(0.25f, 0.8f, 1.0f, 1.0f));
	DragLabel->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(),
		14,
		TEXT("Bold")));
	Operation->DefaultDragVisual = DragLabel;
	OutOperation = Operation;
}

bool UBotanicusQuickBarSlotWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UBotanicusQuickBarDragOperation* Operation =
		Cast<UBotanicusQuickBarDragOperation>(InOperation);
	if (!OwnerWidget || !Operation ||
		!OwnerWidget->IsReorganizationModeActive())
	{
		return Super::NativeOnDrop(
			InGeometry,
			InDragDropEvent,
			InOperation);
	}

	OwnerWidget->HandleSlotDropped(
		Operation->SourceSlotIndex,
		SlotIndex);
	return true;
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

void UBotanicusQuickBarWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshWateringCanStatus();
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
	RowCanvasSlot->SetPosition(FVector2D::ZeroVector);
	RowCanvasSlot->SetAutoSize(true);

	ReorganizationHelpText =
		WidgetTree->ConstructWidget<UTextBlock>();
	ReorganizationHelpText->SetText(
		FText::FromString(
			TEXT(
				"ORGANISATION DE LA HOTBAR  |  GLISSEZ-DEPOSEZ LES OBJETS  |  TAB POUR FERMER")));
	ReorganizationHelpText->SetJustification(
		ETextJustify::Center);
	ReorganizationHelpText->SetColorAndOpacity(
		FLinearColor(0.3f, 0.82f, 1.0f, 1.0f));
	ReorganizationHelpText->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(),
		15,
		TEXT("Bold")));
	UCanvasPanelSlot* HelpCanvasSlot =
		RootCanvas->AddChildToCanvas(ReorganizationHelpText);
	HelpCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	HelpCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	HelpCanvasSlot->SetPosition(FVector2D(0.0f, -160.0f));
	HelpCanvasSlot->SetAutoSize(true);
	ReorganizationHelpText->SetVisibility(
		ESlateVisibility::Collapsed);

	USizeBox* WaterStatusSize =
		WidgetTree->ConstructWidget<USizeBox>();
	WaterStatusSize->SetWidthOverride(300.0f);
	WaterStatusSize->SetHeightOverride(52.0f);
	UCanvasPanelSlot* WaterStatusCanvasSlot =
		RootCanvas->AddChildToCanvas(WaterStatusSize);
	WaterStatusCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	WaterStatusCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	WaterStatusCanvasSlot->SetPosition(FVector2D(0.0f, -92.0f));
	WaterStatusCanvasSlot->SetAutoSize(true);

	WateringCanStatus =
		WidgetTree->ConstructWidget<UBorder>();
	WateringCanStatus->SetPadding(FMargin(12.0f, 6.0f));
	WateringCanStatus->SetBrushColor(
		FLinearColor(0.02f, 0.10f, 0.14f, 0.94f));
	WaterStatusSize->SetContent(WateringCanStatus);

	UVerticalBox* WaterStatusContent =
		WidgetTree->ConstructWidget<UVerticalBox>();
	WateringCanStatus->SetContent(WaterStatusContent);

	WateringCanText =
		WidgetTree->ConstructWidget<UTextBlock>();
	WateringCanText->SetJustification(ETextJustify::Center);
	WateringCanText->SetColorAndOpacity(
		FLinearColor(0.45f, 0.88f, 1.0f, 1.0f));
	WateringCanText->SetFont(FSlateFontInfo(
		FCoreStyle::GetDefaultFont(),
		15,
		TEXT("Bold")));
	UVerticalBoxSlot* WaterTextSlot =
		WaterStatusContent->AddChildToVerticalBox(
			WateringCanText);
	WaterTextSlot->SetHorizontalAlignment(HAlign_Fill);

	WateringCanProgress =
		WidgetTree->ConstructWidget<UProgressBar>();
	WateringCanProgress->SetFillColorAndOpacity(
		FLinearColor(0.08f, 0.60f, 1.0f, 1.0f));
	WateringCanProgress->SetPercent(1.0f);
	UVerticalBoxSlot* WaterProgressSlot =
		WaterStatusContent->AddChildToVerticalBox(
			WateringCanProgress);
	WaterProgressSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	WaterProgressSlot->SetHorizontalAlignment(HAlign_Fill);

	WateringCanStatus->SetVisibility(
		ESlateVisibility::Collapsed);

	SlotBackgrounds.Reserve(UBotanicusQuickBarComponent::SlotCount);
	ItemLabels.Reserve(UBotanicusQuickBarComponent::SlotCount);
	ItemIcons.Reserve(UBotanicusQuickBarComponent::SlotCount);
	FrameIcons.Reserve(UBotanicusQuickBarComponent::SlotCount);
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
		HorizontalSlot->SetPadding(FMargin(2.0f));

		UBotanicusQuickBarSlotWidget* SlotWidget =
			WidgetTree->ConstructWidget<
				UBotanicusQuickBarSlotWidget>();
		SlotWidget->InitializeDragSlot(this, SlotIndex);
		SlotSizeBox->SetContent(SlotWidget);
		UBorder* Background = SlotWidget->GetBackground();
		UVerticalBox* Content =
			SlotWidget->GetContentContainer();
		if (!Background || !Content)
		{
			continue;
		}
		Background->SetBrushColor(EmptySlotColor);

		UTextBlock* KeyLabel =
			WidgetTree->ConstructWidget<UTextBlock>();
		KeyLabel->SetText(FText::AsNumber(
			SlotIndex == UBotanicusQuickBarComponent::SlotCount - 1
				? 0
				: SlotIndex + 1));
		KeyLabel->SetColorAndOpacity(
			FLinearColor(0.88f, 0.87f, 0.70f, 1.0f));
		KeyLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
		KeyLabel->SetShadowColorAndOpacity(FLinearColor::Black);
		KeyLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			11,
			TEXT("Bold")));
		Content->AddChildToVerticalBox(KeyLabel);

		UImage* ItemIcon = SlotWidget->GetBackgroundIcon();
		UImage* FrameIcon = SlotWidget->GetFrameIcon();

		UTextBlock* ItemLabel =
			WidgetTree->ConstructWidget<UTextBlock>();
		ItemLabel->SetText(FText::FromString(TEXT("—")));
		ItemLabel->SetColorAndOpacity(PrimaryTextColor);
		ItemLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
		ItemLabel->SetShadowColorAndOpacity(FLinearColor::Black);
		ItemLabel->SetJustification(ETextJustify::Center);
		ItemLabel->SetAutoWrapText(true);
		ItemLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			9));
		UVerticalBoxSlot* ItemSlot =
			Content->AddChildToVerticalBox(ItemLabel);
		ItemSlot->SetSize(
			FSlateChildSize(ESlateSizeRule::Fill));
		ItemSlot->SetHorizontalAlignment(HAlign_Fill);
		ItemSlot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* QuantityLabel =
			WidgetTree->ConstructWidget<UTextBlock>();
		QuantityLabel->SetColorAndOpacity(
			FLinearColor(0.94f, 0.90f, 0.66f, 1.0f));
		QuantityLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
		QuantityLabel->SetShadowColorAndOpacity(FLinearColor::Black);
		QuantityLabel->SetJustification(ETextJustify::Right);
		QuantityLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(),
			11,
			TEXT("Bold")));
		Content->AddChildToVerticalBox(QuantityLabel);

		SlotBackgrounds.Add(Background);
		ItemLabels.Add(ItemLabel);
		ItemIcons.Add(ItemIcon);
		FrameIcons.Add(FrameIcon);
		QuantityLabels.Add(QuantityLabel);
	}

	Refresh();
}

void UBotanicusQuickBarWidget::SetReorganizationMode(
	bool bEnabled)
{
	bReorganizationMode = bEnabled;
	if (WidgetTree && WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget->SetVisibility(
			bEnabled
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::HitTestInvisible);
	}
	if (ReorganizationHelpText)
	{
		ReorganizationHelpText->SetVisibility(
			bEnabled
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	Refresh();
}

bool UBotanicusQuickBarWidget::CanDragSlot(
	int32 SlotIndex) const
{
	return bReorganizationMode &&
		QuickBar &&
		!QuickBar->GetSlot(SlotIndex).IsEmpty();
}

void UBotanicusQuickBarWidget::HandleSlotDropped(
	int32 SourceSlotIndex,
	int32 TargetSlotIndex)
{
	if (bReorganizationMode && QuickBar)
	{
		QuickBar->RequestSwapSlots(
			SourceSlotIndex,
			TargetSlotIndex);
	}
}

void UBotanicusQuickBarWidget::RefreshWateringCanStatus()
{
	if (!WateringCanStatus ||
		!WateringCanProgress ||
		!WateringCanText)
	{
		return;
	}

	const ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetOwningPlayerPawn());
	const ABotanicusWateringCanActor* WateringCan =
		BotanicusCharacter
			? BotanicusCharacter->GetHeldWateringCan()
			: nullptr;
	if (!IsValid(WateringCan))
	{
		WateringCanStatus->SetVisibility(
			ESlateVisibility::Collapsed);
		return;
	}

	const float WaterLevel =
		FMath::Clamp(WateringCan->GetWaterLevel(), 0.0f, 1.0f);
	WateringCanStatus->SetVisibility(
		ESlateVisibility::HitTestInvisible);
	WateringCanProgress->SetPercent(WaterLevel);
	WateringCanProgress->SetFillColorAndOpacity(
		WaterLevel > 0.2f
			? FLinearColor(0.08f, 0.60f, 1.0f, 1.0f)
			: FLinearColor(1.0f, 0.24f, 0.08f, 1.0f));
	WateringCanText->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("ARROSOIR  |  EAU : %d%%"),
				FMath::RoundToInt(WaterLevel * 100.0f))));
}

void UBotanicusQuickBarWidget::Refresh()
{
	if (!QuickBar ||
		SlotBackgrounds.Num() != UBotanicusQuickBarComponent::SlotCount ||
		ItemIcons.Num() != UBotanicusQuickBarComponent::SlotCount)
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
		UImage* ItemIcon = ItemIcons[SlotIndex];
		UImage* FrameIcon = FrameIcons.IsValidIndex(SlotIndex)
			? FrameIcons[SlotIndex]
			: nullptr;
		UTextBlock* QuantityLabel = QuantityLabels[SlotIndex];

		Background->SetBrushColor(
			bReorganizationMode
				? (InventorySlot.IsEmpty()
					? FLinearColor(0.03f, 0.08f, 0.10f, 0.95f)
					: FLinearColor(0.05f, 0.28f, 0.38f, 0.98f))
				: (SlotIndex == SelectedSlotIndex
				? SelectedSlotColor
				: (InventorySlot.IsEmpty()
					? EmptySlotColor
					: OccupiedSlotColor)));
		if (FrameIcon)
		{
			FrameIcon->SetBrushFromTexture(
				BotanicusHudStyle::LoadTexture(
					SlotIndex == SelectedSlotIndex
						? TEXT("T_HUD_SlotSelected")
						: TEXT("T_HUD_SlotNormal")),
				true);
		}

		if (InventorySlot.IsEmpty())
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
			ItemLabel->SetVisibility(
				ESlateVisibility::HitTestInvisible);
			ItemLabel->SetText(FText::FromString(TEXT("—")));
			QuantityLabel->SetText(FText::GetEmpty());
			continue;
		}

		FText DisplayName = FText::FromName(InventorySlot.ItemKey);
		UTexture2D* IconTexture = nullptr;
		if (UItemDataAsset* ItemData =
			QuickBar->GetItemDataForSlot(SlotIndex))
		{
			DisplayName = ItemData->GetItemName();
			IconTexture =
				ItemData->GetItemIcon().LoadSynchronous();
		}
		else
		{
			const UGameInstance* GameInstance = GetGameInstance();
			const UBotanicusItemCatalogSubsystem* Catalog =
				GameInstance
					? GameInstance->GetSubsystem<
						UBotanicusItemCatalogSubsystem>()
					: nullptr;
			const FBotanicusItemDefinition* Definition =
				Catalog
					? Catalog->FindItem(InventorySlot.ItemKey)
					: nullptr;
			if (Definition &&
				!Definition->DisplayName.IsEmpty())
			{
				DisplayName = Definition->DisplayName;
			}
		}

		ItemLabel->SetText(DisplayName);
		if (IconTexture)
		{
			ItemIcon->SetBrushFromTexture(IconTexture, true);
			ItemIcon->SetToolTipText(DisplayName);
			ItemIcon->SetVisibility(
				ESlateVisibility::HitTestInvisible);
			ItemLabel->SetVisibility(
				ESlateVisibility::Collapsed);
		}
		else
		{
			ItemIcon->SetVisibility(
				ESlateVisibility::Collapsed);
			ItemLabel->SetVisibility(
				ESlateVisibility::HitTestInvisible);
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
