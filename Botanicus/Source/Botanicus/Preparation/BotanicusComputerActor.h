// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusComputerActor.generated.h"

class UStaticMeshComponent;

/** Physical nursery computer used to access the command panel. */
UCLASS()
class BOTANICUS_API ABotanicusComputerActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusComputerActor();

	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ScreenFrame;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ScreenSurface;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Stand;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Keyboard;
};
