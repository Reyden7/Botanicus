// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusCommandWidgetBuilder.generated.h"

UCLASS()
class BOTANICUS_API UBotanicusCommandWidgetBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Botanicus|Editor")
	static bool RebuildCommandComputerWidget();
};
