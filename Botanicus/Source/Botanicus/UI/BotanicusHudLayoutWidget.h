// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusHudLayoutWidget.generated.h"

class UBotanicusClockWidget;
class UBotanicusCrosshairWidget;
class UBotanicusHudMessageWidget;
class UBotanicusInteractionTargetWidget;
class UBotanicusOutdoorEnvironmentWidget;
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
	UBotanicusOutdoorEnvironmentWidget* GetOutdoorEnvironmentWidget() const
	{
		return OutdoorEnvironmentWidget;
	}
	UBotanicusSharedFundsWidget* GetCreditsWidget() const { return CreditsWidget; }
	UBotanicusReputationWidget* GetReputationWidget() const { return ReputationWidget; }
	UBotanicusShopObjectivesWidget* GetObjectivesWidget() const { return ObjectivesWidget; }
	UBotanicusQuickBarWidget* GetQuickBarWidget() const { return QuickBarWidget; }
	UBotanicusInteractionTargetWidget* GetInteractionWidget() const { return InteractionWidget; }
	UBotanicusHudMessageWidget* GetMessageWidget() const { return MessageWidget; }
	UBotanicusCrosshairWidget* GetCrosshairWidget() const { return CrosshairWidget; }
	void SetInteractionSize(const FVector2D& NewSize);

protected:
	virtual void NativeOnInitialized() override;

private:
	template <typename WidgetType>
	WidgetType* CreateElement(
		UNamedSlot* HostSlot,
		const TCHAR* BlueprintClassPath = nullptr);

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> ClockSlot;

	// Optional during migration of older WBP_BotanicusHUD assets; the editor
	// migration adds this named slot without rebuilding the user's layout.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNamedSlot> OutdoorEnvironmentSlot;

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

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNamedSlot> CrosshairSlot;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusClockWidget> ClockWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusOutdoorEnvironmentWidget> OutdoorEnvironmentWidget;

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

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusCrosshairWidget> CrosshairWidget;
};
