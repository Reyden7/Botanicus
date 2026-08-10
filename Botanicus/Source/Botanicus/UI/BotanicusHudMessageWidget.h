// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusHudMessageWidget.generated.h"

class UTextBlock;

/** Short, discreet gameplay feedback displayed above the quick bar. */
UCLASS()
class BOTANICUS_API UBotanicusHudMessageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowMessage(const FText& Message, float Lifetime = 3.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageLabel;

	double HideAtTime = 0.0;
};
