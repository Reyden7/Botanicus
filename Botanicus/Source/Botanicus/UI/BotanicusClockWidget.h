// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusClockWidget.generated.h"

class UTextBlock;

/** Top-left shared nursery clock and opening-hours indicator. */
UCLASS()
class BOTANICUS_API UBotanicusClockWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RefreshClock();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScheduleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FurnitureModeLabel;
};
