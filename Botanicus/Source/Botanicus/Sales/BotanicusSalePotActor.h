// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusSalePotActor.generated.h"

class ABotanicusCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UChildActorComponent;

/** Sale container that must be filled with compatible soil and a whole plant. */
UCLASS()
class BOTANICUS_API ABotanicusSalePotActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusSalePotActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	void BeginPrimaryUse(AActor* Interactor);
	void EndPrimaryUse(AActor* Interactor);
	void RestoreSalePotState(
		FName InSoilItemKey,
		FName InPlantItemKey);

	/** True only when there is enough soil to receive a plant. */
	bool HasSoil() const { return IsSoilFull(); }
	bool IsReadyForSale() const { return !PlantItemKey.IsNone(); }
	FName GetSoilItemKey() const { return SoilItemKey; }
	FName GetPlantItemKey() const { return PlantItemKey; }
	float GetPreparationHeightAdjustment() const;

private:
	float GetSoilLevel() const;
	bool IsSoilFull() const;
	bool IsOnPreparationWorkbench() const;
	bool IsInteractorStillTargeting(AActor* Interactor) const;
	FVector2D GetConfiguredSoilHorizontalOffset() const;
	float GetConfiguredSoilMaximumHeight() const;
	FVector2D GetConfiguredSoilBottomRadii() const;
	FVector2D GetConfiguredSoilTopRadii() const;
	float GetConfiguredSoilVolumeHeight() const;
	bool GetConfiguredSquareSoilProfile() const;
	void UpdatePrimaryUse(float DeltaSeconds);
	void RefreshSoilVisual();
	void RefreshVisuals();
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_SalePotState();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SoilVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> SoilShapeVisual;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	FVector2D SoilHorizontalOffset = FVector2D::ZeroVector;

	/** Final height of the soil surface relative to the pot actor, in centimetres. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category="Botanicus|Appearance|Soil",
		meta=(
			AllowPrivateAccess="true",
			DisplayName="Soil Maximum Height",
			Units="cm"))
	float SoilSurfaceHeight = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	FVector2D SoilBottomRadii = FVector2D(8.5f, 8.5f);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	FVector2D SoilTopRadii = FVector2D(13.5f, 13.5f);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil", meta=(Units="cm", ClampMin="1.0"))
	float SoilVolumeHeight = 19.0f;

	/** Use a square soil surface for square preparation-pot meshes. */
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Appearance|Soil")
	bool bSquareSoilProfile = false;

	/** Per-mesh pivot correction when this pot is aligned to a workbench slot. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category="Botanicus|Appearance",
		meta=(
			AllowPrivateAccess="true",
			DisplayName="Workbench Height Adjustment",
			Units="cm"))
	float PreparationHeightAdjustment = 0.0f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PlantVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StemVisual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlantMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(ReplicatedUsing=OnRep_SalePotState)
	FName SoilItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_SalePotState)
	FName PlantItemKey = NAME_None;

	TWeakObjectPtr<ABotanicusCharacter> ActiveUser;
	enum class EPrimaryUseMode : uint8
	{
		None,
		FillSoil,
		RemoveSoil
	};
	EPrimaryUseMode PrimaryUseMode = EPrimaryUseMode::None;

	UPROPERTY(ReplicatedUsing=OnRep_SalePotState)
	float SoilFillProgress = 0.0f;
	float SoilFillDuration = 1.5f;
	float SoilRemovalDuration = 2.0f;
	FName PendingSoilItemKey = NAME_None;
};
