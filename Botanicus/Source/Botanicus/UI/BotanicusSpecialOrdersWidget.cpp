#include "UI/BotanicusSpecialOrdersWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "SpecialOrders/BotanicusSpecialOrderComponent.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

void UBotanicusSpecialOrdersWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(370.0f);
	WidgetTree->RootWidget = Size;
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetPadding(FMargin(16.0f, 12.0f));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.08f, 0.055f, 0.94f));
	Size->SetContent(Panel);
	OrdersText = WidgetTree->ConstructWidget<UTextBlock>();
	OrdersText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
	OrdersText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.9f, 0.70f)));
	OrdersText->SetAutoWrapText(true);
	OrdersText->SetWrapTextAt(338.0f);
	Panel->SetContent(OrdersText);
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UBotanicusSpecialOrdersWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshOrders();
	// A timer also refreshes an empty/collapsed panel when the first order arrives.
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this,
		&UBotanicusSpecialOrdersWidget::RefreshOrders, 0.25f, true);
}

void UBotanicusSpecialOrdersWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

void UBotanicusSpecialOrdersWidget::RefreshOrders()
{
	const auto* Component = UBotanicusSpecialOrderComponent::Get(GetWorld());
	if (!Panel || !OrdersText) return;
	FString Text;
	bool bUrgent = false;
	if (Component)
	{
		for (const auto& Order : Component->GetOrders())
		{
			if (Order.Status == EBotanicusSpecialOrderStatus::Travelling || Order.Status == EBotanicusSpecialOrderStatus::Offered) continue;
			if (!Order.bWasAccepted && Order.ResultReputationDelta == 0) continue;
			if (!Text.IsEmpty()) Text += TEXT("\n\n");
			Text += FString::Printf(TEXT("COMMANDE #%d — %s\n"), Order.Number, *Order.Title.ToString());
			if (Order.Status == EBotanicusSpecialOrderStatus::Completed)
			{
				Text += FString::Printf(TEXT("Livrée ! +%d crédits · client satisfait"), Order.RewardCredits);
				continue;
			}
			if (Order.Status != EBotanicusSpecialOrderStatus::Active)
			{
				Text += Order.ResultReputationDelta < 0
					? FString::Printf(TEXT("Client parti mécontent · réputation %d"), Order.ResultReputationDelta)
					: TEXT("Client parti · commande terminée");
				continue;
			}
			const int32 Seconds = FMath::CeilToInt(Component->GetRemainingSeconds(Order));
			bUrgent |= Seconds <= 60;
			Text += FString::Printf(TEXT("%02d:%02d restantes · %d crédits\n"), Seconds / 60, Seconds % 60, Order.RewardCredits);
			for (const auto& R : Order.Requirements)
				Text += FString::Printf(TEXT("%s %d/%d  %s\n"), R.Delivered >= R.Quantity ? TEXT("✓") : TEXT("•"), R.Delivered, R.Quantity, *R.Description.ToString());
			Text += TEXT("Sélectionnez une plante, puis [E] auprès du client.");
		}
	}
	Panel->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (LastText != Text)
	{
		LastText = Text;
		OrdersText->SetText(FText::FromString(Text));
	}
	OrdersText->SetColorAndOpacity(FSlateColor(bUrgent
		? FLinearColor(1.0f, 0.57f, 0.30f) : FLinearColor(0.94f, 0.9f, 0.70f)));
}
