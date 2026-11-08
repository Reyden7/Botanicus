#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BotanicusSurfaceContactAudio.generated.h"
class UPhysicalMaterial;
class USoundCue;
class UAudioComponent;
class USoundConcurrency;

/** Direct connection to user-owned cues. No Tick, audio catalogue, import or RPC. */
UCLASS(ClassGroup=Audio,meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusSurfaceContactAudio : public UActorComponent
{
    GENERATED_BODY()
public:
    UBotanicusSurfaceContactAudio();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts") TMap<UPhysicalMaterial*,USoundCue*> SurfaceSounds;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts") TObjectPtr<USoundCue> DefaultSound;
    /** Enable ONLY after the body AnimBP routes its real contacts to AnimationContact. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts") bool bUseAnimationContacts=false;
    UFUNCTION(BlueprintCallable,Category="Foot contacts") void AnimationContact(bool bRightFoot);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts",meta=(ClampMin=".01",ClampMax="1")) float Volume=.35f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts",meta=(ClampMin="20")) float SlowStepDistance=80.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts",meta=(ClampMin="20")) float RunStepDistance=200.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Foot contacts",meta=(ClampMin="20")) float SprintStepDistance=245.f;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Diagnostics") int32 Contacts=0;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Diagnostics") int32 Plays=0;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Diagnostics") FName LastFoot;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Diagnostics") TObjectPtr<USoundCue> LastCue;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Diagnostics") TObjectPtr<UPhysicalMaterial> LastPhysicalMaterial;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void MovementUpdated(float DeltaSeconds,FVector OldLocation,FVector OldVelocity);
    void Contact(FName Foot);
    UPROPERTY(Transient) TObjectPtr<USoundConcurrency> Concurrency;
    TArray<TWeakObjectPtr<UAudioComponent>> Voices;
    FVector PreviousPosition=FVector::ZeroVector;
    float Travel=0.f;
    bool bInitialized=false;
    bool bNextRight=false;
    uint64 LastMovementFrame=MAX_uint64;
    uint64 LastContactFrame=MAX_uint64;
};
