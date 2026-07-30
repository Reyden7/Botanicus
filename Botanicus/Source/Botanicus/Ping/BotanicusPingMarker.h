// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusPingMarker.generated.h"

class UTextRenderComponent;

/** Lightweight replicated world marker used by the Botanicus ping system. */
UCLASS()
class BOTANICUS_API ABotanicusPingMarker : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusPingMarker();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Configures server-owned data before the initial replication update. */
	void InitializePing(const FString& InOwnerDisplayName, float LifeTime);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Ping")
	TObjectPtr<UTextRenderComponent> MarkerText;

	UPROPERTY(ReplicatedUsing=OnRep_OwnerDisplayName)
	FString OwnerDisplayName;

	UFUNCTION()
	void OnRep_OwnerDisplayName();

	void RefreshMarkerText();
};
