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

	/** Creates the designer-editable botanist notebook opened with I. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Botanicus|Editor")
	static bool RebuildBotanistNotebookWidget();

	/** Makes every emitter in NS_AnimeWater follow its owning Blueprint component. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Botanicus|Editor")
	static bool MakeAnimeWaterLocalSpace();
};
