// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusThrowPowerWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** HUD feedback displayed while charging a thrown hotbar item. */
UCLASS()
class BOTANICUS_API UBotanicusThrowPowerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetThrowPower(float NormalizedPower);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> PowerBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PercentageText;
};
