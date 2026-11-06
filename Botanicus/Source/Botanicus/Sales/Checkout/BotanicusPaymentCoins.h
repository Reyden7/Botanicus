#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sales/Checkout/BotanicusCheckoutTypes.h"
#include "BotanicusPaymentCoins.generated.h"
class ABotanicusCashRegisterActor;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusPaymentCoins : public AActor
{
    GENERATED_BODY()
public:
    ABotanicusPaymentCoins();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void InitializeRegister(ABotanicusCashRegisterActor* InRegister);
    void RefreshFromReceipt();
    ABotanicusCashRegisterActor* GetRegister() const {return Register;}
    int32 GetRenderedCoinCount() const;
    int32 GetArrivalCount() const {return Arrivals;}
private:
    UFUNCTION() void OnRep_Register();
    void UpdatePresentation();
    FVector CounterPosition(int32 Index) const;
    FVector DepositPosition(int32 Index,double Time) const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> HitBox;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> GoldCoins;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CreditVoucher;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> VoucherLabel;
    UPROPERTY(ReplicatedUsing=OnRep_Register) TObjectPtr<ABotanicusCashRegisterActor> Register;
    FGuid LastTransaction;
    EBotanicusCheckoutPhase LastPhase=EBotanicusCheckoutPhase::Idle;
    TArray<bool> ArrivalPlayed;
    bool bCollectionAudio=false;
    int32 Arrivals=0;
};
