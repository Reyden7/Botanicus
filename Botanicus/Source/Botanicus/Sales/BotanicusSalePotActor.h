// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusSalePotActor.generated.h"

class ABotanicusCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** Sale container that must be filled with compatible soil and a whole plant. */
UCLASS()
class BOTANICUS_API ABotanicusSalePotActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusSalePotActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	void BeginPrimaryUse(AActor* Interactor);
	void EndPrimaryUse(AActor* Interactor);
	void RestoreSalePotState(
		FName InSoilItemKey,
		FName InPlantItemKey);

	bool HasSoil() const { return !SoilItemKey.IsNone(); }
	bool IsReadyForSale() const { return !PlantItemKey.IsNone(); }
	FName GetSoilItemKey() const { return SoilItemKey; }
	FName GetPlantItemKey() const { return PlantItemKey; }

private:
	bool IsOnPreparationWorkbench() const;
	bool IsInteractorStillTargeting(AActor* Interactor) const;
	void UpdatePrimaryUse(float DeltaSeconds);
	void RefreshVisuals();
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_SalePotState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SoilVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PlantVisual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlantMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(ReplicatedUsing=OnRep_SalePotState)
	FName SoilItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_SalePotState)
	FName PlantItemKey = NAME_None;

	TWeakObjectPtr<ABotanicusCharacter> ActiveUser;
	float SoilFillProgress = 0.0f;
	float SoilFillDuration = 1.5f;
	FName PendingSoilItemKey = NAME_None;
};
