#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpecialOrders/BotanicusSpecialOrderTypes.h"
#include "BotanicusSpecialOrderComponent.generated.h"

class ABotanicusCashRegisterActor;
class ABotanicusCharacter;
struct FBotanicusQuickBarSlot;

/** Shared orders live on GameState, including for late-joining clients. */
UCLASS(ClassGroup=(Botanicus))
class BOTANICUS_API UBotanicusSpecialOrderComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBotanicusSpecialOrderComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static UBotanicusSpecialOrderComponent* Get(const UWorld* World);
	const TArray<FBotanicusSpecialOrder>& GetOrders() const { return Orders; }
	const FBotanicusSpecialOrder* FindOrder(FGuid Id) const;
	float GetRemainingSeconds(const FBotanicusSpecialOrder& Order) const;
	bool TryAssignVisitor(ABotanicusVisitorCharacter* Visitor);
	void CustomerArrived(FGuid Id, ABotanicusVisitorCharacter* Visitor);
	void CustomerDeparted(FGuid Id, ABotanicusVisitorCharacter* Visitor);
	bool CanInteract(FGuid Id, AActor* Interactor) const;
	void Interact(FGuid Id, AActor* Interactor);
	FText GetRequestText(FGuid Id) const;
	TArray<FBotanicusSpecialOrderSaveData> CaptureSaveData() const;
	void RestoreSaveData(const TArray<FBotanicusSpecialOrderSaveData>& Saved);

private:
	friend class FBotanicusSpecialOrderLifecycleTest;
	bool BuildRequest(FBotanicusSpecialOrder& OutOrder) const;
	bool ResolvePlant(FName ItemKey, bool bPreparedPot, FBotanicusSpecialOrderPlant& OutPlant) const;
	bool ResolveSlot(const FBotanicusQuickBarSlot& Slot, FBotanicusSpecialOrderPlant& OutPlant) const;
	ABotanicusCashRegisterActor* FindCounter() const;
	bool ValidateInteraction(const FBotanicusSpecialOrder& Order, AActor* Interactor) const;
	void FinishOrder(FBotanicusSpecialOrder& Order, EBotanicusSpecialOrderStatus Status);
	void Changed(bool bSave = true);
	float ServerTime() const;

	UPROPERTY(Replicated) TArray<FBotanicusSpecialOrder> Orders;
	float NextOfferServerTime = 30.0f;
	int32 NextOrderNumber = 1;
	bool bInteractionInProgress = false;
};
