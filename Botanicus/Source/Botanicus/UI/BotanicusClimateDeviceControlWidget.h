// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusClimateDeviceControlWidget.generated.h"

class ABotanicusClimateDeviceActor;
class UButton;
class USlider;
class UTextBlock;

/** Editable WBP panel used to control one local climate device. */
UCLASS(Blueprintable)
class BOTANICUS_API UBotanicusClimateDeviceControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenForDevice(ABotanicusClimateDeviceActor* InDevice);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UTextBlock> DeviceNameText;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UTextBlock> PowerValueText;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<USlider> PowerSlider;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UButton> PowerToggleButton;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UTextBlock> PowerToggleText;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UButton> CloseButton;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePowerToggleClicked();

	UFUNCTION()
	void HandlePowerChanged(float NewValue);

	UFUNCTION()
	void HandleSliderCaptureBegin();

	UFUNCTION()
	void HandleSliderCaptureEnd();

	void RefreshFromDevice();
	void RefreshPowerText(float NormalizedPower);
	bool UsesCelsiusSlider() const;
	float ToSliderValue(float NormalizedPower) const;
	float ToNormalizedPower(float SliderValue) const;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusClimateDeviceActor> ControlledDevice;

	bool bSliderCaptured = false;
};
