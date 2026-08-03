// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusDevelopmentPanelWidget.generated.h"

class ABotanicusPlayerController;
class UButton;
class UTextBlock;

/** Separate development-only command panel toggled with N. */
UCLASS()
class BOTANICUS_API UBotanicusDevelopmentPanelWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithController(
		ABotanicusPlayerController* InController);
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleAddCreditsClicked();

	UFUNCTION()
	void HandleTimeScaleClicked();

	UFUNCTION()
	void HandleDecreaseLevelClicked();

	UFUNCTION()
	void HandleIncreaseLevelClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeScaleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShopLevelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionFeedbackLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DecreaseLevelButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> IncreaseLevelButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AddCreditsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TimeScaleButton;
};
