// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusSalesDisplayEmptyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

namespace
{
	void AddSalesDisplayImage(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const TCHAR* Name,
		const TCHAR* TexturePath,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(
			UImage::StaticClass(), FName(Name));
		Image->SetBrushFromTexture(
			LoadObject<UTexture2D>(nullptr, TexturePath), true);
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetZOrder(ZOrder);
	}
}

void UBotanicusSalesDisplayEmptyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(720.0f);
		SizeBox->SetHeightOverride(420.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		SizeBox->SetContent(Canvas);

		AddSalesDisplayImage(WidgetTree, Canvas, TEXT("LeftLeaves"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesEmpty_LeftLeaves.T_SalesEmpty_LeftLeaves"),
			FVector2D(150.0f, 88.0f), FVector2D(180.0f, 120.0f), 1);
		AddSalesDisplayImage(WidgetTree, Canvas, TEXT("RightLeaves"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesEmpty_RightLeaves.T_SalesEmpty_RightLeaves"),
			FVector2D(390.0f, 88.0f), FVector2D(180.0f, 120.0f), 1);
		AddSalesDisplayImage(WidgetTree, Canvas, TEXT("PlantIcon"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesEmpty_Icon.T_SalesEmpty_Icon"),
			FVector2D(245.0f, -10.0f), FVector2D(230.0f, 154.0f), 3);
		AddSalesDisplayImage(WidgetTree, Canvas, TEXT("EmptyTitle"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesEmpty_Title.T_SalesEmpty_Title"),
			FVector2D(60.0f, 112.0f), FVector2D(600.0f, 200.0f), 2);
		AddSalesDisplayImage(WidgetTree, Canvas, TEXT("PlaceAction"),
			TEXT("/Game/Botanicus/UI/Sales/Textures/T_SalesEmpty_Action.T_SalesEmpty_Action"),
			FVector2D(100.0f, 222.0f), FVector2D(520.0f, 174.0f), 2);

		WidgetTree->RootWidget = SizeBox;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
