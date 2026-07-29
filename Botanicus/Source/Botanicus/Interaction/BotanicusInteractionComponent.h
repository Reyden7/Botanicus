// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/BotanicusInteractable.h"
#include "BotanicusInteractionComponent.generated.h"

class AController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FBotanicusFocusChangedSignature,
	AActor*, FocusedActor,
	FBotanicusInteractionPrompt, Prompt);

/**
 * Locally detects interactable actors and asks the server to execute interactions.
 *
 * The server never trusts the actor sent by the client: distance, line of sight and
 * the interactable's own CanInteract rule are all validated authoritatively.
 */
UCLASS(ClassGroup=(Botanicus), meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBotanicusInteractionComponent();

	/** Actor currently targeted by the owning local player. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Interaction")
	AActor* GetFocusedActor() const;

	/** Current prompt. bCanInteract is false when no valid actor is focused. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Interaction")
	FBotanicusInteractionPrompt GetCurrentPrompt() const;

	/** Attempts to use the focused actor. Safe to call from an input action or Blueprint. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Interaction")
	void TryInteract();

	/** Clears focus and immediately refreshes the interaction UI. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Interaction")
	void ClearFocus();

	/** Fired locally whenever the focused actor or its displayed prompt changes. */
	UPROPERTY(BlueprintAssignable, Category="Botanicus|Interaction")
	FBotanicusFocusChangedSignature OnFocusChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Maximum local and server-authoritative interaction range in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="50.0", UIMin="50.0"))
	float InteractionDistance = 300.0f;

	/** Collision channel used to find objects under the crosshair. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Draws the local trace to help level-design debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Debug")
	bool bDrawDebugTrace = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> FocusedActor;

	UPROPERTY(Transient)
	FBotanicusInteractionPrompt CurrentPrompt;

	void RefreshFocus();
	bool TraceForInteractable(AActor*& OutActor) const;
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	bool IsLocallyControlledOwner() const;
	void SetFocusedActor(AActor* NewFocusedActor);
	bool IsServerInteractionValid(AActor* TargetActor) const;

	UFUNCTION(Server, Reliable)
	void ServerTryInteract(AActor* TargetActor);
};
