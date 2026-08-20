// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BotanicusHudEditorLibrary.generated.h"

/** Editor automation used to build the editable master HUD Blueprint. */
UCLASS()
class BOTANICUS_API UBotanicusHudEditorLibrary
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Botanicus|HUD|Editor")
	static bool BuildEditableHudLayout(UObject* WidgetBlueprintAsset);

	UFUNCTION(BlueprintCallable, Category="Botanicus|HUD|Editor")
	static bool BuildEditableHudElement(
		UObject* WidgetBlueprintAsset,
		FName ElementType);

	UFUNCTION(BlueprintCallable, Category="Botanicus|HUD|Editor")
	static bool UpgradeInteractionElement(UObject* WidgetBlueprintAsset);

	UFUNCTION(BlueprintCallable, Category="Botanicus|HUD|Editor")
	static bool AddCrosshairToLayout(UObject* WidgetBlueprintAsset);

	UFUNCTION(BlueprintCallable, Category="Botanicus|HUD|Editor")
	static bool AddOutdoorEnvironmentToLayout(
		UObject* WidgetBlueprintAsset);
};
