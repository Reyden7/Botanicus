// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusInteractionTargetWidget.generated.h"

class UTextBlock;
class UImage;
class UCanvasPanel;
class UBotanicusHudLayoutWidget;

/** Shows the catalogue name of the actor currently outlined in blue. */
UCLASS()
class BOTANICUS_API UBotanicusInteractionTargetWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTargetName(const FText& TargetName);
	void SetPlantInspectPrompt(const FText& PlantName);
	void SetLeftMousePrompt(
		const FText& ActionText,
		const FText& TargetName,
		bool bRequiresHold);
	void SetHoldProgress(float InProgress);
	void ClearTarget();
	void SetLayoutOwner(UBotanicusHudLayoutWidget* InLayoutOwner);

protected:
	virtual void NativeOnInitialized() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	void UpdateAdaptiveHeight();
	void ApplyPanelHeight(float NewHeight);

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TargetNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionLabel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ActionBackground;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> KeyIcon;

	UPROPERTY(Transient)
	TObjectPtr<UBotanicusHudLayoutWidget> LayoutOwner;

	float HoldProgress = 0.0f;
	bool bShowHoldProgress = false;
};
