// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusLargeEquipmentActor.generated.h"

class ABotanicusCharacter;
class UTextRenderComponent;

/** Prototype equipment that must be carried in the world instead of a hotbar. */
UCLASS()
class BOTANICUS_API ABotanicusLargeEquipmentActor
	: public ABotanicusInteractableActor
{
	GENERATED_BODY()

public:
	ABotanicusLargeEquipmentActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	ABotanicusCharacter* GetCarrier() const { return Carrier; }

private:
	void PickUp(ABotanicusCharacter* Character);
	void Drop();
	void ApplyCarryState();

	UFUNCTION()
	void OnRep_Carrier();

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionIndicator;
};
