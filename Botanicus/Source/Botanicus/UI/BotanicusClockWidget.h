// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusClockWidget.generated.h"

class UTextBlock;
class UBorder;

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
    void RefreshClock(float DeltaSeconds=0.f);
    void EnsureScheduleBackdrop();
    UPROPERTY(Transient) TObjectPtr<UBorder> ScheduleBackdrop;
    int32 LastStatus=-1;
    FLinearColor StatusTint=FLinearColor::Transparent;
    FLinearColor StatusFrom=FLinearColor::Transparent;
    FLinearColor StatusTarget=FLinearColor::Transparent;
    float StatusBlend=1.f;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ClockLabel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeLabel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ScheduleLabel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FurnitureModeLabel;
};
