// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusSalesDisplayEmptyWidget.generated.h"

/** World-space prompt shown while the player aims at an empty sales display. */
UCLASS()
class BOTANICUS_API UBotanicusSalesDisplayEmptyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
};
