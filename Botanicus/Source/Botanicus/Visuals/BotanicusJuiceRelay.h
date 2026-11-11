#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Visuals/BotanicusPickupJuiceTypes.h"
#include "BotanicusJuiceRelay.generated.h"
class APawn;
USTRUCT()
struct FBotanicusPlacementEvent
{
    GENERATED_BODY()
    UPROPERTY() uint32 Sequence=0;
    UPROPERTY() TObjectPtr<AActor> Item;
    UPROPERTY() TObjectPtr<APawn> Placer;
    UPROPERTY() FName ItemKey;
    UPROPERTY() FTransform Final;
    UPROPERTY() FVector_NetQuantize Start;
    UPROPERTY() FName AudioId;
    UPROPERTY() bool bContentsOnly=false;
};
/** Transient planting and lifecycle events; watering reuses the existing replicated audio active-source list unchanged. */
UCLASS(NotBlueprintable)
class ABotanicusJuiceRelay : public AActor
{
	GENERATED_BODY()
public:
	ABotanicusJuiceRelay();
    void Pickup(APawn* Pawn,FName ItemKey,const FBotanicusPickupVisual& Visual,AActor* CarriedSource);
    UFUNCTION(NetMulticast,Reliable) void MulticastPickup(uint32 Sequence,APawn* Pawn,FName ItemKey,const FBotanicusPickupVisual& Visual,AActor* CarriedSource);
    UFUNCTION(NetMulticast,Reliable) void MulticastAbortPickup(AActor* Source);
	void Placement(AActor* Item,APawn* Placer,FName ItemKey,const FTransform& Final,const FVector& Start,FName AudioId,bool bContentsOnly=false);
    UFUNCTION(NetMulticast,Unreliable) void MulticastPlacementBatch(const TArray<FBotanicusPlacementEvent>& Events);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Lifecycle(AActor* Pot,uint8 Event,const FVector& Position);
	UFUNCTION(NetMulticast,Unreliable) void MulticastLifecycle(uint32 Sequence,AActor* Pot,uint8 Event,FVector_NetQuantize Position);
	void PlantSucceeded(AActor* Pot,const FVector& Position);
	UFUNCTION(NetMulticast,Unreliable) void MulticastPlant(uint32 Sequence,AActor* Pot,FVector_NetQuantize Position);
private:
    void FlushPlacementBatch();
    void ReceiveLocalPlacement(const FBotanicusPlacementEvent& Event);
    UPROPERTY(Transient) TArray<FBotanicusPlacementEvent> PendingPlacementEvents;
    bool bPlacementFlushScheduled=false;
	uint32 NextSequence=0;
};
