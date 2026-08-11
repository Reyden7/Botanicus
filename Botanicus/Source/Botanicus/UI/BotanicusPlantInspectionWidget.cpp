// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusPlantInspectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
UImage* AddImage(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name,
	const TCHAR* Path, const FVector2D Position, const FVector2D Size,
	const int32 ZOrder = 0)
{
	UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, Path), true);
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(ZOrder);
	return Image;
}

UTextBlock* AddText(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name,
	const TCHAR* Preview, const FVector2D Position, const FVector2D Size,
	const int32 FontSize, const FLinearColor Color = FLinearColor(0.10f, 0.16f, 0.08f))
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), Name);
	Text->SetText(FText::FromString(Preview));
	Text->SetColorAndOpacity(Color);
	Text->SetJustification(ETextJustify::Center);
	Text->SetFont(FSlateFontInfo(
		LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
		FontSize, TEXT("Bold")));
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Text);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(2);
	return Text;
}
}

void UBotanicusPlantInspectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(420.0f);
		Root->SetHeightOverride(260.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetContent(Canvas);
		AddImage(WidgetTree, Canvas, TEXT("QualityBackground"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantInspect_QualityBackground.T_PlantInspect_QualityBackground"),
			FVector2D(10.0f, 4.0f), FVector2D(400.0f, 76.0f));
		AddImage(WidgetTree, Canvas, TEXT("PriceBackground"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantInspect_PriceBackground.T_PlantInspect_PriceBackground"),
			FVector2D(10.0f, 66.0f), FVector2D(400.0f, 76.0f));
		QualityText = AddText(WidgetTree, Canvas, TEXT("QualityText"),
			TEXT("BELLE"), FVector2D(54.0f, 27.0f),
			FVector2D(312.0f, 32.0f), 20);
		AddImage(WidgetTree, Canvas, TEXT("CreditIcon"),
			TEXT("/Game/Botanicus/UI/HUD/Textures/T_HUD_Credit.T_HUD_Credit"),
			FVector2D(104.0f, 80.0f), FVector2D(50.0f, 50.0f), 2);
		PriceText = AddText(WidgetTree, Canvas, TEXT("PriceText"),
			TEXT("120"), FVector2D(154.0f, 89.0f),
			FVector2D(162.0f, 32.0f), 20);
		ElementIcon = AddImage(WidgetTree, Canvas, TEXT("ElementIcon"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Normal.T_PlantElement_Normal"),
			FVector2D(170.0f, 139.0f), FVector2D(80.0f, 80.0f), 2);
		AgeText = AddText(WidgetTree, Canvas, TEXT("AgeText"),
			TEXT("02:15"), FVector2D(30.0f, 224.0f),
			FVector2D(360.0f, 28.0f), 14, FLinearColor::White);
		WidgetTree->RootWidget = Root;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusPlantInspectionWidget::SetInspectionData(
	const FText& Quality, const int32 Price, const FText& Element,
	UTexture2D* ElementTexture, const int32 WateringCount,
	const float AgeSeconds)
{
	if (QualityText)
	{
		QualityText->SetText(Quality);
	}
	if (PriceText)
	{
		PriceText->SetText(FText::AsNumber(Price));
	}
	if (ElementIcon && ElementTexture)
	{
		ElementIcon->SetBrushFromTexture(ElementTexture, true);
	}
	(void)Element;
	(void)WateringCount;
	if (AgeText)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(AgeSeconds));
		AgeText->SetText(FText::FromString(FString::Printf(
			TEXT("%02d:%02d"),
			TotalSeconds / 60, TotalSeconds % 60)));
	}
}
