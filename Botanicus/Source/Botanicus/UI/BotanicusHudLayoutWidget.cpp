// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusHudLayoutWidget.h"

#include "Components/NamedSlot.h"
#include "UI/BotanicusClockWidget.h"
#include "UI/BotanicusHudMessageWidget.h"
#include "UI/BotanicusInteractionTargetWidget.h"
#include "UI/BotanicusQuickBarWidget.h"
#include "UI/BotanicusReputationWidget.h"
#include "UI/BotanicusSharedFundsWidget.h"
#include "UI/BotanicusShopObjectivesWidget.h"

template <typename WidgetType>
WidgetType* UBotanicusHudLayoutWidget::CreateElement(UNamedSlot* HostSlot)
{
	if (!HostSlot)
	{
		return nullptr;
	}
	WidgetType* Element = CreateWidget<WidgetType>(
		GetOwningPlayer(), WidgetType::StaticClass());
	if (Element)
	{
		HostSlot->SetContent(Element);
	}
	return Element;
}

void UBotanicusHudLayoutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	ClockWidget = CreateElement<UBotanicusClockWidget>(ClockSlot);
	CreditsWidget = CreateElement<UBotanicusSharedFundsWidget>(CreditsSlot);
	ReputationWidget =
		CreateElement<UBotanicusReputationWidget>(ReputationSlot);
	ObjectivesWidget =
		CreateElement<UBotanicusShopObjectivesWidget>(ObjectivesSlot);
	QuickBarWidget = CreateElement<UBotanicusQuickBarWidget>(QuickBarSlot);
	InteractionWidget =
		CreateElement<UBotanicusInteractionTargetWidget>(InteractionSlot);
	MessageWidget = CreateElement<UBotanicusHudMessageWidget>(MessageSlot);
}
