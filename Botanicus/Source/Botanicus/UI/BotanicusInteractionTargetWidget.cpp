// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusInteractionTargetWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"
#include "UI/BotanicusHudLayoutWidget.h"

void UBotanicusInteractionTargetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(310.0f);
		Size->SetHeightOverride(108.0f);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		Size->SetContent(Overlay);
		UImage* Background = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("ActionBackground"));
		Background->SetBrushFromTexture(BotanicusHudStyle::LoadTexture(
			TEXT("T_HUD_ActionBackground")), true);
		if (UOverlaySlot* BackgroundOverlaySlot =
				Overlay->AddChildToOverlay(Background))
		{
			BackgroundOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}

		UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
		Overlay->AddChildToOverlay(Content);

		UImage* FallbackKeyIcon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("KeyIcon"));
		FallbackKeyIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_KeyboardE.T_KeyboardE")),
			true);
		UCanvasPanelSlot* KeySlot = Content->AddChildToCanvas(FallbackKeyIcon);
		KeySlot->SetPosition(FVector2D(22.0f, 32.0f));
		KeySlot->SetSize(FVector2D(48.0f, 43.0f));

		ActionLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ActionLabel"));
		ActionLabel->SetText(FText::FromString(TEXT("INTERAGIR")));
		ActionLabel->SetColorAndOpacity(
			FSlateColor(BotanicusHudStyle::PrimaryText()));
		ActionLabel->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 14, TEXT("Bold")));
		UCanvasPanelSlot* ActionSlot = Content->AddChildToCanvas(ActionLabel);
		ActionSlot->SetPosition(FVector2D(84.0f, 28.0f));
		ActionSlot->SetSize(FVector2D(190.0f, 28.0f));

		TargetNameText = WidgetTree->ConstructWidget<UTextBlock>();
		TargetNameText->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.75f, 0.80f, 0.69f, 1.0f)));
		TargetNameText->SetFont(FSlateFontInfo(
			FCoreStyle::GetDefaultFont(), 12));
		TargetNameText->SetAutoWrapText(true);
		UCanvasPanelSlot* NameSlot = Content->AddChildToCanvas(TargetNameText);
		NameSlot->SetPosition(FVector2D(84.0f, 54.0f));
		NameSlot->SetSize(FVector2D(190.0f, 38.0f));
		WidgetTree->RootWidget = Size;
	}
	if (WidgetTree)
	{
		ActionBackground = Cast<UImage>(
			WidgetTree->FindWidget(TEXT("ActionBackground")));
		KeyIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("KeyIcon")));
		ActionLabel = Cast<UTextBlock>(
			WidgetTree->FindWidget(TEXT("ActionLabel")));
	}
	if (ActionBackground)
	{
		FSlateBrush Brush = ActionBackground->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Margin = FMargin(0.0f);
		ActionBackground->SetBrush(Brush);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusInteractionTargetWidget::SetLayoutOwner(
	UBotanicusHudLayoutWidget* InLayoutOwner)
{
	LayoutOwner = InLayoutOwner;
}

void UBotanicusInteractionTargetWidget::SetTargetName(const FText& TargetName)
{
	if (!TargetNameText || TargetName.IsEmpty())
	{
		ClearTarget();
		return;
	}
	TargetNameText->SetText(TargetName);
	if (ActionLabel)
	{
		ActionLabel->SetText(NSLOCTEXT(
			"BotanicusInteraction", "Interact", "INTERAGIR"));
	}
	if (KeyIcon)
	{
		KeyIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_KeyboardE.T_KeyboardE")),
			true);
	}
	UpdateAdaptiveHeight();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusInteractionTargetWidget::SetPlantInspectPrompt(
	const FText& PlantName)
{
	if (!TargetNameText || PlantName.IsEmpty())
	{
		ClearTarget();
		return;
	}
	TargetNameText->SetText(PlantName);
	if (ActionLabel)
	{
		ActionLabel->SetText(NSLOCTEXT(
			"BotanicusInteraction", "Inspect", "INSPECTER"));
	}
	if (KeyIcon)
	{
		KeyIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_MouseRightClick.T_MouseRightClick")),
			true);
	}
	UpdateAdaptiveHeight();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusInteractionTargetWidget::ClearTarget()
{
	if (TargetNameText)
	{
		TargetNameText->SetText(FText::GetEmpty());
	}
	ApplyPanelHeight(108.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusInteractionTargetWidget::UpdateAdaptiveHeight()
{
	if (!TargetNameText)
	{
		return;
	}
	TargetNameText->InvalidateLayoutAndVolatility();
	ForceLayoutPrepass();
	float AvailableTextWidth = 190.0f;
	if (const UCanvasPanelSlot* TextSlot =
		Cast<UCanvasPanelSlot>(TargetNameText->Slot))
	{
		AvailableTextWidth = FMath::Max(1.0f, TextSlot->GetSize().X);
	}
	const FVector2D DesiredTextSize = TargetNameText->GetDesiredSize();
	const int32 EstimatedLineCount = FMath::Max(1,
		FMath::CeilToInt(DesiredTextSize.X / AvailableTextWidth));
	const float EstimatedTextHeight = EstimatedLineCount
		* static_cast<float>(TargetNameText->GetFont().Size + 5);
	const float DesiredTextHeight = FMath::Max3(
		38.0f, static_cast<float>(DesiredTextSize.Y), EstimatedTextHeight);
	ApplyPanelHeight(FMath::Clamp(
		70.0f + DesiredTextHeight, 108.0f, 220.0f));
}

void UBotanicusInteractionTargetWidget::ApplyPanelHeight(float NewHeight)
{
	NewHeight = FMath::Max(108.0f, NewHeight);
	if (USizeBox* RootSizeBox = WidgetTree
		? Cast<USizeBox>(WidgetTree->RootWidget)
		: nullptr)
	{
		RootSizeBox->SetHeightOverride(NewHeight);
	}
	if (ActionBackground)
	{
		if (UCanvasPanelSlot* BackgroundSlot =
			Cast<UCanvasPanelSlot>(ActionBackground->Slot))
		{
			FVector2D Size = BackgroundSlot->GetSize();
			Size.Y = NewHeight;
			BackgroundSlot->SetSize(Size);
		}
	}
	if (TargetNameText)
	{
		if (UCanvasPanelSlot* TextSlot =
			Cast<UCanvasPanelSlot>(TargetNameText->Slot))
		{
			FVector2D Size = TextSlot->GetSize();
			Size.Y = FMath::Max(38.0f, NewHeight - 58.0f);
			TextSlot->SetSize(Size);
		}
	}
	if (LayoutOwner)
	{
		LayoutOwner->SetInteractionHeight(NewHeight);
	}
}
