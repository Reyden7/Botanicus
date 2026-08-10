// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusHudLayoutWidget.generated.h"

class UBotanicusClockWidget;
class UBotanicusHudMessageWidget;
class UBotanicusInteractionTargetWidget;
class UBotanicusQuickBarWidget;
class UBotanicusReputationWidget;
class UBotanicusSharedFundsWidget;
class UBotanicusShopObjectivesWidget;
class UNamedSlot;

/**
 * Blueprint-owned HUD canvas. WBP_BotanicusHUD supplies movable named slots;
 * this class injects the gameplay-aware native widgets into those slots.
 */
UCLASS()
class BOTANICUS_API UBotanicusHudLayoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBotanicusClockWidget* GetClockWidget() const { return ClockWidget; }
	UBotanicusSharedFundsWidget* GetCreditsWidget() const { return CreditsWidget; }
	UBotanicusReputationWidget* GetReputationWidget() const { return ReputationWidget; }
	UBotanicusShopObjectivesWidget* GetObjectivesWidget() const { return ObjectivesWidget; }
	UBotanicusQuickBarWidget* GetQuickBarWidget() const { return QuickBarWidget; }
	UBotanicusInteractionTargetWidget* GetInteractionWidget() const { return InteractionWidget; }
	UBotanicusHudMessageWidget* GetMessageWidget() const { return MessageWidget; }

protected:
	virtual void NativeOnInitialized() override;

private:
	template <typename WidgetType>
	WidgetType* CreateElement(UNamedSlot* HostSlot);

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> ClockSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> CreditsSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> ReputationSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> ObjectivesSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> QuickBarSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> InteractionSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> MessageSlot;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusClockWidget> ClockWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusSharedFundsWidget> CreditsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusReputationWidget> ReputationWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusShopObjectivesWidget> ObjectivesWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusQuickBarWidget> QuickBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusInteractionTargetWidget> InteractionWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusHudMessageWidget> MessageWidget;
};
