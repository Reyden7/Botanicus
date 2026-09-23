// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/BotanicusInteractable.h"
#include "BotanicusIllegalCustomerCharacter.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EBotanicusIllegalCustomerState : uint8
{
	Approaching,
	Waiting,
	Leaving
};

/** Server-authoritative nocturnal customer with one simple product order. */
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusIllegalCustomerCharacter
	: public ACharacter,
	  public IBotanicusInteractable
{
	GENERATED_BODY()

public:
	ABotanicusIllegalCustomerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	void InitializeIllegalOrder(
		FName InProductItemKey,
		FText InProductName,
		int32 InQuantity,
		int32 InReward,
		float InWaitSeconds,
		const FVector& InTargetLocation,
		const FVector& InExitLocation);

	void ForceCustomerToLeave();

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	EBotanicusIllegalCustomerState GetCustomerState() const { return CustomerState; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	FText GetProductName() const { return ProductName; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	int32 GetRequestedQuantity() const { return RequestedQuantity; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	int32 GetRewardCredits() const { return RewardCredits; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Illegal Trade")
	float GetRemainingWaitSeconds() const { return RemainingWaitSeconds; }

private:
	UFUNCTION()
	void OnRep_OrderState();

	void MoveTo(const FVector& Destination);
	void EnterWaitingState();
	void BeginLeaving(bool bOrderSold);
	void RefreshOrderText();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> BodyVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> OrderWidget;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	EBotanicusIllegalCustomerState CustomerState = EBotanicusIllegalCustomerState::Approaching;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	FName ProductItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	FText ProductName;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	int32 RequestedQuantity = 0;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	int32 RewardCredits = 0;

	UPROPERTY(ReplicatedUsing=OnRep_OrderState)
	float RemainingWaitSeconds = 0.0f;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 TargetLocation;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 ExitLocation;

	float ReplicationAccumulator = 0.0f;
	float LeavingElapsedSeconds = 0.0f;
	float MovementRetryElapsedSeconds = 0.0f;
	float MovementDiagnosticElapsedSeconds = 0.0f;
	FVector LastMovementDiagnosticPosition = FVector::ZeroVector;
};
