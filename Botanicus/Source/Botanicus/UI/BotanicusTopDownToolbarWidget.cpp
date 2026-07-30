// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusTopDownToolbarWidget.h"

#include "BotanicusPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

namespace
{
UButton* AddToolbarButton(
	UWidgetTree* WidgetTree,
	UHorizontalBox* Row,
	const FText& Label,
	UTextBlock*& OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(FLinearColor(0.12f, 0.14f, 0.12f, 0.96f));

	OutLabel = WidgetTree->ConstructWidget<UTextBlock>();
	OutLabel->SetText(Label);
	OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	OutLabel->SetJustification(ETextJustify::Center);
	OutLabel->SetMargin(FMargin(18.0f, 10.0f));
	Button->AddChild(OutLabel);

	UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Button);
	Slot->SetPadding(FMargin(4.0f));
	Slot->SetVerticalAlignment(VAlign_Center);
	return Button;
}
}

void UBotanicusTopDownToolbarWidget::InitializeWithController(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	RefreshPathState(false, false, false);
}

void UBotanicusTopDownToolbarWidget::RefreshPathState(
	bool bPathModeActive,
	bool bCanConfirm,
	bool bPathDeletionActive)
{
	if (PathButton)
	{
		PathButton->SetBackgroundColor(
			bPathModeActive
				? FLinearColor(0.95f, 0.62f, 0.03f, 1.0f)
				: FLinearColor(0.12f, 0.14f, 0.12f, 0.96f));
	}
	if (PathButtonLabel)
	{
		PathButtonLabel->SetText(
			bPathModeActive
				? NSLOCTEXT("Botanicus", "PathModeActive", "CHEMIN ACTIF")
				: NSLOCTEXT("Botanicus", "PathModeInactive", "CHEMIN"));
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(bCanConfirm);
		ConfirmButton->SetVisibility(
			bPathModeActive
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (DeletePathButton)
	{
		DeletePathButton->SetBackgroundColor(
			bPathDeletionActive
				? FLinearColor(0.85f, 0.08f, 0.04f, 1.0f)
				: FLinearColor(0.12f, 0.14f, 0.12f, 0.96f));
	}
	if (DeletePathButtonLabel)
	{
		DeletePathButtonLabel->SetText(
			bPathDeletionActive
				? NSLOCTEXT(
					"Botanicus",
					"DeletePathModeActive",
					"SUPPRESSION ACTIVE")
				: NSLOCTEXT(
					"Botanicus",
					"DeletePathModeInactive",
					"SUPPRIMER ROUTE"));
	}
	if (CancelButton)
	{
		CancelButton->SetVisibility(
			bPathModeActive || bPathDeletionActive
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
}

void UBotanicusTopDownToolbarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UBotanicusTopDownToolbarWidget::BuildLayout()
{
	UCanvasPanel* Root =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("PlanningToolbarRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("PlanningToolbarPanel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.02f, 0.88f));
	Panel->SetPadding(FMargin(8.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	PanelSlot->SetPosition(FVector2D(0.0f, 28.0f));
	PanelSlot->SetAutoSize(true);

	UHorizontalBox* Row =
		WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("PlanningToolbarRow"));
	Panel->AddChild(Row);

	UTextBlock* PathLabel = nullptr;
	UTextBlock* ConfirmLabel = nullptr;
	UTextBlock* DeletePathLabel = nullptr;
	UTextBlock* PurchaseBuildingLabel = nullptr;
	UTextBlock* OrderDeliveryLabel = nullptr;
	UTextBlock* OrderLargeEquipmentLabel = nullptr;
	UTextBlock* OrderSoloEquipmentLabel = nullptr;
	UTextBlock* CancelLabel = nullptr;
	PurchaseBuildingButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT(
			"Botanicus",
			"PurchaseTestBuildingButton",
			"ACHETER BATIMENT TEST"),
		PurchaseBuildingLabel);
	OrderDeliveryButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT(
			"Botanicus",
			"OrderTestDeliveryButton",
			"COMMANDER COLIS TEST"),
		OrderDeliveryLabel);
	OrderLargeEquipmentButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT(
			"Botanicus",
			"OrderTestLargeEquipmentButton",
			"OBJET LOURD A DEUX"),
		OrderLargeEquipmentLabel);
	OrderSoloEquipmentButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT(
			"Botanicus",
			"OrderTestSoloEquipmentButton",
			"OBJET LOURD SOLO"),
		OrderSoloEquipmentLabel);
	PathButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT("Botanicus", "PathButton", "CHEMIN"),
		PathLabel);
	PathButtonLabel = PathLabel;
	ConfirmButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT("Botanicus", "ConfirmPathButton", "VALIDER"),
		ConfirmLabel);
	DeletePathButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT("Botanicus", "DeletePathButton", "SUPPRIMER ROUTE"),
		DeletePathLabel);
	DeletePathButtonLabel = DeletePathLabel;
	CancelButton = AddToolbarButton(
		WidgetTree,
		Row,
		NSLOCTEXT("Botanicus", "CancelPathButton", "ANNULER"),
		CancelLabel);

	PathButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandlePathClicked);
	PurchaseBuildingButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandlePurchaseBuildingClicked);
	OrderDeliveryButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleOrderDeliveryClicked);
	OrderLargeEquipmentButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleOrderLargeEquipmentClicked);
	OrderSoloEquipmentButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleOrderSoloEquipmentClicked);
	ConfirmButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleConfirmClicked);
	DeletePathButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleDeletePathClicked);
	CancelButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleCancelClicked);
	RefreshPathState(false, false, false);
}

void UBotanicusTopDownToolbarWidget::HandlePathClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginPathPlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleConfirmClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ConfirmPathPlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleDeletePathClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginPathDeletion();
	}
}

void UBotanicusTopDownToolbarWidget::HandlePurchaseBuildingClicked()
{
	if (BotanicusController)
	{
		BotanicusController->PurchaseTestBuilding();
	}
}

void UBotanicusTopDownToolbarWidget::HandleOrderDeliveryClicked()
{
	if (BotanicusController)
	{
		BotanicusController->OrderTestDelivery();
	}
}

void UBotanicusTopDownToolbarWidget::
	HandleOrderLargeEquipmentClicked()
{
	if (BotanicusController)
	{
		BotanicusController->OrderTestLargeEquipment();
	}
}

void UBotanicusTopDownToolbarWidget::
	HandleOrderSoloEquipmentClicked()
{
	if (BotanicusController)
	{
		BotanicusController->OrderTestSoloEquipment();
	}
}

void UBotanicusTopDownToolbarWidget::HandleCancelClicked()
{
	if (BotanicusController)
	{
		BotanicusController->CancelPathPlacement();
		BotanicusController->CancelPathDeletion();
	}
}
