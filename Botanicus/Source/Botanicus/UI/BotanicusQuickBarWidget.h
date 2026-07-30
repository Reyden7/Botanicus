// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "BotanicusQuickBarWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * Neutral prototype hotbar generated entirely in C++.
 *
 * The gameplay data and events remain independent from this presentation, so a
 * designer-authored Widget Blueprint can replace it without changing inventory
 * or networking code.
 */
UCLASS()
class BOTANICUS_API UBotanicusQuickBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithQuickBar(UBotanicusQuickBarComponent* InQuickBar);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void BuildPrototypeLayout();
	void Refresh();
	void UnbindQuickBar();

	UFUNCTION()
	void HandleQuickBarChanged();

	UFUNCTION()
	void HandleSelectedSlotChanged(
		int32 SelectedSlotIndex,
		FBotanicusQuickBarSlot SelectedSlot);

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusQuickBarComponent> QuickBar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> SlotBackgrounds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ItemLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> QuantityLabels;
};
