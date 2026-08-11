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
#include "Rendering/DrawElements.h"
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
	bShowHoldProgress = false;
	HoldProgress = 0.0f;
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
	bShowHoldProgress = false;
	HoldProgress = 0.0f;
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

void UBotanicusInteractionTargetWidget::SetLeftMousePrompt(
	const FText& InActionText,
	const FText& InTargetName,
	bool bRequiresHold)
{
	if (!TargetNameText || InActionText.IsEmpty())
	{
		ClearTarget();
		return;
	}
	TargetNameText->SetText(InTargetName);
	if (ActionLabel)
	{
		ActionLabel->SetText(InActionText);
	}
	if (KeyIcon)
	{
		KeyIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_MouseLeftClick.T_MouseLeftClick")),
			true);
	}
	bShowHoldProgress = bRequiresHold;
	if (!bShowHoldProgress)
	{
		HoldProgress = 0.0f;
	}
	UpdateAdaptiveHeight();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
}

void UBotanicusInteractionTargetWidget::SetHoldProgress(float InProgress)
{
	HoldProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	bShowHoldProgress = true;
	InvalidateLayoutAndVolatility();
}

void UBotanicusInteractionTargetWidget::ClearTarget()
{
	if (TargetNameText)
	{
		TargetNameText->SetText(FText::GetEmpty());
	}
	ApplyPanelHeight(108.0f);
	HoldProgress = 0.0f;
	bShowHoldProgress = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

int32 UBotanicusInteractionTargetWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 Result = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	if (!bShowHoldProgress || !KeyIcon)
	{
		return Result;
	}

	const FGeometry KeyGeometry = KeyIcon->GetCachedGeometry();
	const FVector2D Center = AllottedGeometry.AbsoluteToLocal(
		KeyGeometry.GetAbsolutePosition() + KeyGeometry.GetAbsoluteSize() * 0.5f);
	const float Radius = FMath::Max(
		25.0f, FMath::Max(KeyGeometry.GetLocalSize().X,
			KeyGeometry.GetLocalSize().Y) * 0.5f + 5.0f);
	constexpr int32 SegmentCount = 40;
	auto BuildArc = [Center, Radius](float Fraction, TArray<FVector2D>& Points)
	{
		const int32 VisibleSegments = FMath::Clamp(
			FMath::CeilToInt(SegmentCount * Fraction), 1, SegmentCount);
		Points.Reserve(VisibleSegments + 1);
		for (int32 Index = 0; Index <= VisibleSegments; ++Index)
		{
			const float Angle = -UE_HALF_PI + UE_TWO_PI *
				(static_cast<float>(Index) / SegmentCount);
			Points.Add(Center + FVector2D(FMath::Cos(Angle),
				FMath::Sin(Angle)) * Radius);
		}
	};

	TArray<FVector2D> BackgroundPoints;
	BuildArc(1.0f, BackgroundPoints);
	FSlateDrawElement::MakeLines(
		OutDrawElements, Result + 1, AllottedGeometry.ToPaintGeometry(),
		BackgroundPoints, ESlateDrawEffect::None,
		FLinearColor(0.04f, 0.05f, 0.03f, 0.85f), true, 6.0f);
	if (HoldProgress > KINDA_SMALL_NUMBER)
	{
		TArray<FVector2D> ProgressPoints;
		BuildArc(HoldProgress, ProgressPoints);
		FSlateDrawElement::MakeLines(
			OutDrawElements, Result + 2, AllottedGeometry.ToPaintGeometry(),
			ProgressPoints, ESlateDrawEffect::None,
			FLinearColor(0.62f, 0.95f, 0.20f, 1.0f), true, 6.0f);
	}
	return Result + 2;
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
