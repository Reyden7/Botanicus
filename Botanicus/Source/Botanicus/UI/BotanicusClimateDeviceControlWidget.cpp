// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusClimateDeviceControlWidget.h"

#include "BotanicusPlayerController.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Environment/BotanicusClimateDeviceActor.h"
#include "InputCoreTypes.h"

void UBotanicusClimateDeviceControlWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(
			this, &ThisClass::HandleCloseClicked);
	}
	if (PowerToggleButton)
	{
		PowerToggleButton->OnClicked.AddUniqueDynamic(
			this, &ThisClass::HandlePowerToggleClicked);
	}
	if (PowerSlider)
	{
		PowerSlider->OnValueChanged.AddUniqueDynamic(
			this, &ThisClass::HandlePowerChanged);
		PowerSlider->OnMouseCaptureBegin.AddUniqueDynamic(
			this, &ThisClass::HandleSliderCaptureBegin);
		PowerSlider->OnMouseCaptureEnd.AddUniqueDynamic(
			this, &ThisClass::HandleSliderCaptureEnd);
	}
}

void UBotanicusClimateDeviceControlWidget::OpenForDevice(
	ABotanicusClimateDeviceActor* InDevice)
{
	ControlledDevice = InDevice;
	if (PowerSlider && IsValid(ControlledDevice))
	{
		const float MaximumPower = ControlledDevice->GetMaximumPowerLevel();
		if (UsesCelsiusSlider())
		{
			PowerSlider->SetMinValue(0.0f);
			PowerSlider->SetMaxValue(
				ControlledDevice->GetMaximumEffectStrength() * MaximumPower);
			PowerSlider->SetStepSize(0.5f);
		}
		else
		{
			PowerSlider->SetMinValue(0.0f);
			PowerSlider->SetMaxValue(MaximumPower);
			PowerSlider->SetStepSize(0.01f);
		}
	}
	RefreshFromDevice();
	SetKeyboardFocus();
}

void UBotanicusClimateDeviceControlWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsValid(ControlledDevice))
	{
		HandleCloseClicked();
		return;
	}
	RefreshFromDevice();
}

FReply UBotanicusClimateDeviceControlWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBotanicusClimateDeviceControlWidget::HandleCloseClicked()
{
	if (ABotanicusPlayerController* Controller =
		GetOwningPlayer<ABotanicusPlayerController>())
	{
		Controller->CloseClimateDeviceControl();
	}
}

void UBotanicusClimateDeviceControlWidget::HandlePowerToggleClicked()
{
	if (ABotanicusPlayerController* Controller =
		GetOwningPlayer<ABotanicusPlayerController>())
	{
		Controller->RequestToggleClimateDevice(ControlledDevice);
	}
}

void UBotanicusClimateDeviceControlWidget::HandlePowerChanged(float NewValue)
{
	if (!bSliderCaptured || !IsValid(ControlledDevice))
	{
		return;
	}
	const float NormalizedPower = ToNormalizedPower(NewValue);
	if (ABotanicusPlayerController* Controller =
		GetOwningPlayer<ABotanicusPlayerController>())
	{
		Controller->RequestSetClimateDevicePower(
			ControlledDevice, NormalizedPower);
	}
	RefreshPowerText(NormalizedPower);
}

void UBotanicusClimateDeviceControlWidget::HandleSliderCaptureBegin()
{
	bSliderCaptured = true;
}

void UBotanicusClimateDeviceControlWidget::HandleSliderCaptureEnd()
{
	bSliderCaptured = false;
	RefreshFromDevice();
}

void UBotanicusClimateDeviceControlWidget::RefreshFromDevice()
{
	if (!IsValid(ControlledDevice))
	{
		return;
	}
	const FBotanicusInteractionPrompt Prompt =
		ControlledDevice->GetInteractionPrompt_Implementation(
			GetOwningPlayerPawn());
	if (DeviceNameText)
	{
		DeviceNameText->SetText(Prompt.TargetName);
	}
	if (PowerToggleText)
	{
		PowerToggleText->SetText(
			ControlledDevice->IsClimateDeviceEnabled()
				? NSLOCTEXT("BotanicusClimate", "ControlTurnOff", "ÉTEINDRE")
				: NSLOCTEXT("BotanicusClimate", "ControlTurnOn", "ALLUMER"));
	}
	if (!bSliderCaptured && PowerSlider)
	{
		PowerSlider->SetValue(ToSliderValue(
			ControlledDevice->GetPowerLevel()));
	}
	if (!bSliderCaptured)
	{
		RefreshPowerText(ControlledDevice->GetPowerLevel());
	}
}

bool UBotanicusClimateDeviceControlWidget::UsesCelsiusSlider() const
{
	return IsValid(ControlledDevice) &&
		ControlledDevice->GetDeviceType() ==
			EBotanicusClimateDeviceType::Heater;
}

float UBotanicusClimateDeviceControlWidget::ToSliderValue(
	float NormalizedPower) const
{
	return UsesCelsiusSlider()
		? NormalizedPower * ControlledDevice->GetMaximumEffectStrength()
		: NormalizedPower;
}

float UBotanicusClimateDeviceControlWidget::ToNormalizedPower(
	float SliderValue) const
{
	if (!UsesCelsiusSlider())
	{
		return FMath::Clamp(
			SliderValue, 0.0f, ControlledDevice->GetMaximumPowerLevel());
	}
	return FMath::Clamp(
		SliderValue /
			FMath::Max(0.01f, ControlledDevice->GetMaximumEffectStrength()),
		0.0f,
		ControlledDevice->GetMaximumPowerLevel());
}

void UBotanicusClimateDeviceControlWidget::RefreshPowerText(
	float NormalizedPower)
{
	if (!PowerValueText || !IsValid(ControlledDevice))
	{
		return;
	}
	if (UsesCelsiusSlider())
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 1;
		Format.MaximumFractionalDigits = 1;
		PowerValueText->SetText(FText::Format(
			NSLOCTEXT(
				"BotanicusClimate",
				"HeatingPowerCelsius",
				"CHALEUR AJOUTÉE : +{0} °C"),
			FText::AsNumber(
				NormalizedPower *
					ControlledDevice->GetMaximumEffectStrength(),
				&Format)));
		return;
	}
	if (ControlledDevice->GetDeviceType() ==
		EBotanicusClimateDeviceType::Shade)
	{
		PowerValueText->SetText(FText::Format(
			NSLOCTEXT(
				"BotanicusClimate",
				"ShadePowerPercent",
				"RÉDUCTION DE LUMIÈRE : -{0}%"),
			FText::AsNumber(FMath::RoundToInt(
				NormalizedPower *
				ControlledDevice->GetMaximumEffectStrength()))));
		return;
	}
	if (ControlledDevice->GetDeviceType() ==
		EBotanicusClimateDeviceType::Dehumidifier)
	{
		PowerValueText->SetText(FText::Format(
			NSLOCTEXT(
				"BotanicusClimate",
				"DehumidifierPowerPercent",
				"RÉDUCTION D'HUMIDITÉ : -{0}%"),
			FText::AsNumber(FMath::RoundToInt(
				NormalizedPower *
				ControlledDevice->GetMaximumEffectStrength()))));
		return;
	}
	PowerValueText->SetText(FText::Format(
		NSLOCTEXT("BotanicusClimate", "PowerPercent", "PUISSANCE : {0}%"),
		FText::AsNumber(FMath::RoundToInt(NormalizedPower * 100.0f))));
}
