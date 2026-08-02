// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusSharedFundsWidget.generated.h"

class UTextBlock;

/** Permanent top-right display for the nursery's shared wallet. */
UCLASS()
class BOTANICUS_API UBotanicusSharedFundsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RefreshFunds();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShopLevelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReputationLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrendsLabel;

	int32 LastDisplayedFunds = INDEX_NONE;
	int32 LastDisplayedShopLevel = INDEX_NONE;
	int32 LastDisplayedReputation = INDEX_NONE;
	int32 LastDisplayedSatisfaction = INDEX_NONE;
	int32 LastDisplayedReviews = INDEX_NONE;
	int32 LastDisplayedTrendSeconds = INDEX_NONE;
	FName LastDisplayedTrendColor = NAME_None;
	FName LastDisplayedTrendType = NAME_None;
	FName LastDisplayedTrendQuality = NAME_None;
};
