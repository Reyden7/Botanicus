// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/BotanicusInteractable.h"
#include "BotanicusVisitorCharacter.generated.h"

class ABotanicusSalesDisplayActor;
class ABotanicusCashRegisterActor;
class ABotanicusSelfCheckoutActor;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class USoundBase;

UENUM()
enum class EBotanicusVisitorState : uint8
{
	Queued,
	FollowingRoute,
	Approaching,
	Inspecting,
	CheckoutQueue,
	Paying,
	SelfCheckout,
	Leaving,
	SpecialOrderApproaching,
	SpecialOrderWaiting
};

/** First casual nursery visitor: arrives, inspects, buys, then leaves. */
UCLASS()
class BOTANICUS_API ABotanicusVisitorCharacter : public ACharacter, public IBotanicusInteractable
{
	GENERATED_BODY()

public:
	ABotanicusVisitorCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeCircuit(
		const TArray<FVector>& InRoutePoints,
		int32 InCheckoutWaypointIndex);
	void InitializeQueuedCircuit(
		const TArray<FVector>& InRoutePoints,
		int32 InCheckoutWaypointIndex,
		const FVector& InQueueDestination,
		const TArray<FVector>& InArrivalRoute,
		const TArray<FVector>& InDirectReturnRoute);
	void SetQueueDestination(const FVector& InQueueDestination);
	FVector GetVariedQueueDestination(
		const FVector& BaseDestination,
		const FVector& QueueDirection) const;
	void AdmitFromQueue();
	bool IsQueued() const
	{
		return VisitorState == EBotanicusVisitorState::Queued;
	}
	bool OccupiesShopCapacity() const
	{
		return VisitorState == EBotanicusVisitorState::FollowingRoute ||
			VisitorState == EBotanicusVisitorState::Approaching ||
			VisitorState == EBotanicusVisitorState::Inspecting ||
			VisitorState == EBotanicusVisitorState::CheckoutQueue ||
			VisitorState == EBotanicusVisitorState::Paying ||
			VisitorState == EBotanicusVisitorState::SelfCheckout ||
			VisitorState == EBotanicusVisitorState::SpecialOrderApproaching ||
			VisitorState == EBotanicusVisitorState::SpecialOrderWaiting;
	}
	bool IsInCheckoutQueue() const
	{
		return VisitorState == EBotanicusVisitorState::CheckoutQueue ||
			VisitorState == EBotanicusVisitorState::Paying;
	}
	float GetCheckoutQueueArrivalTime() const
	{
		return CheckoutQueueArrivalTime;
	}
	void ConfigureCheckoutQueue(
		const FVector& InQueueDestination,
		bool bIsFront);
	bool CanUsePlayerCheckout() const
	{
		return VisitorState == EBotanicusVisitorState::Paying &&
			bCarryingPlant;
	}
	void HandlePlayerCheckoutAction();
	bool AssignSelfCheckout(
		ABotanicusSelfCheckoutActor* InSelfCheckout);
	ABotanicusSelfCheckoutActor* GetAssignedSelfCheckout() const
	{
		return AssignedSelfCheckout;
	}
	bool IsWaitingForCheckoutAssignment() const
	{
		return VisitorState ==
				EBotanicusVisitorState::CheckoutQueue ||
			(VisitorState == EBotanicusVisitorState::Paying &&
			 CheckoutStage == 0);
	}
	int32 GetCheckoutPrice() const;
	bool IsCheckoutPlantScanned() const
	{
		return CheckoutStage > 0;
	}
	void BeginDeparture(bool bKeepPurchasedPlant = false);
	void BeginShopClosureDeparture();
	void BeginSpecialOrderVisit(FGuid Id, ABotanicusCashRegisterActor* Counter);
	void FinishSpecialOrderVisit(bool bSucceeded);
	void RefreshSpecialOrderSpeech();
	bool IsWaitingForSpecialOrder() const { return VisitorState == EBotanicusVisitorState::SpecialOrderWaiting; }
	ABotanicusCashRegisterActor* GetSpecialOrderCounter() const { return SpecialOrderCounter.Get(); }
	virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

private:
	friend class FBotanicusSpecialOrderLifecycleTest;
	void MoveTowards(
		const FVector& Destination,
		float DeltaSeconds);
	void BeginInspection();
	void FinishInspection();
	bool TryFindAvailableDisplay();
	int32 CountBrowsableDisplays() const;
	int32 ScoreDisplayForPreferences(
		const ABotanicusSalesDisplayActor* Display) const;
	void ResetBrowsingState();
	void RecordVisitOutcome(bool bPurchasedPlant);
	void SetSpeechLine(const FString& NewSpeech);
	FString ChooseInspectionSpeech(
		const ABotanicusSalesDisplayActor* Display) const;
	FString ChooseFinalRefusalSpeech() const;
	void RefreshCarriedPlantVisuals();
	bool IsInsideSalesArea() const;
	void FollowRoute(float DeltaSeconds);
	void SwitchToDirectReturnRoute();
	void UpdateStuckDetection(float DeltaSeconds);
	void RefreshStatusText();
	bool TryRingSpecialOrderBell();
	bool TryRingSpecialOrderBellReminder();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySpecialOrderBell(FVector_NetQuantize BellLocation);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Special Orders|Audio")
	TObjectPtr<USoundBase> SpecialOrderBellSound;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Special Orders|Audio", meta=(ClampMin="0.0", ClampMax="2.0"))
	float SpecialOrderBellVolume = 0.8f;
	bool bSpecialOrderBellRung = false;
	float NextSpecialOrderBellTime = 0.0f;

