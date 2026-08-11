// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusHudLayoutWidget.h"

#include "Components/NamedSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/BotanicusClockWidget.h"
#include "UI/BotanicusCrosshairWidget.h"
#include "UI/BotanicusHudMessageWidget.h"
#include "UI/BotanicusInteractionTargetWidget.h"
#include "UI/BotanicusQuickBarWidget.h"
#include "UI/BotanicusReputationWidget.h"
#include "UI/BotanicusSharedFundsWidget.h"
#include "UI/BotanicusShopObjectivesWidget.h"

template <typename WidgetType>
WidgetType* UBotanicusHudLayoutWidget::CreateElement(
	UNamedSlot* HostSlot,
	const TCHAR* BlueprintClassPath)
{
	if (!HostSlot)
	{
		return nullptr;
	}
	UClass* ElementClass = WidgetType::StaticClass();
	if (BlueprintClassPath)
	{
		if (UClass* BlueprintClass =
			LoadClass<WidgetType>(nullptr, BlueprintClassPath))
		{
			ElementClass = BlueprintClass;
		}
	}
	WidgetType* Element = CreateWidget<WidgetType>(
		GetOwningPlayer(), ElementClass);
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

	ClockWidget = CreateElement<UBotanicusClockWidget>(ClockSlot,
		TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Clock.WBP_HUD_Clock_C"));
	CreditsWidget = CreateElement<UBotanicusSharedFundsWidget>(CreditsSlot,
		TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Credits.WBP_HUD_Credits_C"));
	ReputationWidget =
		CreateElement<UBotanicusReputationWidget>(ReputationSlot,
			TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Reputation.WBP_HUD_Reputation_C"));
	ObjectivesWidget =
		CreateElement<UBotanicusShopObjectivesWidget>(ObjectivesSlot,
			TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Objectives.WBP_HUD_Objectives_C"));
	QuickBarWidget = CreateElement<UBotanicusQuickBarWidget>(QuickBarSlot,
		TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_QuickBar.WBP_HUD_QuickBar_C"));
	InteractionWidget =
		CreateElement<UBotanicusInteractionTargetWidget>(InteractionSlot,
			TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Interaction.WBP_HUD_Interaction_C"));
	if (InteractionWidget)
	{
		InteractionWidget->SetLayoutOwner(this);
	}
	MessageWidget = CreateElement<UBotanicusHudMessageWidget>(MessageSlot,
		TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Message.WBP_HUD_Message_C"));
	CrosshairWidget = CreateElement<UBotanicusCrosshairWidget>(CrosshairSlot,
		TEXT("/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Crosshair.WBP_HUD_Crosshair_C"));
}

void UBotanicusHudLayoutWidget::SetInteractionHeight(float NewHeight)
{
	if (UCanvasPanelSlot* InteractionCanvasSlot = InteractionSlot
		? Cast<UCanvasPanelSlot>(InteractionSlot->Slot)
		: nullptr)
	{
		FVector2D Size = InteractionCanvasSlot->GetSize();
		Size.Y = FMath::Max(108.0f, NewHeight);
		InteractionCanvasSlot->SetSize(Size);
	}
}
