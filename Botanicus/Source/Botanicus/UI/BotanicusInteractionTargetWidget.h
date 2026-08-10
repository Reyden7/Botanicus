// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusInteractionTargetWidget.generated.h"

class UTextBlock;

/** Shows the catalogue name of the actor currently outlined in blue. */
UCLASS()
class BOTANICUS_API UBotanicusInteractionTargetWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTargetName(const FText& TargetName);
	void ClearTarget();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TargetNameText;
};
