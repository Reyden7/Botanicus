// Copyright Epic Games, Inc. All Rights Reserved.

#include "IllegalTrade/BotanicusIllegalCustomerSpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

ABotanicusIllegalCustomerSpawnPoint::ABotanicusIllegalCustomerSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(SceneRoot);
	SpawnPoint->ArrowColor = FColor::Blue;
	SpawnPoint->SetRelativeLocation(FVector(-350.0f, 0.0f, 0.0f));

	TargetPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("TargetPoint"));
	TargetPoint->SetupAttachment(SceneRoot);
	TargetPoint->ArrowColor = FColor::Purple;

	ExitPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(SceneRoot);
	ExitPoint->ArrowColor = FColor::Red;
	ExitPoint->SetRelativeLocation(FVector(-500.0f, 0.0f, 0.0f));
}

FVector ABotanicusIllegalCustomerSpawnPoint::GetSpawnLocation() const
{
	return SpawnPoint->GetComponentLocation();
}

FVector ABotanicusIllegalCustomerSpawnPoint::GetWaitingLocation() const
{
	return TargetPoint->GetComponentLocation();
}

FVector ABotanicusIllegalCustomerSpawnPoint::GetExitLocation() const
{
	return ExitPoint->GetComponentLocation();
}