	UFUNCTION()
	void OnRep_VisitorState();

	UFUNCTION()
	void OnRep_SpeechLine();

	UFUNCTION()
	void OnRep_CarriedPlant();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusSalesDisplayActor> TargetDisplay;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> SpeechBubbleComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CarriedPotVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CarriedPlantVisual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CarriedPlantMaterial;

	UPROPERTY(ReplicatedUsing=OnRep_VisitorState)
	FGuid SpecialOrderId;
	UPROPERTY(Transient)
	TWeakObjectPtr<ABotanicusCashRegisterActor> SpecialOrderCounter;
	FVector SpecialOrderStandLocation = FVector::ZeroVector;
	FVector EntranceLocation = FVector::ZeroVector;
	TArray<FVector> RoutePoints;
	TArray<FVector> PurchaseRoute;
	TArray<FVector> DirectReturnRoute;
	int32 PurchaseCheckoutWaypointIndex = INDEX_NONE;
	int32 RouteWaypointIndex = 0;
	int32 CheckoutWaypointIndex = INDEX_NONE;
	bool bPlantSelected = false;
	bool bVisitOutcomeRecorded = false;
	bool bReturningToRoute = false;
	TArray<TWeakObjectPtr<ABotanicusSalesDisplayActor>>
		InspectedDisplays;
	int32 DesiredInspectionCount = 2;
	int32 CompletedInspections = 0;
	FName PreferredPlantColor = NAME_None;
	FName PreferredPlantType = NAME_None;
	int32 ShoppingBudget = 60;
	int32 MinimumAcceptableScore = 65;
	int32 CurrentInspectionScore = 0;
	TWeakObjectPtr<ABotanicusSalesDisplayActor> BestMatchingDisplay;
	int32 BestMatchingScore = TNumericLimits<int32>::Lowest();
	FVector RouteResumeLocation = FVector::ZeroVector;
	FVector QueueDestination = FVector::ZeroVector;
	FVector CheckoutQueueDestination = FVector::ZeroVector;
	float QueueLongitudinalOffset = 0.0f;
	float QueueLateralOffset = 0.0f;
	float CheckoutQueueArrivalTime = 0.0f;
	float CheckoutWaitDuration = 0.0f;
	UPROPERTY(ReplicatedUsing=OnRep_VisitorState)
	EBotanicusVisitorState VisitorState =
		EBotanicusVisitorState::FollowingRoute;
	UPROPERTY(ReplicatedUsing=OnRep_SpeechLine)
	FString SpeechLine;
	UPROPERTY(ReplicatedUsing=OnRep_CarriedPlant)
	bool bCarryingPlant = false;
	UPROPERTY(ReplicatedUsing=OnRep_CarriedPlant)
	FName CarriedPlantItemKey = NAME_None;
	UPROPERTY(ReplicatedUsing=OnRep_CarriedPlant)
	FName CarriedPotItemKey = NAME_None;
	UPROPERTY(ReplicatedUsing=OnRep_VisitorState)
	uint8 CheckoutStage = 0;
	UPROPERTY(ReplicatedUsing=OnRep_VisitorState)
	TObjectPtr<ABotanicusSelfCheckoutActor> AssignedSelfCheckout;
	float SelfCheckoutElapsed = 0.0f;
	float InspectionRemaining = 0.0f;
	float DisplaySearchRemaining = 0.0f;
	FVector LastMovementLocation = FVector::ZeroVector;
	float StuckDuration = 0.0f;
	float MovementSpeed = 180.0f;
	float AcceptanceRadius = 135.0f;
	float InspectionDuration = 3.0f;
};
