// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusStorageQuantityWidget.generated.h"

class UTextBlock;
class UImage;

/** Compact prompt used to choose how many stacked items are stored or taken. */
UCLASS()
class BOTANICUS_API UBotanicusStorageQuantityWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetQuantitySelection(
		const FText& ItemName,
		int32 Quantity,
		int32 MaximumQuantity,
		bool bStoring);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> InputIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InstructionText;
};
