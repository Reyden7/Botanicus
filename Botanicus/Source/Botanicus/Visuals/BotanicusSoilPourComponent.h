#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BotanicusSoilPourComponent.generated.h"
class APawn;
class ABotanicusCharacter;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
USTRUCT()
struct FBotanicusSoilPourState
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<APawn> Source;
    UPROPERTY() int32 Slot=0;
};
/** Presentation of accepted transfers only; no inventory, collision, or particle replication. */
UCLASS(ClassGroup=(Botanicus),meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusSoilPourComponent:public UActorComponent
{
    GENERATED_BODY()
public:
    UBotanicusSoilPourComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void Transferred(APawn* Source,int32 Slot);
    void StopTransfer();
    static FVector BagMouth(const UStaticMeshComponent& Mesh, const FVector& NormalizedOffset);
    UPROPERTY(EditAnywhere,Category="Soil|Juice") TSoftObjectPtr<UNiagaraSystem> PourSystem;
    UPROPERTY(EditAnywhere,Category="Soil|Juice") TSoftObjectPtr<UNiagaraSystem> ContactSystem;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Soil|Juice",meta=(ClampMin="0",ClampMax="220")) float ParticlesPerSecond=180;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Soil|Juice",meta=(ClampMin="0.1",ClampMax="3")) float GrainSize=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Soil|Juice") FVector BagMouthNormalizedOffset=FVector(0,0,.95f);
private:
    UFUNCTION() void OnRep_Pour();
    void UpdateLocal();
    void ClearVisuals();
    FVector Surface() const;
    UPROPERTY(ReplicatedUsing=OnRep_Pour) FBotanicusSoilPourState Pour;
    UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedPour;
    UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedContact;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Stream;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Contact;
    FTimerHandle UpdateTimer,CleanupTimer;
    TWeakObjectPtr<ABotanicusCharacter> ActionSource;
    double StartedAt=0;
};
