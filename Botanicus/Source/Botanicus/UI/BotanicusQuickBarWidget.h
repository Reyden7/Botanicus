// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "BotanicusQuickBarWidget.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UBotanicusQuickBarWidget;

UCLASS()
class BOTANICUS_API UBotanicusQuickBarDragOperation
	: public UDragDropOperation
{
	GENERATED_BODY()

public:
	int32 SourceSlotIndex = INDEX_NONE;
};

UCLASS()
class BOTANICUS_API UBotanicusQuickBarSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeDragSlot(
		UBotanicusQuickBarWidget* InOwnerWidget,
		int32 InSlotIndex);
	UBorder* GetBackground() const { return Background; }
	UImage* GetBackgroundIcon() const { return BackgroundIcon; }
	UVerticalBox* GetContentContainer() const
	{
		return ContentContainer;
	}

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBotanicusQuickBarWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BackgroundIcon;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentContainer;

	int32 SlotIndex = INDEX_NONE;
};

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
	void SetReorganizationMode(bool bEnabled);
	bool IsReorganizationModeActive() const
	{
		return bReorganizationMode;
	}
	bool CanDragSlot(int32 SlotIndex) const;
	void HandleSlotDropped(
		int32 SourceSlotIndex,
		int32 TargetSlotIndex);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildPrototypeLayout();
	void Refresh();
	void RefreshWateringCanStatus();
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
	TArray<TObjectPtr<UImage>> ItemIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> QuantityLabels;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WateringCanStatus;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> WateringCanProgress;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WateringCanText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReorganizationHelpText;

	bool bReorganizationMode = false;
};
