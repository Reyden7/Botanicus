// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/BotanicusInteractable.h"
#include "BotanicusInteractableActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * Ready-to-use replicated base actor for simple interactable Blueprint objects.
 */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusInteractableActor : public AActor, public IBotanicusInteractable
{
	GENERATED_BODY()

public:
	ABotanicusInteractableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	/** Enables or disables this object's interaction state on the authoritative server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Botanicus|Interaction")
	void SetInteractionEnabled(bool bEnabled);

	/** Root used to attach additional Blueprint components. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Optional mesh with Visibility collision enabled by default. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText InteractionAction = NSLOCTEXT("BotanicusInteraction", "BaseActorAction", "Interagir");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText InteractionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_InteractionEnabled, Category="Interaction")
	bool bInteractionEnabled = true;

	/** Implement this event in a child Blueprint. It always runs on the server. */
	UFUNCTION(BlueprintImplementableEvent, Category="Botanicus|Interaction", meta=(DisplayName="On Interacted (Server)"))
	void ReceiveInteraction(AActor* Interactor);

	/** Lets a child Blueprint react visually when the replicated enabled state changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="Botanicus|Interaction", meta=(DisplayName="On Interaction Enabled Changed"))
	void ReceiveInteractionEnabledChanged(bool bEnabled);

private:
	UFUNCTION()
	void OnRep_InteractionEnabled();
};
