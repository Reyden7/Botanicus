// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusComputerActor.generated.h"

class UCameraComponent;

/** Physical nursery computer used to access the command panel. */
UCLASS()
class BOTANICUS_API ABotanicusComputerActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusComputerActor();

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
	/** Camera used while smoothly approaching the command screen. Its transform
	 * can be adjusted directly on the inherited component in the computer BP. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category="Botanicus|Computer View")
	TObjectPtr<UCameraComponent> InteractionCamera;
};
