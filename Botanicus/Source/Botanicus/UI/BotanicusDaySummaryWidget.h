// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusDaySummaryWidget.generated.h"

class UTextBlock;

/** Shared end-of-day report displayed when the nursery closes. */
UCLASS()
class BOTANICUS_API UBotanicusDaySummaryWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RefreshSummary();

	UFUNCTION()
	void HandleContinueClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NextDayLabel;

	int32 LastSeenSummaryRevision = 0;
};
