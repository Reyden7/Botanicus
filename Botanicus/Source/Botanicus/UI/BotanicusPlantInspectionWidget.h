// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusPlantInspectionWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/** Expandable world-space details displayed beside a growing plant. */
UCLASS()
class BOTANICUS_API UBotanicusPlantInspectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetInspectionData(const FText& Quality, int32 Price,
		const FText& Element, UTexture2D* ElementTexture,
		int32 WateringCount, float AgeSeconds);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QualityText;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ElementIcon;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AgeText;
};
