// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Sales/Checkout/BotanicusCheckoutTypes.h"
#include "BotanicusCashRegisterActor.generated.h"

class ABotanicusVisitorCharacter;
class ABotanicusCheckoutScanner;
class ABotanicusPaymentCoins;
class ABotanicusPlayerController;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Starter checkout station used by players to scan and collect sales. */
UCLASS()
class BOTANICUS_API ABotanicusCashRegisterActor
	: public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusCashRegisterActor();
    virtual void OnConstruction(const FTransform& Transform) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    const FBotanicusCheckoutReceipt& GetReceipt() const {return Receipt;}
    bool TryScanCustomer(ABotanicusPlayerController* PC,ABotanicusVisitorCharacter* Customer);
    bool TryCollectPayment(ABotanicusPlayerController* PC,const FGuid& ExpectedId);
    double CheckoutServerTime() const;
    FVector GetDrawerDepositLocation() const;
    bool IsScannerDockHit(const UPrimitiveComponent* Component) const;
    ABotanicusCheckoutScanner* GetDockScanner() const {return DockScanner;}
    ABotanicusPaymentCoins* GetPaymentCoins() const {return PaymentCoins;}
    FBotanicusSavedCashRegister CaptureCheckout() const;
    void RestoreCheckout(const FBotanicusSavedCashRegister& Saved);
    int32 GetAcceptedScans() const {return AcceptedScans;}
    int32 GetAcceptedCollections() const {return AcceptedCollections;}
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Checkout Points") TObjectPtr<USceneComponent> ScannerDock;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Checkout Points") TObjectPtr<USceneComponent> CoinDropPoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Checkout Points") TObjectPtr<USceneComponent> DrawerTarget;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Checkout Points") TObjectPtr<USceneComponent> DrawerClosedTarget;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Checkout Points") TObjectPtr<USceneComponent> DrawerOpenTarget;


	virtual void Tick(float DeltaSeconds) override;
	virtual void ConfigureAsLocalPreview(bool bIsValid) override;

	FVector GetCustomerStandLocation() const;
	ABotanicusVisitorCharacter* GetCheckoutCustomer() const;
	void HandleCheckoutAction(AActor* Interactor);
	bool IsOperational() const { return IsInsideCheckoutZone(); }
	FTransform GetSelfCheckoutSlotTransform(int32 SlotIndex) const;
	bool FindClosestAvailableSelfCheckoutSlot(
		const FVector& RequestedLocation,
		FTransform& OutTransform,
		const AActor* IgnoredSelfCheckout = nullptr) const;
	int32 FindSelfCheckoutSlotIndex(
		const FVector& WorldLocation,
		float Tolerance = 35.0f) const;
	static constexpr int32 SelfCheckoutSlotCount = 6;

protected:
    virtual void ApplyItemDefinition() override;
	virtual void BeginPlay() override;

private:

    friend class UBotanicusCashRegisterValidation;
    void InitializeCheckoutComponents();
    void ConfigureRegisterRepresentation();
    void BeginCheckout();
    void UpdatePaymentAnchor(ABotanicusVisitorCharacter* Customer);
    void TickCheckout(float DeltaSeconds);
    void NotifyCheckoutChanged();
    UFUNCTION() void OnRep_Checkout();
    UPROPERTY(ReplicatedUsing=OnRep_Checkout) FBotanicusCheckoutReceipt Receipt;
    UPROPERTY(Replicated) FGuid RegisterId;
    UPROPERTY(ReplicatedUsing=OnRep_Checkout) TObjectPtr<ABotanicusCheckoutScanner> DockScanner;
    UPROPERTY(ReplicatedUsing=OnRep_Checkout) TObjectPtr<ABotanicusPaymentCoins> PaymentCoins;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> DrawerVisualRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DrawerMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DrawerLip;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScannerDockMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> ScannerDockHit;
    TWeakObjectPtr<ABotanicusVisitorCharacter> OpeningCustomer;
    FGuid LastBeamTransaction;
    int32 AcceptedScans=0,AcceptedCollections=0;
	void InitializeVisualMaterials();
	bool IsInsideCheckoutZone() const;
	bool IsLocalPlayerTargetingRegister() const;
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CounterVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RegisterVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ScreenVisual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ContextActionText;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>>
		SelfCheckoutSlotVisuals;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> SelfCheckoutZoneLabel;
};
