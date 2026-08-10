// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusReputationWidget.generated.h"

class UTextBlock;

/** Movable HUD element displaying the shop reputation. */
UCLASS()
class BOTANICUS_API UBotanicusReputationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RefreshReputation();

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReputationLabel;

	int32 LastDisplayedReputation = INDEX_NONE;
};
