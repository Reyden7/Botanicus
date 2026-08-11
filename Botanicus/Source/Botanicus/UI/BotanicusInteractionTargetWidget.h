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
	void ClearTarget();
	void SetLayoutOwner(UBotanicusHudLayoutWidget* InLayoutOwner);

protected:
	virtual void NativeOnInitialized() override;

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
};
