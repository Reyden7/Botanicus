// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusLargeEquipmentActor.generated.h"

class ABotanicusCharacter;
class UMaterialInterface;
class UMeshComponent;
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
	virtual void InitializeEquipment(FName InItemKey);
	/** Starts carrying through furniture mode without invoking the furniture's use action. */
	void BeginFurnitureMove(ABotanicusCharacter* Character);
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
	virtual FVector GetPlacementBoxExtent() const;

	virtual void BeginPlacement(ABotanicusCharacter* Character);
	virtual void UpdatePlacement(
		const FTransform& PlacementTransform,
		bool bIsValid);
	virtual void ConfirmPlacement();
	virtual void CancelPlacement();
	virtual void SetLocalPlacementPreview(
		const FTransform& PlacementTransform,
		bool bIsValid);

protected:
	virtual void OnEquipmentDefinitionApplied() {}
	void SetInteractionIndicatorVisibility(bool bVisible);
	bool IsUsingItemDataMesh() const
	{
		return bUsingItemDataMesh;
	}

private:
	void PickUp(ABotanicusCharacter* Character);
	void Drop();
	void ApplyCarryState();
	void ApplyPlacementMaterial(UMaterialInterface* Material);
	void RestorePlacementMaterials();

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> PreviewMaterialMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> PreviewOriginalMaterials;

	TArray<int32> PreviewMaterialCounts;

	bool bUsingItemDataMesh = false;
	FTransform PlacementOriginTransform = FTransform::Identity;
};
