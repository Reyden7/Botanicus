// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/BotanicusWaterSourceComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Water/BotanicusWaterSourceProfile.h"

UBotanicusWaterSourceComponent::UBotanicusWaterSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBotanicusWaterSourceComponent::SetNozzleComponent(
	USceneComponent* InNozzleComponent)
{
	NozzleComponent = InNozzleComponent;
}

void UBotanicusWaterSourceComponent::SetWaterSourceActive(bool bNewActive)
{
	bSourceActive = bNewActive;
}

const UBotanicusWaterSourceProfile*
	UBotanicusWaterSourceComponent::GetSourceProfile() const
{
	return SourceProfile
		? SourceProfile.Get()
		: GetDefault<UBotanicusWaterSourceProfile>();
}

float UBotanicusWaterSourceComponent::GetSimulationInterval() const
{
	const UBotanicusWaterSourceProfile* Profile = GetSourceProfile();
	return 1.0f / FMath::Max(1.0f, Profile->SimulationFrequency);
}

bool UBotanicusWaterSourceComponent::BuildStreamParams(
	FBotanicusWaterStreamParams& OutParams) const
{
	if (!NozzleComponent)
	{
		return false;
	}
	BuildStreamParams(
		NozzleComponent->GetComponentLocation(),
		NozzleComponent->GetForwardVector(),
		OutParams);
	return true;
}

void UBotanicusWaterSourceComponent::BuildStreamParams(
	const FVector& Origin,
	const FVector& Direction,
	FBotanicusWaterStreamParams& OutParams) const
{
	const UBotanicusWaterSourceProfile* Profile = GetSourceProfile();
	OutParams = FBotanicusWaterStreamParams();
	OutParams.Origin = Origin;
	OutParams.Direction = Direction.GetSafeNormal();
	OutParams.InitialSpeed = FMath::Max(0.0f, Profile->InitialSpeed);
	const float WorldGravityZ = GetWorld() ? GetWorld()->GetGravityZ() : -980.0f;
	OutParams.Gravity = FVector(
		0.0f,
		0.0f,
		WorldGravityZ * FMath::Max(0.0f, Profile->GravityScale));
	OutParams.Radius = FMath::Max(0.1f, Profile->TraceRadius);
	OutParams.FlowRate = FMath::Max(0.0f, Profile->FlowRate);
	OutParams.MaxSimulationTime = FMath::Max(0.01f, Profile->MaxSimulationTime);
	OutParams.MaxDistance = FMath::Max(1.0f, Profile->MaxDistance);
	OutParams.TargetSegmentLength = FMath::Max(5.0f, Profile->TargetSegmentLength);
	OutParams.MaxCurveDeviation = FMath::Max(0.1f, Profile->MaxCurveDeviation);
	OutParams.MaxTraceSegments = FMath::Clamp(Profile->MaxTraceSegments, 1, 64);
}
