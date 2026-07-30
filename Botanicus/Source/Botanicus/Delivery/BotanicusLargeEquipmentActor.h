// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusLargeEquipmentActor.generated.h"

class ABotanicusCharacter;
class UMaterialInterface;
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
	ABotanicusCharacter* GetHelper() const { return Helper; }
	FName GetItemKey() const { return ItemKey; }
	void InitializeEquipment(FName InItemKey);
	bool RequiresTwoPlayers() const { return bCooperativeCarry; }
	bool IsWaitingForHelper() const
	{
		return bCooperativeCarry &&
			Carrier != nullptr &&
			Helper == nullptr &&
			!bPlacementMode;
	}
	void BeginCooperativeHold(ABotanicusCharacter* Character);
	void EndCooperativeHold(ABotanicusCharacter* Character);
	bool IsInPlacementMode() const { return bPlacementMode; }
	bool IsPlacementValid() const { return bPlacementValid; }
	FVector GetPlacementBoxExtent() const;

	void BeginPlacement(ABotanicusCharacter* Character);
	void UpdatePlacement(
		const FTransform& PlacementTransform,
		bool bIsValid);
	void ConfirmPlacement();
	void CancelPlacement();
	void SetLocalPlacementPreview(
		const FTransform& PlacementTransform,
		bool bIsValid);

private:
	void PickUp(ABotanicusCharacter* Character);
	void Drop();
	void ApplyCarryState();

	UFUNCTION()
	void OnRep_Carrier();

	UFUNCTION()
	void OnRep_PlacementState();

	UFUNCTION()
	void OnRep_ItemKey();

	void ApplyItemDefinition();
	void ApplyCarrierMovementPenalty();
	void RestoreCarrierMovement();

	UPROPERTY(ReplicatedUsing=OnRep_ItemKey)
	FName ItemKey = TEXT("LargeEquipment_Test");

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Carrier;

	UPROPERTY(ReplicatedUsing=OnRep_Carrier)
	TObjectPtr<ABotanicusCharacter> Helper;

	UPROPERTY(ReplicatedUsing=OnRep_PlacementState)
	bool bCooperativeCarry = false;

	float CarryMovementSpeedMultiplier = 0.55f;

	UPROPERTY(ReplicatedUsing=OnRep_PlacementState)
	bool bPlacementMode = false;

	UPROPERTY(ReplicatedUsing=OnRep_PlacementState)
	bool bPlacementValid = false;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionIndicator;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial;

	FTransform PlacementOriginTransform = FTransform::Identity;
};
