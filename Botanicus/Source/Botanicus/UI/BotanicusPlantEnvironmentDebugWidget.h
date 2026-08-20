// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "BotanicusPlantEnvironmentDebugWidget.generated.h"

class UTextBlock;

/** Editable world-space debug readout for a plant's three climate values. */
UCLASS()
class BOTANICUS_API UBotanicusPlantEnvironmentDebugWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Botanicus|Plant UI")
	void SetEnvironmentState(
		const FBotanicusPlantEnvironmentState& EnvironmentState);

protected:
	virtual void NativeOnInitialized() override;

private:
	void SetDiagnosticLine(
		UTextBlock* TextBlock,
		const FString& Value,
		EBotanicusPlantEnvironmentCondition Condition);

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TemperatureText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AirHumidityText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LuminosityText;
};
