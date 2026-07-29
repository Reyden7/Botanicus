// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BotanicusInteractable.generated.h"

/**
 * Text and state displayed by the interaction UI for the currently focused actor.
 */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusInteractionPrompt
{
	GENERATED_BODY()

	/** Verb shown to the player, for example "Prendre" or "Arroser". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText ActionText = NSLOCTEXT("BotanicusInteraction", "DefaultAction", "Interagir");

	/** Optional name of the focused object, for example "Monstera". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText TargetName;

	/** False keeps the focus information available while disabling the action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	bool bCanInteract = true;
};

UINTERFACE(BlueprintType)
class UBotanicusInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract implemented by every actor that can be used by a Botanicus player.
 *
 * Interact is only executed by the authoritative server. Prompt queries can also
 * happen locally so the UI remains responsive.
 */
class BOTANICUS_API IBotanicusInteractable
{
	GENERATED_BODY()

public:
	/** Returns the text and availability that should be presented to this player. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Botanicus|Interaction")
	FBotanicusInteractionPrompt GetInteractionPrompt(AActor* Interactor) const;

	/** Final gameplay validation. This is called again by the server before Interact. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Botanicus|Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** Performs the interaction. This event is invoked on the authoritative server. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Botanicus|Interaction")
	void Interact(AActor* Interactor);
};
