// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusCrosshairWidget.generated.h"

class UImage;

/** Non-interactive aiming reticle displayed at the exact viewport centre. */
UCLASS()
class BOTANICUS_API UBotanicusCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> CrosshairImage;
};
