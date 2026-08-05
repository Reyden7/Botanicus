// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "BotanicusPreparationWorkbenchActor.generated.h"

class ABotanicusSalePotActor;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Carryable preparation bench with one to five sale-pot snap points. */
UCLASS()
class BOTANICUS_API ABotanicusPreparationWorkbenchActor
	: public ABotanicusLargeEquipmentActor
{
	GENERATED_BODY()

public:
	ABotanicusPreparationWorkbenchActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void BeginPlacement(ABotanicusCharacter* Character) override;
	virtual void UpdatePlacement(
		const FTransform& PlacementTransform,
		bool bIsValid) override;
	virtual void ConfirmPlacement() override;
	virtual void CancelPlacement() override;
	virtual void SetLocalPlacementPreview(
		const FTransform& PlacementTransform,
		bool bIsValid) override;

	FTransform GetSalePotPreparationTransform(
		int32 SlotIndex = 0) const;
	bool FindClosestAvailableSalePotSlot(
		const FVector& ReferenceLocation,
		FTransform& OutTransform,
		const ABotanicusSalePotActor* IgnoredPot = nullptr) const;
	bool FindAimedAvailableSalePotSlot(
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		FTransform& OutTransform,
		int32& OutSlotIndex,
		const ABotanicusSalePotActor* IgnoredPot = nullptr) const;
	bool IsLocationOnPreparationSlot(
		const FVector& WorldLocation,
		float Tolerance = 45.0f) const;
	bool IsSalePotSlotAvailable(
		const ABotanicusSalePotActor* IgnoredPot = nullptr) const;
	int32 GetWorkbenchLevel() const { return WorkbenchLevel; }
	int32 GetSlotCount() const { return WorkbenchLevel; }
	int32 GetUpgradeCost() const;
	static int32 GetLevelUnlockCost(int32 Level);
	int32 GetLevelTransitionCost(int32 TargetLevel) const;
	bool CanUpgrade() const { return WorkbenchLevel < 5; }
	bool UpgradeWorkbench();
	bool CanChangeWorkbenchLevel(int32 TargetLevel) const;
	bool SetWorkbenchLevel(int32 TargetLevel);
	void RestoreWorkbenchLevel(int32 InLevel);
	bool HasPreparedPots() const;
	void GetPreparedPots(
		TArray<ABotanicusSalePotActor*>& OutPots) const;
	void SetMoveContentsWithFurniture(bool bEnabled);
	bool CanAccessUpgradeTerminal(const AActor* Interactor) const;
	bool IsUpgradeTerminalTargeted(const AActor* Interactor) const;

protected:
	virtual void OnEquipmentDefinitionApplied() override;

private:
	bool IsSlotOccupied(
		int32 SlotIndex,
		const ABotanicusSalePotActor* IgnoredPot) const;
	void RefreshLevelVisuals();
	void AttachPreparedPotsForMove();
	void DetachPreparedPotsAfterMove();
	void RefreshLocalPlacementPrompt();

	UFUNCTION()
	void OnRep_WorkbenchLevel();



	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> SlotMarkers;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> PreparationLabel;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> PlacementPrompt;

	/** Temporary interaction cube opening the native upgrade panel. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Botanicus|Preparation",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> UpgradeTerminal;

	/** Optional final meshes, indexed from level 1 to level 5. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Botanicus|Preparation",
		meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UStaticMesh>> LevelMeshes;

	UPROPERTY(ReplicatedUsing=OnRep_WorkbenchLevel)
	int32 WorkbenchLevel = 1;

	float SlotSpacing = 75.0f;
	TArray<TWeakObjectPtr<ABotanicusSalePotActor>> MovingPreparedPots;
	bool bMoveContentsWithFurniture = false;
};
