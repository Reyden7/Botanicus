#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusCheckoutScanner.generated.h"
class ABotanicusCashRegisterActor;
class ABotanicusPlayerController;
class UBoxComponent;
class UStaticMeshComponent;
UCLASS(Blueprintable)
class BOTANICUS_API ABotanicusCheckoutScanner : public AActor
{
    GENERATED_BODY()
public:
    ABotanicusCheckoutScanner();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void InitializeDock(ABotanicusCashRegisterActor* InRegister);
    bool TryTake(ABotanicusPlayerController* Controller);
    void ReturnToDock();
    APawn* GetCarrier() const {return Carrier;}
    ABotanicusCashRegisterActor* GetRegister() const {return Register;}
    FVector GetMuzzleLocation();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Scanner") TObjectPtr<USceneComponent> ScannerMuzzle;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scanner") TSoftObjectPtr<UStaticMesh> FutureScannerMesh;
private:
    UFUNCTION() void OnRep_Carry();
    void UpdatePose();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> HitBox;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Handle;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(ReplicatedUsing=OnRep_Carry) TObjectPtr<ABotanicusCashRegisterActor> Register;
    UPROPERTY(ReplicatedUsing=OnRep_Carry) TObjectPtr<APawn> Carrier;
};
