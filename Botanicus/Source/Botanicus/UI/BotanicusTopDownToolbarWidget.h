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
		bool bPathDeletionActive);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandlePathClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleDeletePathClicked();

	UFUNCTION()
	void HandlePurchaseBuildingClicked();

	UFUNCTION()
	void HandleOrderDeliveryClicked();

	UFUNCTION()
	void HandleOrderLargeEquipmentClicked();

	UFUNCTION()
	void HandleOrderSoloEquipmentClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PathButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DeletePathButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PurchaseBuildingButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OrderDeliveryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OrderLargeEquipmentButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OrderSoloEquipmentButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PathButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeletePathButtonLabel;
};
