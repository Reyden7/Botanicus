// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusOutdoorEnvironmentWidget.generated.h"

class UTextBlock;

/** HUD readout for the replicated outdoor seasonal environment. */
UCLASS()
class BOTANICUS_API UBotanicusOutdoorEnvironmentWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildFallbackLayout();
	void RefreshEnvironment();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TemperatureText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AirHumidityText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LuminosityText;

	float RefreshAccumulator = 0.0f;
};
