// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantCompatibility.h"

#include "EngineUtils.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "Growing/BotanicusPlantPotActor.h"

namespace
{
	void AccumulateNeighbour(
		FName NeighbourPlantKey,
		const FVector& NeighbourLocation,
		const FVector& TargetLocation,
		float RadiusSquared,
		const FBotanicusPlantDefinition& Definition,
		FBotanicusPlantCompatibilityResult& Result)
	{
		if (NeighbourPlantKey.IsNone() ||
			FVector::DistSquared2D(NeighbourLocation, TargetLocation) >
				RadiusSquared)
		{
			return;
		}
		if (Definition.CompatibleNeighbourPlantKeys.Contains(
				NeighbourPlantKey))
		{
			++Result.CompatibleNeighbourCount;
		}
		else if (Definition.IncompatibleNeighbourPlantKeys.Contains(
					 NeighbourPlantKey))
		{
			++Result.IncompatibleNeighbourCount;
		}
	}
}

FBotanicusPlantCompatibilityResult EvaluateBotanicusPlantCompatibility(
	const UWorld* World,
	const FVector& TargetLocation,
	const AActor* IgnoredOwner,
	int32 IgnoredSlotIndex,
	const FBotanicusPlantDefinition& Definition)
{
	FBotanicusPlantCompatibilityResult Result;
	const float Radius = FMath::Max(
		0.0f,
		Definition.ElementalInteractionRadius);
	if (!World || Radius <= KINDA_SMALL_NUMBER)
	{
		return Result;
	}

	const float RadiusSquared = FMath::Square(Radius);
	for (TActorIterator<ABotanicusPlantPotActor> PotIt(World); PotIt; ++PotIt)
	{
		const ABotanicusPlantPotActor* Pot = *PotIt;
		if (!IsValid(Pot))
		{
			continue;
		}
		// Local placement silhouettes copy the complete plant state. They are
		// visual-only and must never be counted as an additional neighbour.
		if (Pot->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}
		if (const ABotanicusMultiPlantPotActor* MultiPot =
				Cast<ABotanicusMultiPlantPotActor>(Pot))
		{
			const TArray<FBotanicusMultiPlantSlotState>& Slots =
				MultiPot->GetPlantSlots();
			for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
			{
				if (MultiPot == IgnoredOwner &&
					(IgnoredSlotIndex == INDEX_NONE ||
					 SlotIndex == IgnoredSlotIndex))
				{
					continue;
				}
				const FBotanicusMultiPlantSlotState& Slot = Slots[SlotIndex];
				if (!Slot.bElementalDead)
				{
					AccumulateNeighbour(
						Slot.PlantKey,
						MultiPot->GetActorLocation(),
						TargetLocation,
						RadiusSquared,
						Definition,
						Result);
				}
			}
			continue;
		}

		if (Pot == IgnoredOwner || Pot->IsElementalDead())
		{
			continue;
		}
		AccumulateNeighbour(
			Pot->GetPlantKey(),
			Pot->GetActorLocation(),
			TargetLocation,
			RadiusSquared,
			Definition,
			Result);
	}

	const float Bonus =
		FMath::Max(0.0f, Definition.CompatibleNeighbourGrowthBonus) *
		static_cast<float>(Result.CompatibleNeighbourCount);
	const float Penalty =
		FMath::Max(0.0f, Definition.IncompatibleNeighbourGrowthPenalty) *
		static_cast<float>(Result.IncompatibleNeighbourCount);
	Result.GrowthMultiplier = FMath::Clamp(
		1.0f + Bonus - Penalty,
		0.25f,
		2.0f);
	return Result;
}
