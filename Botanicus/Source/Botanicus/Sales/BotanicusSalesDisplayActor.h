// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusSalesDisplayActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ABotanicusSalePotActor;
class ABotanicusPlayerController;
class ABotanicusVisitorCharacter;
struct FBotanicusItemDefinition;

/** Small nursery display that accepts one complete harvested plant. */
UCLASS()
class BOTANICUS_API ABotanicusSalesDisplayActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusSalesDisplayActor();

	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	/** Server-only placement of the selected complete plant. */
	void TryPlaceSelectedPlant(AActor* Interactor);
	bool TryMountSalePot(
		ABotanicusSalePotActor* SalePot,
		ABotanicusPlayerController* Seller);
	bool TryMountSalePotState(
		FName SoilItemKey,
		FName PlantItemKey,
		ABotanicusPlayerController* Seller,
		FName SalePotItemKey = TEXT("SalePot"));
	bool IsEmpty() const { return DisplayedPlantItemKey.IsNone(); }
	bool CanRetrieveDisplayedSalePot() const;
	bool IsDisplayedSalePotTargeted(const AActor* Interactor) const;
	void SetDisplayedSalePotHighlighted(
		bool bHighlighted,
		UMaterialInterface* HighlightMaterial);
	bool TryRetrieveDisplayedSalePot(AActor* Interactor);
	FTransform GetSalePotPlacementTransform() const;
	bool TryReserveForVisitor(
		ABotanicusVisitorCharacter* Visitor);
	bool IsAvailableForVisitorBrowsing(
		const ABotanicusVisitorCharacter* Visitor) const;
	bool TakeReservedPlantForVisitor(
		ABotanicusVisitorCharacter* Visitor);
	bool CompleteVisitorPurchase(
		ABotanicusVisitorCharacter* Visitor);
	void NotifyVisitorEnded(
		ABotanicusVisitorCharacter* Visitor);

	void RestoreDisplayedPlant(
		FName InDisplayedPlantItemKey,
		FName InDisplayedSoilItemKey = NAME_None,
		FName InDisplayedPotItemKey = TEXT("SalePot"));
	FName GetDisplayedPlantItemKey() const
	{
		return DisplayedPlantItemKey;
	}
	FName GetDisplayedSoilItemKey() const
	{
		return DisplayedSoilItemKey;
	}
	FName GetDisplayedPotItemKey() const
	{
		return DisplayedPotItemKey;
	}
	const FBotanicusItemDefinition*
		GetDisplayedPlantDefinitionForVisitor() const
	{
		return GetDisplayedPlantDefinition();
	}

	/** Horizontal position of the pot slot relative to the furniture centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|Slot",
		meta=(DisplayName="Position X Y du slot", Units="cm"))
	FVector2D SalePotSlotPosition = FVector2D::ZeroVector;

	/** Height of the pot slot relative to the measured top of the furniture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|Slot",
		meta=(DisplayName="Hauteur Z du slot", Units="cm",
			UIMin="-100.0", UIMax="100.0"))
	float SalePotSlotHeight = 0.0f;

private:
	FName GetSelectedItemKey(AActor* Interactor) const;
	const FBotanicusItemDefinition* GetSelectedPlantDefinition(
		AActor* Interactor) const;
	const FBotanicusItemDefinition* GetDisplayedPlantDefinition() const;
	FVector GetConfiguredDisplayedContentOffset() const;
	float GetDisplaySurfaceHeight() const;
	void RefreshSalePotSlotTransform();
	bool IsLocalPlayerTargetingDisplay(AActor* Interactor) const;
	void RefreshVisuals();
	void RefreshLocalAction();
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_DisplayedPlant();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SalePotSlot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> DisplayedContentRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PlantVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StemVisual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlantMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PotVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ContextActionText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> EmptyDisplayWidget;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> OccupiedDisplayWidget;

	/** Fine adjustment added to the measured top of the furniture mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|Appearance",
		meta=(DisplayName="Decalage vertical du pot sur le meuble", Units="cm",
			ClampMin="-30.0", ClampMax="30.0", UIMin="-10.0", UIMax="10.0",
			AllowPrivateAccess="true"))
	float DisplayedPotSurfaceOffset = 0.0f;

	/** Multiplies the mature stem height while preserving the species proportions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|Plant",
		meta=(DisplayName="Multiplicateur hauteur de la plante",
			ClampMin="0.25", ClampMax="3.0", UIMin="0.5", UIMax="2.0",
			AllowPrivateAccess="true"))
	float DisplayedPlantHeightMultiplier = 1.0f;

	/** Vertical position of the empty-display UI relative to the furniture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|UI",
		meta=(DisplayName="Hauteur UI presentoir vide", ClampMin="0.0",
			UIMin="0.0", UIMax="300.0", AllowPrivateAccess="true"))
	float EmptyDisplayWidgetHeight = 85.0f;

	/** Emissive tint multiplier used to keep the world-space UI vivid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|UI",
		meta=(DisplayName="Luminosite UI presentoir vide", ClampMin="0.1",
			ClampMax="5.0", UIMin="0.5", UIMax="3.0",
			AllowPrivateAccess="true"))
	float EmptyDisplayWidgetBrightness = 1.6f;

	/** Vertical position of the occupied plant information card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|UI",
		meta=(DisplayName="Hauteur UI plante en vente", ClampMin="0.0",
			UIMin="0.0", UIMax="300.0", AllowPrivateAccess="true"))
	float OccupiedDisplayWidgetHeight = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|UI",
		meta=(DisplayName="Echelle UI plante en vente", ClampMin="0.05",
			ClampMax="0.5", UIMin="0.1", UIMax="0.3",
			AllowPrivateAccess="true"))
	float OccupiedDisplayWidgetScale = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sales Display|UI",
		meta=(DisplayName="Luminosite UI plante en vente", ClampMin="0.1",
			ClampMax="5.0", UIMin="0.5", UIMax="3.0",
			AllowPrivateAccess="true"))
	float OccupiedDisplayWidgetBrightness = 1.6f;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayedPlant)
	FName DisplayedPlantItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayedPlant)
	FName DisplayedSoilItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayedPlant)
	FName DisplayedPotItemKey = TEXT("SalePot");

	UPROPERTY(ReplicatedUsing=OnRep_DisplayedPlant)
	float SaleEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayedPlant)
	bool bVisitorEnRoute = false;

	UPROPERTY(ReplicatedUsing=OnRep_DisplayedPlant)
	bool bPlantTakenByVisitor = false;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> SellerController;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusVisitorCharacter> ActiveVisitor;
};
