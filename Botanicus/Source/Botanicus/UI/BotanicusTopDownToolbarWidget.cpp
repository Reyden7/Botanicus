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
#include "Components/VerticalBox.h"

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
	RefreshPathState(false, false, false, false, INDEX_NONE);
}

void UBotanicusTopDownToolbarWidget::RefreshPathState(
	bool bPathModeActive,
	bool bCanConfirm,
	bool bPathDeletionActive,
	bool bVisitorRouteMode,
	int32 ActiveVisitorZoneType)
{
	if (PathButton)
	{
		PathButton->SetBackgroundColor(
			bPathModeActive && !bVisitorRouteMode
				? FLinearColor(0.95f, 0.62f, 0.03f, 1.0f)
				: FLinearColor(0.12f, 0.14f, 0.12f, 0.96f));
	}
	if (PathButtonLabel)
	{
		PathButtonLabel->SetText(
			bPathModeActive && !bVisitorRouteMode
				? NSLOCTEXT("Botanicus", "PathModeActive", "CHEMIN ACTIF")
				: NSLOCTEXT("Botanicus", "PathModeInactive", "CHEMIN"));
	}
	if (VisitorRouteButton)
	{
		VisitorRouteButton->SetBackgroundColor(
			bPathModeActive && bVisitorRouteMode
				? FLinearColor(0.03f, 0.78f, 0.75f, 1.0f)
				: FLinearColor(0.08f, 0.24f, 0.23f, 0.96f));
	}
	const auto RefreshZoneButton =
		[ActiveVisitorZoneType](
			UButton* Button,
			int32 ZoneType,
			const FLinearColor& ActiveColor)
		{
			if (Button)
			{
				Button->SetBackgroundColor(
					ActiveVisitorZoneType == ZoneType
						? ActiveColor
						: FLinearColor(0.08f, 0.24f, 0.23f, 0.96f));
			}
		};
	RefreshZoneButton(
		VisitorParkingButton,
		0,
		FLinearColor(0.08f, 0.35f, 0.85f, 1.0f));
	RefreshZoneButton(
		VisitorSalesAreaButton,
		1,
		FLinearColor(0.12f, 0.8f, 0.3f, 1.0f));
	RefreshZoneButton(
		VisitorCheckoutButton,
		2,
		FLinearColor(0.95f, 0.55f, 0.05f, 1.0f));
	RefreshZoneButton(
		RefundZoneButton,
		3,
		FLinearColor(0.95f, 0.25f, 0.03f, 1.0f));
	RefreshZoneButton(
		DeliveryZoneButton,
		4,
		FLinearColor(0.95f, 0.72f, 0.08f, 1.0f));
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
			bPathModeActive ||
				bPathDeletionActive ||
				ActiveVisitorZoneType != INDEX_NONE
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

	UVerticalBox* Rows =
		WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("PlanningToolbarRows"));
	Panel->AddChild(Rows);

	UHorizontalBox* Row =
		WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("PlanningToolbarRow"));
	Rows->AddChildToVerticalBox(Row);

	UHorizontalBox* VisitorRow =
		WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("VisitorPlanningToolbarRow"));
	Rows->AddChildToVerticalBox(VisitorRow);

	UTextBlock* PathLabel = nullptr;
	UTextBlock* ConfirmLabel = nullptr;
	UTextBlock* DeletePathLabel = nullptr;
	UTextBlock* CancelLabel = nullptr;
	UTextBlock* VisitorRouteLabel = nullptr;
	UTextBlock* VisitorParkingLabel = nullptr;
	UTextBlock* VisitorSalesAreaLabel = nullptr;
	UTextBlock* VisitorCheckoutLabel = nullptr;
	UTextBlock* RefundZoneLabel = nullptr;
	UTextBlock* DeliveryZoneLabel = nullptr;
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
	VisitorRouteButton = AddToolbarButton(
		WidgetTree,
		VisitorRow,
		NSLOCTEXT("Botanicus", "VisitorRouteButton", "ROUTE PNJ"),
		VisitorRouteLabel);
	VisitorParkingButton = AddToolbarButton(
		WidgetTree,
		VisitorRow,
		NSLOCTEXT("Botanicus", "VisitorParkingButton", "PARKING PNJ"),
		VisitorParkingLabel);
	VisitorSalesAreaButton = AddToolbarButton(
		WidgetTree,
		VisitorRow,
		NSLOCTEXT("Botanicus", "VisitorSalesAreaButton", "ZONE VENTE"),
		VisitorSalesAreaLabel);
	VisitorCheckoutButton = AddToolbarButton(
		WidgetTree,
		VisitorRow,
		NSLOCTEXT("Botanicus", "VisitorCheckoutButton", "CAISSE PNJ"),
		VisitorCheckoutLabel);
	RefundZoneButton = AddToolbarButton(
		WidgetTree,
		VisitorRow,
		NSLOCTEXT(
			"Botanicus",
			"RefundZoneButton",
			"REMBOURSEMENT OBJET"),
		RefundZoneLabel);
	DeliveryZoneButton = AddToolbarButton(
		WidgetTree,
		VisitorRow,
		NSLOCTEXT(
			"Botanicus",
			"DeliveryZoneButton",
			"ZONE LIVRAISON"),
		DeliveryZoneLabel);

	PathButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandlePathClicked);
	VisitorRouteButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleVisitorRouteClicked);
	VisitorParkingButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleVisitorParkingClicked);
	VisitorSalesAreaButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleVisitorSalesAreaClicked);
	VisitorCheckoutButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleVisitorCheckoutClicked);
	RefundZoneButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleRefundZoneClicked);
	DeliveryZoneButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleDeliveryZoneClicked);
	ConfirmButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleConfirmClicked);
	DeletePathButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleDeletePathClicked);
	CancelButton->OnClicked.AddDynamic(
		this,
		&UBotanicusTopDownToolbarWidget::HandleCancelClicked);
	RefreshPathState(false, false, false, false, INDEX_NONE);
}

void UBotanicusTopDownToolbarWidget::HandlePathClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginPathPlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleVisitorRouteClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginVisitorRoutePlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleVisitorParkingClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginVisitorParkingPlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleVisitorSalesAreaClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginVisitorSalesAreaPlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleVisitorCheckoutClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginVisitorCheckoutPlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleRefundZoneClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginRefundZonePlacement();
	}
}

void UBotanicusTopDownToolbarWidget::HandleDeliveryZoneClicked()
{
	if (BotanicusController)
	{
		BotanicusController->BeginDeliveryZonePlacement();
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

void UBotanicusTopDownToolbarWidget::HandleCancelClicked()
{
	if (BotanicusController)
	{
		BotanicusController->CancelPathPlacement();
		BotanicusController->CancelPathDeletion();
		BotanicusController->CancelVisitorZonePlacement();
	}
}
