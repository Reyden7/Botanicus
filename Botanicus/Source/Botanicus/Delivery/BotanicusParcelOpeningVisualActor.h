#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusParcelOpeningVisualActor.generated.h"
class ABotanicusDeliveryParcelActor;
class USceneComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInstanceDynamic;

USTRUCT()
struct FBotanicusParcelOpeningPresentation
{
	GENERATED_BODY()
	UPROPERTY() FGuid SourceId;
	UPROPERTY() TObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY() TObjectPtr<UAnimSequence> Animation;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Materials;
	UPROPERTY() FTransform MeshLocalTransform;
	UPROPERTY() FVector HalfExtent = FVector::ZeroVector;
	UPROPERTY() double StartedAt = -1;
	UPROPERTY() float Duration = .5f;
	UPROPERTY() float Lifetime = 1;
	UPROPERTY() int32 Count = 40;
	UPROPERTY() bool bReady = false;
};

/** Replicated cosmetic snapshot; never saved and never owns or releases inventory. */
UCLASS(NotBlueprintable, Transient)
class BOTANICUS_API ABotanicusParcelOpeningVisualActor : public AActor
{
	GENERATED_BODY()
public:
	ABotanicusParcelOpeningVisualActor();
	static ABotanicusParcelOpeningVisualActor* Start(ABotanicusDeliveryParcelActor* Parcel);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FGuid& GetSourceId() const { return Presentation.SourceId; }
private:
	void InitializeLocalPresentation();
	void HideLogicalSource();
	void PrepareConfetti();
	void BurstAndHide(double Now);
	UFUNCTION() void OnRep_Presentation();
	UPROPERTY(ReplicatedUsing=OnRep_Presentation) FBotanicusParcelOpeningPresentation Presentation;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> CartonVisual;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedSystem;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Confetti;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ConfettiMaterial;
	TWeakObjectPtr<ABotanicusDeliveryParcelActor> Source;
	bool bConfettiPrepared = false;
	uint64 ConfettiPreparedFrame = 0;
	bool bConfettiActivationRequested = false;
	bool bInitialized = false;
	bool bBurstStarted = false;
};
