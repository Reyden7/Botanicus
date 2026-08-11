// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusPlantGrowthWidget.generated.h"

class UImage;
class UTextBlock;

/** Compact world-space plant name, water and growth readout. */
UCLASS()
class BOTANICUS_API UBotanicusPlantGrowthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Botanicus|Plant UI")
	void SetPlantState(FText PlantName, float Water, float Growth);

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

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlantNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> WaterIcon;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> GrowthIcon;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WaterPercentText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GrowthPercentText;

private:
	float WaterPercent = 0.0f;
	float GrowthPercent = 0.0f;
};
