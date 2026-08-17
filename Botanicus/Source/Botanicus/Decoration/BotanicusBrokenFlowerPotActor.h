// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusBrokenFlowerPotActor.generated.h"

class UStaticMesh;

/**
 * Small renewable scavenging prop. It can be collected like any hotbar item,
 * refunded for five credits, or broken by the player for a seed roll.
 */
UCLASS()
class BOTANICUS_API ABotanicusBrokenFlowerPotActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusBrokenFlowerPotActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Changes the visual variant on the authoritative server. */
	void SetPotVariant(int32 InVariant);
	int32 GetPotVariant() const { return PotVariant; }

protected:
	virtual void ApplyItemDefinition() override;

private:
	UFUNCTION()
	void OnRep_PotVariant();

	void ApplyPotVariant();

	UPROPERTY(ReplicatedUsing=OnRep_PotVariant)
	int32 PotVariant = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PotMeshOne;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PotMeshTwo;
};
