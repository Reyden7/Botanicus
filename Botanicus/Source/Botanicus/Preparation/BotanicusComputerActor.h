// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusComputerActor.generated.h"

class UCameraComponent;
class UBotanicusTerminalEyesComponent;

/** Level-authored nursery terminal used to access the command panel. */
UCLASS()
class BOTANICUS_API ABotanicusComputerActor
	: public ABotanicusInteractableActor
{
	GENERATED_BODY()

public:
	ABotanicusComputerActor();

	/** SKU used by saves made when the terminal was a movable item. */
	static FName LegacyItemKey();

	UFUNCTION(BlueprintPure, Category="Botanicus|Computer View")
	UCameraComponent* GetInteractionCamera() const
	{
		return InteractionCamera;
	}

	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;

	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual void Interact_Implementation(
		AActor* Interactor) override;

protected:
	virtual void PostInitializeComponents() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Eyes")
	TObjectPtr<UBotanicusTerminalEyesComponent> TerminalEyes;

	/** Camera used while smoothly approaching the command screen. Its transform
	 * can be adjusted directly on the inherited component in the computer BP. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category="Botanicus|Computer View")
	TObjectPtr<UCameraComponent> InteractionCamera;
};
