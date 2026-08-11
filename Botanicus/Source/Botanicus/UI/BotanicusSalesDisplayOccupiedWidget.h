// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusSalesDisplayOccupiedWidget.generated.h"

class UTextBlock;

/** Compact world-space card shown above a plant offered for sale. */
UCLASS()
class BOTANICUS_API UBotanicusSalesDisplayOccupiedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSaleData(const FText& PlantName, const FText& Quality, int32 Price);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlantNameText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QualityText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PriceText;
};
