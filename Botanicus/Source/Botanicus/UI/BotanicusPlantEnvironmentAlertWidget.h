// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "BotanicusPlantEnvironmentAlertWidget.generated.h"

class UImage;

/** World-space row of climate warnings displayed above a growing plant. */
UCLASS()
class BOTANICUS_API UBotanicusPlantEnvironmentAlertWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Botanicus|Plant UI")
	void SetEnvironmentState(
		const FBotanicusPlantEnvironmentState& EnvironmentState);

	UFUNCTION(BlueprintPure, Category="Botanicus|Plant UI")
	bool HasAnyAlert() const { return bHasAnyAlert; }

protected:
	virtual void NativeOnInitialized() override;

private:
	bool ApplyConditionIcon(
		UImage* Image,
		EBotanicusPlantEnvironmentCondition Condition,
		const TCHAR* LowTexturePath,
		const TCHAR* HighTexturePath);

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> TemperatureImage;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> AirHumidityImage;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> LuminosityImage;

	bool bHasAnyAlert = false;
};
