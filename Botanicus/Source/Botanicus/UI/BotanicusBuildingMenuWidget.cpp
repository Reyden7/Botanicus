// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusBuildingMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BotanicusPlayerController.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

void UBotanicusBuildingMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TopDownViewButton || !WidgetTree)
	{
		return;
	}

	ObservedBuildingMenu = WidgetTree->FindWidget(TEXT("BuildingMenu"));
	if (!ObservedBuildingMenu)
	{
		ObservedBuildingMenu = WidgetTree->FindWidget(TEXT("LegacyBuildingMenu"));
	}

	UCanvasPanel* TargetCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!TargetCanvas)
	{
		TArray<UWidget*> Widgets;
		WidgetTree->GetAllWidgets(Widgets);

		for (UWidget* Widget : Widgets)
		{
			if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Widget))
			{
				TargetCanvas = Canvas;
				break;
			}
		}
	}

	if (!TargetCanvas)
	{
		return;
	}

	TopDownViewButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("BotanicusTopDownViewButton"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BotanicusTopDownViewLabel"));

	if (!TopDownViewButton || !Label)
	{
		return;
	}

	TopDownViewButton->SetBackgroundColor(FLinearColor(1.0f, 0.65f, 0.0f, 1.0f));
	Label->SetText(FText::FromString(TEXT("VUE DU DESSUS")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Label->SetJustification(ETextJustify::Center);
	TopDownViewButton->AddChild(Label);
	TopDownViewButton->OnClicked.AddDynamic(
		this,
		&UBotanicusBuildingMenuWidget::HandleTopDownViewClicked);

	if (UCanvasPanelSlot* CanvasSlot = TargetCanvas->AddChildToCanvas(TopDownViewButton))
	{
		CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(-20.0f, 20.0f));
		CanvasSlot->SetSize(FVector2D(240.0f, 58.0f));
		CanvasSlot->SetZOrder(100);
	}
}

void UBotanicusBuildingMenuWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TopDownViewButton || !ObservedBuildingMenu)
	{
		return;
	}

	const ESlateVisibility MenuVisibility = ObservedBuildingMenu->GetVisibility();
	const bool bMenuIsVisible =
		MenuVisibility != ESlateVisibility::Collapsed &&
		MenuVisibility != ESlateVisibility::Hidden;

	TopDownViewButton->SetVisibility(
		bMenuIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UBotanicusBuildingMenuWidget::HandleTopDownViewClicked()
{
	if (ABotanicusPlayerController* Controller =
			Cast<ABotanicusPlayerController>(GetOwningPlayer()))
	{
		Controller->ToggleBuildingTopDownView();
	}
}
