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
	EnsureHoldRing();
	RefreshHoldRing();
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
	// The target label is refreshed periodically. Preserve a hold animation
	// started by this widget's owning local controller while that happens.
	if (!bLocalHoldProgressActive)
	{
		bShowHoldProgress = false;
		HoldProgress = 0.0f;
	}
	if (ActionLabel)
	{
		ActionLabel->SetText(
			bLocalHoldProgressActive
				? NSLOCTEXT(
					"BotanicusInteraction",
					"PickUpHold",
					"PRENDRE")
				: NSLOCTEXT(
					"BotanicusInteraction",
					"Interact",
					"INTERAGIR"));
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

void UBotanicusInteractionTargetWidget::SetKeyboardPrompt(
	const FText& InActionText,
	const FText& InTargetName)
{
	if (!TargetNameText || InActionText.IsEmpty() || InTargetName.IsEmpty())
	{
		ClearTarget();
		return;
	}

	TargetNameText->SetText(InTargetName);
	if (!bLocalHoldProgressActive)
	{
		bShowHoldProgress = false;
		HoldProgress = 0.0f;
	}
	if (ActionLabel)
	{
		ActionLabel->SetText(InActionText);
	}
	if (KeyIcon)
	{
		KeyIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_KeyboardE.T_KeyboardE")),
			true);
	}
	UpdateAdaptiveHeight();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
	RefreshHoldRing();
}

