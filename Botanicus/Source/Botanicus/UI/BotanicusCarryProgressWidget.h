// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusCarryProgressWidget.generated.h"

/** Small native circular progress indicator used while holding E to lift. */
UCLASS()
class BOTANICUS_API UBotanicusCarryProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCarryProgress(float InProgress);
	float GetCarryProgress() const { return CarryProgress; }

protected:
	virtual void NativeOnInitialized() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	float CarryProgress = 0.0f;
};
