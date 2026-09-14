// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Water/BotanicusWaterTypes.h"
#include "BotanicusWaterSourceComponent.generated.h"

class USceneComponent;
class UBotanicusWaterSourceProfile;

/** Reusable definition of a water-producing point such as a nozzle or tap. */
UCLASS(ClassGroup=(Botanicus), BlueprintType, Blueprintable,
	meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusWaterSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBotanicusWaterSourceComponent();

	void SetNozzleComponent(USceneComponent* InNozzleComponent);

	UFUNCTION(BlueprintCallable, Category="Botanicus|Water")
	void SetWaterSourceActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category="Botanicus|Water")
	bool IsWaterSourceActive() const { return bSourceActive; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Water")
	const UBotanicusWaterSourceProfile* GetSourceProfile() const;

	UFUNCTION(BlueprintPure, Category="Botanicus|Water")
	float GetSimulationInterval() const;

	/** Creates the common stream description from this component's nozzle. */
	bool BuildStreamParams(FBotanicusWaterStreamParams& OutParams) const;

	/** Creates authoritative parameters from a validated origin and direction. */
	void BuildStreamParams(
		const FVector& Origin,
		const FVector& Direction,
		FBotanicusWaterStreamParams& OutParams) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Botanicus|Water",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBotanicusWaterSourceProfile> SourceProfile;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> NozzleComponent;

	bool bSourceActive = false;
};