void UBotanicusInteractionTargetWidget::BeginLocalHoldProgress(
	float DurationSeconds)
{
	if (bLocalHoldProgressActive)
	{
		// The server also confirms the hold to the owning client. Do not restart
		// an animation which was already started immediately from local input.
		LocalHoldDuration = FMath::Max(0.1f, DurationSeconds);
		bShowHoldProgress = true;
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	bLocalHoldProgressActive = true;
	bShowHoldProgress = true;
	LocalHoldElapsed = 0.0f;
	LocalHoldDuration = FMath::Max(0.1f, DurationSeconds);
	HoldProgress = 0.0f;
	if (ActionLabel)
	{
		ActionLabel->SetText(NSLOCTEXT(
			"BotanicusInteraction",
			"PickUpHold",
			"PRENDRE"));
	}
	if (KeyIcon)
	{
		KeyIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/PCKeyboardMouseIconPack/Textures/T_KeyboardE.T_KeyboardE")),
			true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
	RefreshHoldRing();
}

void UBotanicusInteractionTargetWidget::EndLocalHoldProgress()
{
	bLocalHoldProgressActive = false;
	bShowHoldProgress = false;
	LocalHoldElapsed = 0.0f;
	HoldProgress = 0.0f;
	InvalidateLayoutAndVolatility();
	RefreshHoldRing();
}

void UBotanicusInteractionTargetWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bLocalHoldProgressActive)
	{
		return;
	}

	LocalHoldElapsed += InDeltaTime;
	HoldProgress = FMath::Clamp(
		LocalHoldElapsed / LocalHoldDuration,
		0.0f,
		1.0f);
	bShowHoldProgress = true;
	InvalidateLayoutAndVolatility();
	RefreshHoldRing();
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
	if (bLocalHoldProgressActive)
	{
		// Picking up a planted pot with E temporarily takes priority over the
		// passive right-click inspection hint.
		UpdateAdaptiveHeight();
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
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
	if (bLocalHoldProgressActive)
	{
		UpdateAdaptiveHeight();
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
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
	RefreshHoldRing();
}

void UBotanicusInteractionTargetWidget::SetHoldProgress(float InProgress)
{
	HoldProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	bShowHoldProgress = true;
	InvalidateLayoutAndVolatility();
	RefreshHoldRing();
}

void UBotanicusInteractionTargetWidget::ClearTarget()
{
	// A short-lived focus miss must not cancel an input action which is still
	// being held by the owning player. Remote clients encounter these misses
	// more often while replicated actors update. The controller explicitly
	// ends the hold on release, cancellation or completion.
	if (bLocalHoldProgressActive)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	if (TargetNameText)
	{
		TargetNameText->SetText(FText::GetEmpty());
	}
	ApplyPanelSize(310.0f, 108.0f);
	bLocalHoldProgressActive = false;
	LocalHoldElapsed = 0.0f;
	HoldProgress = 0.0f;
	bShowHoldProgress = false;
	RefreshHoldRing();
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
	return Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
}

void UBotanicusInteractionTargetWidget::EnsureHoldRing()
{
	if (!WidgetTree || !KeyIcon || HoldRingSegments.Num() > 0)
	{
		return;
	}

	UCanvasPanel* ParentCanvas = Cast<UCanvasPanel>(KeyIcon->GetParent());
	UCanvasPanelSlot* KeySlot = Cast<UCanvasPanelSlot>(KeyIcon->Slot);
	if (!ParentCanvas || !KeySlot)
	{
		return;
	}

	constexpr int32 SegmentCount = 40;
	const FVector2D KeyPosition = KeySlot->GetPosition();
	const FVector2D KeySize = KeySlot->GetSize();
	const FVector2D Center = KeyPosition + KeySize * 0.5f;
	const float Radius = FMath::Max(
		25.0f,
		FMath::Max(KeySize.X, KeySize.Y) * 0.5f + 5.0f);
	const FAnchors KeyAnchors = KeySlot->GetAnchors();

	HoldRingSegments.Reserve(SegmentCount);
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float Angle = -UE_HALF_PI + UE_TWO_PI *
			(static_cast<float>(Index) / SegmentCount);
		UImage* Segment = WidgetTree->ConstructWidget<UImage>();
		Segment->SetBrush(
			*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
		Segment->SetVisibility(ESlateVisibility::Collapsed);
		Segment->SetRenderTransformAngle(FMath::RadiansToDegrees(Angle) + 90.0f);

		if (UCanvasPanelSlot* SegmentSlot =
				ParentCanvas->AddChildToCanvas(Segment))
		{
			SegmentSlot->SetAnchors(KeyAnchors);
			SegmentSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			SegmentSlot->SetPosition(
				Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) *
					Radius);
			SegmentSlot->SetSize(FVector2D(4.5f, 8.0f));
			SegmentSlot->SetZOrder(KeySlot->GetZOrder() + 2);
		}
		HoldRingSegments.Add(Segment);
	}
}

void UBotanicusInteractionTargetWidget::RefreshHoldRing()
{
	EnsureHoldRing();
	const int32 ActiveSegmentCount = FMath::Clamp(
		FMath::CeilToInt(HoldProgress * HoldRingSegments.Num()),
		0,
		HoldRingSegments.Num());
	for (int32 Index = 0; Index < HoldRingSegments.Num(); ++Index)
	{
		UImage* Segment = HoldRingSegments[Index];
		if (!Segment)
		{
			continue;
		}
		Segment->SetVisibility(
			bShowHoldProgress
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		Segment->SetColorAndOpacity(
			Index < ActiveSegmentCount
				? FLinearColor(0.62f, 0.95f, 0.20f, 1.0f)
				: FLinearColor(0.08f, 0.10f, 0.06f, 0.92f));
	}
}

void UBotanicusInteractionTargetWidget::UpdateAdaptiveHeight()
{
	if (!TargetNameText)
	{
		return;
	}
	constexpr float MinimumPanelWidth = 310.0f;
	constexpr float MaximumPanelWidth = 620.0f;
	constexpr float TextLeft = 84.0f;
	constexpr float TextRightPadding = 28.0f;

	if (ActionLabel)
	{
		// An action is a short command and must remain on one line. The panel
		// grows horizontally around it instead of letting it escape the artwork.
		ActionLabel->SetAutoWrapText(false);
		ActionLabel->InvalidateLayoutAndVolatility();
	}
	TargetNameText->InvalidateLayoutAndVolatility();
	ForceLayoutPrepass();

	const float DesiredActionWidth = ActionLabel
		? ActionLabel->GetDesiredSize().X
		: 190.0f;
	const float NewPanelWidth = FMath::Clamp(
		TextLeft + DesiredActionWidth + TextRightPadding,
		MinimumPanelWidth,
		MaximumPanelWidth);
	const float AvailableTextWidth = FMath::Max(
		1.0f, NewPanelWidth - TextLeft - TextRightPadding);

	if (UCanvasPanelSlot* ActionSlot = ActionLabel
		? Cast<UCanvasPanelSlot>(ActionLabel->Slot)
		: nullptr)
	{
		FVector2D Size = ActionSlot->GetSize();
		Size.X = AvailableTextWidth;
		ActionSlot->SetSize(Size);
	}
	if (UCanvasPanelSlot* TextSlot =
		Cast<UCanvasPanelSlot>(TargetNameText->Slot))
	{
		FVector2D Size = TextSlot->GetSize();
		Size.X = AvailableTextWidth;
		TextSlot->SetSize(Size);
	}

	const FVector2D DesiredTextSize = TargetNameText->GetDesiredSize();
	const int32 EstimatedLineCount = FMath::Max(1,
		FMath::CeilToInt(DesiredTextSize.X / AvailableTextWidth));
	const float EstimatedTextHeight = EstimatedLineCount
		* static_cast<float>(TargetNameText->GetFont().Size + 5);
	const float DesiredTextHeight = FMath::Max3(
		38.0f, static_cast<float>(DesiredTextSize.Y), EstimatedTextHeight);
	ApplyPanelSize(NewPanelWidth, FMath::Clamp(
		70.0f + DesiredTextHeight, 108.0f, 220.0f));
}

void UBotanicusInteractionTargetWidget::ApplyPanelSize(
	float NewWidth,
	float NewHeight)
{
	NewWidth = FMath::Max(310.0f, NewWidth);
	NewHeight = FMath::Max(108.0f, NewHeight);
	if (USizeBox* RootSizeBox = WidgetTree
		? Cast<USizeBox>(WidgetTree->RootWidget)
		: nullptr)
	{
		RootSizeBox->SetWidthOverride(NewWidth);
		RootSizeBox->SetHeightOverride(NewHeight);
	}
	if (ActionBackground)
	{
		if (UCanvasPanelSlot* BackgroundSlot =
			Cast<UCanvasPanelSlot>(ActionBackground->Slot))
		{
			FVector2D Size = BackgroundSlot->GetSize();
			Size.X = NewWidth;
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
			Size.X = FMath::Max(1.0f, NewWidth - 112.0f);
			Size.Y = FMath::Max(38.0f, NewHeight - 58.0f);
			TextSlot->SetSize(Size);
		}
	}
	if (LayoutOwner)
	{
		LayoutOwner->SetInteractionSize(FVector2D(NewWidth, NewHeight));
	}
}
