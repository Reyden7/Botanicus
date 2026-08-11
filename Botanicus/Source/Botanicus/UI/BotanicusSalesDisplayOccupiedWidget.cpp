// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusSalesDisplayOccupiedWidget.h"

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
	const TCHAR* TexturePath, const FVector2D Position, const FVector2D Size,
	const int32 ZOrder = 0, const FLinearColor Tint = FLinearColor::White)
{
	UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, TexturePath), true);
	Image->SetColorAndOpacity(Tint);
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(ZOrder);
	return Image;
}

UTextBlock* AddText(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name,
	const TCHAR* Preview, const FVector2D Position, const FVector2D Size,
	const int32 FontSize, const int32 ZOrder = 3)
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetText(FText::FromString(Preview));
	Text->SetColorAndOpacity(FLinearColor(0.12f, 0.22f, 0.09f, 1.0f));
	Text->SetJustification(ETextJustify::Center);
	Text->SetFont(FSlateFontInfo(
		LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
		FontSize, TEXT("Bold")));
	Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Text->SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Text);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(ZOrder);
	return Text;
}
}

void UBotanicusSalesDisplayOccupiedWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(620.0f);
		Root->SetHeightOverride(220.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetContent(Canvas);

		// The source illustration includes generous transparent margins. This
		// crop keeps the ornament delicate instead of scaling it like a banner.
		AddImage(WidgetTree, Canvas, TEXT("PlantDecoration"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesOccupied_Decoration.T_SalesOccupied_Decoration"),
			FVector2D(70.0f, -105.0f), FVector2D(480.0f, 320.0f), 0);
		PlantNameText = AddText(WidgetTree, Canvas, TEXT("PlantNameText"),
			TEXT("ORCHIDEE ROSE"), FVector2D(45.0f, 62.0f),
			FVector2D(530.0f, 52.0f), 34);

		AddImage(WidgetTree, Canvas, TEXT("QualityBackground"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantInspect_QualityBackground.T_PlantInspect_QualityBackground"),
			FVector2D(42.0f, 103.0f), FVector2D(270.0f, 108.0f), 1);
		AddImage(WidgetTree, Canvas, TEXT("PriceBackground"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantInspect_PriceBackground.T_PlantInspect_PriceBackground"),
			FVector2D(308.0f, 103.0f), FVector2D(270.0f, 108.0f), 1);
		AddImage(WidgetTree, Canvas, TEXT("QualityIcon"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesOccupied_QualityStar.T_SalesOccupied_QualityStar"),
			FVector2D(67.0f, 125.0f), FVector2D(62.0f, 62.0f), 2);
		AddImage(WidgetTree, Canvas, TEXT("CreditIcon"),
			TEXT("/Game/Botanicus/UI/HUD/Textures/T_HUD_Credit.T_HUD_Credit"),
			FVector2D(330.0f, 129.0f), FVector2D(54.0f, 54.0f), 2);
		QualityText = AddText(WidgetTree, Canvas, TEXT("QualityText"),
			TEXT("BELLE"), FVector2D(126.0f, 137.0f),
			FVector2D(164.0f, 38.0f), 24);
		PriceText = AddText(WidgetTree, Canvas, TEXT("PriceText"),
			TEXT("85 credits"), FVector2D(382.0f, 137.0f),
			FVector2D(174.0f, 38.0f), 22);
		WidgetTree->RootWidget = Root;
	}
	if (WidgetTree)
	{
		PlantNameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PlantNameText")));
		QualityText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("QualityText")));
		PriceText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PriceText")));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusSalesDisplayOccupiedWidget::SetSaleData(
	const FText& PlantName, const FText& Quality, const int32 Price)
{
	if (PlantNameText)
	{
		PlantNameText->SetText(PlantName.ToUpper());
	}
	if (QualityText)
	{
		QualityText->SetText(Quality.ToUpper());
	}
	if (PriceText)
	{
		PriceText->SetText(FText::Format(
			NSLOCTEXT("BotanicusSales", "OccupiedPrice", "{0} credits"),
			FText::AsNumber(FMath::Max(0, Price))));
	}
}
