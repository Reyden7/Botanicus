// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusSharedFundsWidget.generated.h"

class UTextBlock;

/** Movable HUD element displaying the nursery's shared wallet. */
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

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FundsLabel;

	UPROPERTY(Transient)
	int32 LastDisplayedFunds = INDEX_NONE;
};
