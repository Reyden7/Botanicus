#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusJuiceRelay.generated.h"
/** Planting only: watering reuses the existing replicated audio active-source list unchanged. */
UCLASS(NotBlueprintable)
class ABotanicusJuiceRelay : public AActor
{
	GENERATED_BODY()
public:
	ABotanicusJuiceRelay();
	void PlantSucceeded(AActor* Pot,const FVector& Position);
	UFUNCTION(NetMulticast,Unreliable) void MulticastPlant(uint32 Sequence,AActor* Pot,FVector_NetQuantize Position);
private:
	uint32 NextSequence=0;
};
