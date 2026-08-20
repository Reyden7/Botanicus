// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusPlantGrowthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"

namespace
{
UTextBlock* AddPlantText(UWidgetTree* Tree, UCanvasPanel* Canvas,
	const TCHAR* Name, const TCHAR* Preview, const FVector2D Position,
	const FVector2D Size, const int32 FontSize)
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(Name));
	Text->SetText(FText::FromString(Preview));
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FLinearColor::White);
	Text->SetFont(FSlateFontInfo(
		LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
		FontSize, TEXT("Bold")));
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Text);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(3);
	return Text;
}

UImage* AddPlantIcon(UWidgetTree* Tree, UCanvasPanel* Canvas,
	const TCHAR* Name, const TCHAR* TexturePath, const FVector2D Position)
{
	UImage* Image = Tree->ConstructWidget<UImage>(
		UImage::StaticClass(), FName(Name));
	Image->SetBrushFromTexture(
		LoadObject<UTexture2D>(nullptr, TexturePath), true);
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
	Slot->SetPosition(Position);
	Slot->SetSize(FVector2D(58.0f, 58.0f));
	Slot->SetZOrder(2);
	return Image;
}

void DrawRing(FSlateWindowElementList& Elements, const FGeometry& Geometry,
	const int32 Layer, const FVector2D Center, const float Radius,
	const float Percent, const FLinearColor Color)
{
	constexpr int32 Segments = 64;
	TArray<FVector2D> Background;
	Background.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = -HALF_PI + 2.0f * PI * Index / Segments;
		Background.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
		Background, ESlateDrawEffect::None,
		FLinearColor(0.24f, 0.26f, 0.25f, 0.95f), true, 8.0f);

	const int32 FilledSegments = FMath::Max(1,
		FMath::RoundToInt(Segments * FMath::Clamp(Percent, 0.0f, 1.0f)));
	TArray<FVector2D> Progress;
	Progress.Reserve(FilledSegments + 1);
	for (int32 Index = 0; Index <= FilledSegments; ++Index)
	{
		const float Angle = -HALF_PI + 2.0f * PI * Index / Segments;
		Progress.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	FSlateDrawElement::MakeLines(Elements, Layer + 1,
		Geometry.ToPaintGeometry(), Progress, ESlateDrawEffect::None,
		Color, true, 8.0f);
}
}

void UBotanicusPlantGrowthWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(360.0f);
		Root->SetHeightOverride(180.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetContent(Canvas);
		PlantNameText = AddPlantText(WidgetTree, Canvas, TEXT("PlantNameText"),
			TEXT("AURÉLIA DOUCE"), FVector2D(30.0f, 2.0f),
			FVector2D(300.0f, 34.0f), 21);
		WaterIcon = AddPlantIcon(WidgetTree, Canvas, TEXT("WaterIcon"),
			TEXT("/Game/Botanicus/UI/Plant/Textures/T_PlantUI_Water.T_PlantUI_Water"),
			FVector2D(97.0f, 52.0f));
		GrowthIcon = AddPlantIcon(WidgetTree, Canvas, TEXT("GrowthIcon"),
			TEXT("/Game/Botanicus/UI/Plant/Textures/T_PlantUI_Growth.T_PlantUI_Growth"),
			FVector2D(205.0f, 52.0f));
		WaterPercentText = AddPlantText(WidgetTree, Canvas,
			TEXT("WaterPercentText"), TEXT("46%"), FVector2D(83.0f, 124.0f),
			FVector2D(86.0f, 32.0f), 19);
		GrowthPercentText = AddPlantText(WidgetTree, Canvas,
			TEXT("GrowthPercentText"), TEXT("32%"), FVector2D(191.0f, 124.0f),
			FVector2D(86.0f, 32.0f), 19);
		WidgetTree->RootWidget = Root;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusPlantGrowthWidget::SetPlantState(
	FText PlantName, const float Water, const float Growth)
{
	WaterPercent = FMath::Clamp(Water, 0.0f, 1.0f);
	GrowthPercent = FMath::Clamp(Growth, 0.0f, 1.0f);
	if (PlantNameText)
	{
		PlantNameText->SetText(
			FText::FromString(PlantName.ToString().ToUpper()));
	}
	if (WaterPercentText)
	{
		WaterPercentText->SetText(FText::FromString(FString::Printf(
			TEXT("%d%%"), FMath::RoundToInt(WaterPercent * 100.0f))));
	}
	if (GrowthPercentText)
	{
		GrowthPercentText->SetText(FText::FromString(FString::Printf(
			TEXT("%d%%"), FMath::RoundToInt(GrowthPercent * 100.0f))));
	}
	InvalidateLayoutAndVolatility();
}

int32 UBotanicusPlantGrowthWidget::NativePaint(
	const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float ScaleX = Size.X / 360.0f;
	const float ScaleY = Size.Y / 180.0f;
	const float Radius = 40.0f * FMath::Min(ScaleX, ScaleY);
	DrawRing(OutDrawElements, AllottedGeometry, LayerId,
		FVector2D(126.0f * ScaleX, 81.0f * ScaleY), Radius,
		WaterPercent, FLinearColor(0.02f, 0.55f, 1.0f, 1.0f));
	DrawRing(OutDrawElements, AllottedGeometry, LayerId,
		FVector2D(234.0f * ScaleX, 81.0f * ScaleY), Radius,
		GrowthPercent, FLinearColor(0.35f, 0.86f, 0.12f, 1.0f));
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
}
