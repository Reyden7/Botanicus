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
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FTransform GetSalePotPreparationTransform(
		int32 SlotIndex = 0) const;
	bool FindClosestAvailableSalePotSlot(
		const FVector& ReferenceLocation,
		FTransform& OutTransform,
		const ABotanicusSalePotActor* IgnoredPot = nullptr) const;
	bool IsLocationOnPreparationSlot(
		const FVector& WorldLocation,
		float Tolerance = 45.0f) const;
	bool IsSalePotSlotAvailable(
		const ABotanicusSalePotActor* IgnoredPot = nullptr) const;
	int32 GetWorkbenchLevel() const { return WorkbenchLevel; }
	int32 GetSlotCount() const { return WorkbenchLevel; }
	int32 GetUpgradeCost() const;
	bool CanUpgrade() const { return WorkbenchLevel < 5; }
	bool UpgradeWorkbench();
	void RestoreWorkbenchLevel(int32 InLevel);

private:
	bool IsSlotOccupied(
		int32 SlotIndex,
		const ABotanicusSalePotActor* IgnoredPot) const;
	void RefreshLevelVisuals();

	UFUNCTION()
	void OnRep_WorkbenchLevel();

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Legs;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> SlotMarkers;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> PreparationLabel;

	/** Optional final meshes, indexed from level 1 to level 5. */
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Preparation")
	TArray<TObjectPtr<UStaticMesh>> LevelMeshes;

	UPROPERTY(ReplicatedUsing=OnRep_WorkbenchLevel)
	int32 WorkbenchLevel = 1;

	float SlotSpacing = 75.0f;
};
