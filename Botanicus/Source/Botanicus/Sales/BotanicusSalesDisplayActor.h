// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusSalesDisplayActor.generated.h"

class UStaticMeshComponent;
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

private:
	FName GetSelectedItemKey(AActor* Interactor) const;
	const FBotanicusItemDefinition* GetSelectedPlantDefinition(
		AActor* Interactor) const;
	const FBotanicusItemDefinition* GetDisplayedPlantDefinition() const;
	bool IsLocalPlayerTargetingDisplay(AActor* Interactor) const;
	void RefreshVisuals();
	void RefreshLocalAction();
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_DisplayedPlant();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PlantVisual;

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
