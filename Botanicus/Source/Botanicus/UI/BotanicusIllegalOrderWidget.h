// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusIllegalOrderWidget.generated.h"

class UTextBlock;

/** Compact world-space order card for a clandestine customer. */
UCLASS(Blueprintable)
class BOTANICUS_API UBotanicusIllegalOrderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Botanicus|Illegal Trade")
	void SetOrderData(
		const FText& ProductName,
		int32 Quantity,
		int32 RewardCredits,
		float RemainingSeconds,
		bool bWaiting,
		bool bLeaving);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HeadingText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ProductText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RewardText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionText;
};
