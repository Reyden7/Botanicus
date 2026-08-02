// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusVisitorSpeechBubbleWidget.generated.h"

class UTextBlock;

/** Small comic-style speech bubble displayed over an inspecting visitor. */
UCLASS()
class BOTANICUS_API UBotanicusVisitorSpeechBubbleWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSpeech(const FString& NewSpeech);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeechLabel;
};
