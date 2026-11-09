// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusPlantInspectionWidget.generated.h"

class UImage;
class UScaleBox;
class UTextBlock;
class UTexture2D;

/** Expandable world-space details displayed beside a growing plant. */
UCLASS()
class BOTANICUS_API UBotanicusPlantInspectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    void SetPlantName(const FText& Name);
    FVector2D GetContextPanelSize() const { return FVector2D(ContextPanelWidth,360); }
	void SetInspectionData(const FText& Quality, int32 Price,
		const FText& Element, UTexture2D* ElementTexture,
		int32 WateringCount, float AgeSeconds);

protected:
	virtual void NativeOnInitialized() override;

private:
    void ConfigureContextLayout();
    void ApplyPanelWidth(float Width);
    float ContextPanelWidth=360;
    UPROPERTY(Transient) TObjectPtr<UScaleBox> NameScaleBox;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ContextPlantName;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QualityText;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ElementIcon;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AgeText;
};
