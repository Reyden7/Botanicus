// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusTopDownToolbarWidget.generated.h"

class ABotanicusPlayerController;
class UButton;
class UTextBlock;

/** Temporary native planning toolbar, replaceable by the final UI design. */
UCLASS()
class BOTANICUS_API UBotanicusTopDownToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithController(
		ABotanicusPlayerController* InController);
	void RefreshPathState(
		bool bPathModeActive,
		bool bCanConfirm,
		bool bPathDeletionActive,
		bool bVisitorRouteMode,
		int32 ActiveVisitorZoneType,
		bool bDoorEditingActive);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandlePathClicked();

	UFUNCTION()
	void HandleVisitorRouteClicked();

	UFUNCTION()
	void HandleVisitorParkingClicked();

	UFUNCTION()
	void HandleVisitorSalesAreaClicked();

	UFUNCTION()
	void HandleVisitorCheckoutClicked();

	UFUNCTION()
	void HandleRefundZoneClicked();

	UFUNCTION()
	void HandleDeliveryZoneClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleDeletePathClicked();

	UFUNCTION()
	void HandleDoorEditingClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PathButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VisitorRouteButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VisitorParkingButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VisitorSalesAreaButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VisitorCheckoutButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefundZoneButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DeliveryZoneButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DeletePathButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DoorEditingButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PathButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeletePathButtonLabel;
};
