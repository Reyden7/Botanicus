// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "BotanicusDeliveryParcelActor.generated.h"

class UTextRenderComponent;
class UStaticMeshComponent;

/** Replicated delivery carton opened by tracing a cutter along its tape. */
UCLASS()
class BOTANICUS_API ABotanicusDeliveryParcelActor
	: public ABotanicusInteractableActor
{
	GENERATED_BODY()

public:
	ABotanicusDeliveryParcelActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FBotanicusInteractionPrompt
		GetInteractionPrompt_Implementation(
			AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	void InitializeParcel(FName InItemKey, int32 InQuantity);
	void RestoreParcelState(
		FName InItemKey,
		int32 InQuantity,
		uint16 InCutCoverageMask,
		bool bInOpened);
	void BeginCutting(AActor* Interactor);
	void EndCutting(AActor* Interactor);
	FName GetItemKey() const { return ItemKey; }
	int32 GetQuantity() const { return Quantity; }
	uint16 GetCutCoverageMask() const { return CutCoverageMask; }
	bool IsOpened() const { return bOpened; }
	FVector GetParcelHalfExtent() const { return ParcelHalfExtent; }

private:
	void RefreshParcelAppearance();
	void UpdateCutterTrace();
	void MarkTapeSegment(int32 SegmentIndex);
	int32 CountCutSegments() const;
	bool ReleaseContents(AActor* Interactor);
	void SendInteractorMessage(
		AActor* Interactor,
		const FString& Message) const;

	UFUNCTION()
	void OnRep_ParcelState();

	UPROPERTY(ReplicatedUsing=OnRep_ParcelState)
	FName ItemKey = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_ParcelState)
	int32 Quantity = 1;

	UPROPERTY(ReplicatedUsing=OnRep_ParcelState)
	uint16 CutCoverageMask = 0;

	UPROPERTY(ReplicatedUsing=OnRep_ParcelState)
	bool bOpened = false;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionIndicator;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> TapeSegments;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LeftFlap;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RightFlap;

	TWeakObjectPtr<class ABotanicusCharacter> ActiveCutter;
	FVector ParcelHalfExtent = FVector(27.5f, 22.5f, 17.5f);
	float LastCutLocalX = 0.0f;
	bool bHasLastCutSample = false;

	static constexpr int32 TapeSegmentCount = 16;
};
