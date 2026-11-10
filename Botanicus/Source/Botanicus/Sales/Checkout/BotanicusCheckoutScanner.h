#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusCheckoutScanner.generated.h"
class ABotanicusCashRegisterActor;
class ABotanicusPlayerController;
class UBoxComponent;
class ABotanicusVisitorCharacter;
class UStaticMeshComponent;
USTRUCT()
struct FBotanicusCheckoutScan
{
    GENERATED_BODY()
    UPROPERTY() FGuid Id;
    UPROPERTY() TObjectPtr<ABotanicusVisitorCharacter> Customer;
    UPROPERTY() FGuid CustomerId;
    UPROPERTY() FName PlantKey;
    UPROPERTY() double StartedAt=0;
    UPROPERTY() float UpSeconds=.6f;
    UPROPERTY() float DownSeconds=.6f;
    UPROPERTY() float FadeSeconds=.12f;
    UPROPERTY() bool bCompleted=false;
    float TravelSeconds() const {return UpSeconds+DownSeconds;}
    float TotalSeconds() const {return TravelSeconds()+FadeSeconds;}
};
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
    void ConfigureModel();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> HitBox;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Handle;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(ReplicatedUsing=OnRep_Carry) TObjectPtr<ABotanicusCashRegisterActor> Register;
    UPROPERTY(ReplicatedUsing=OnRep_Carry) TObjectPtr<APawn> Carrier;
};
