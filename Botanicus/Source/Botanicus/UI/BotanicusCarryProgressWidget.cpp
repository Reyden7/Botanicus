// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusCarryProgressWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

void UBotanicusCarryProgressWidget::SetCarryProgress(float InProgress)
{
	CarryProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	InvalidateLayoutAndVolatility();
}

void UBotanicusCarryProgressWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(68.0f);
		SizeBox->SetHeightOverride(68.0f);
		WidgetTree->RootWidget = SizeBox;
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

int32 UBotanicusCarryProgressWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 Result = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const float Radius = FMath::Max(
		4.0f,
		FMath::Min(Size.X, Size.Y) * 0.5f - 6.0f);
	constexpr int32 SegmentCount = 40;

	auto BuildArc = [
		Center,
		Radius](
			float Fraction,
			TArray<FVector2D>& OutPoints)
		{
			const int32 VisibleSegments = FMath::Clamp(
				FMath::CeilToInt(SegmentCount * Fraction),
				1,
				SegmentCount);
			OutPoints.Reserve(VisibleSegments + 1);
			for (int32 Index = 0; Index <= VisibleSegments; ++Index)
			{
				const float Angle =
					-UE_HALF_PI +
					UE_TWO_PI *
						(static_cast<float>(Index) /
						 static_cast<float>(SegmentCount));
				OutPoints.Add(
					Center +
					FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) *
						Radius);
			}
		};

	TArray<FVector2D> BackgroundPoints;
	BuildArc(1.0f, BackgroundPoints);
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		Result + 1,
		AllottedGeometry.ToPaintGeometry(),
		BackgroundPoints,
		ESlateDrawEffect::None,
		FLinearColor(0.05f, 0.05f, 0.05f, 0.75f),
		true,
		7.0f);

	if (CarryProgress > KINDA_SMALL_NUMBER)
	{
		TArray<FVector2D> ProgressPoints;
		BuildArc(CarryProgress, ProgressPoints);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			Result + 2,
			AllottedGeometry.ToPaintGeometry(),
			ProgressPoints,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.72f, 0.05f, 1.0f),
			true,
			7.0f);
	}

	return Result + 2;
}
