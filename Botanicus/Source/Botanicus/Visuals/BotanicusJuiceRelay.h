#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusJuiceRelay.generated.h"
/** Transient planting and lifecycle events; watering reuses the existing replicated audio active-source list unchanged. */
UCLASS(NotBlueprintable)
class ABotanicusJuiceRelay : public AActor
{
	GENERATED_BODY()
public:
	ABotanicusJuiceRelay();
	void Lifecycle(AActor* Pot,uint8 Event,const FVector& Position);
	UFUNCTION(NetMulticast,Unreliable) void MulticastLifecycle(uint32 Sequence,AActor* Pot,uint8 Event,FVector_NetQuantize Position);
	void PlantSucceeded(AActor* Pot,const FVector& Position);
	UFUNCTION(NetMulticast,Unreliable) void MulticastPlant(uint32 Sequence,AActor* Pot,FVector_NetQuantize Position);
private:
	uint32 NextSequence=0;
};
