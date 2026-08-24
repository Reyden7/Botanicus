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

	/** Wraps both existing note labels in vertically scrolling designer widgets. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Botanicus|Editor")
	static bool UpgradeBotanistNotebookNoteScrollBoxes();

	/** Converts notebook notes to rich text with data-driven bold emphasis. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Botanicus|Editor")
	static bool UpgradeBotanistNotebookRichNotes();

	/** Makes every emitter in NS_AnimeWater follow its owning Blueprint component. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Botanicus|Editor")
	static bool MakeAnimeWaterLocalSpace();
};
