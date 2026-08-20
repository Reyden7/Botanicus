// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantCompatibility.h"

#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"

namespace
{
	void AccumulateNeighbour(
		FName NeighbourPlantKey,
		const FVector& NeighbourLocation,
		const FVector& TargetLocation,
		float RadiusSquared,
		const FBotanicusPlantDefinition& Definition,
		const UBotanicusPlantSubsystem* PlantSubsystem,
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
		else if (NeighbourPlantKey == Definition.PlantKey)
		{
			// A second plant of the same species is always a compatible
			// neighbour unless the designer explicitly overrides it above.
			++Result.CompatibleNeighbourCount;
		}
		else if (PlantSubsystem)
		{
			if (const FBotanicusPlantDefinition* NeighbourDefinition =
					PlantSubsystem->FindPlant(NeighbourPlantKey))
			{
				// Explicit catalogue relations have priority. For every other
				// living neighbour, plants of the same element cooperate and
				// plants of different elements compete. This keeps blue strictly
				// reserved for an empty interaction radius.
				if (NeighbourDefinition->Element == Definition.Element)
				{
					++Result.CompatibleNeighbourCount;
				}
				else
				{
					++Result.IncompatibleNeighbourCount;
				}
			}
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
	const UBotanicusPlantSubsystem* PlantSubsystem =
		World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<
				UBotanicusPlantSubsystem>()
			: nullptr;
	for (TActorIterator<ABotanicusPlantPotActor> PotIt(World); PotIt; ++PotIt)
	{
		const ABotanicusPlantPotActor* Pot = *PotIt;
		if (!IsValid(Pot))
		{
			continue;
		}
		// Local placement silhouettes copy the complete plant state. They are
		// visual-only and must never be counted as an additional neighbour.
		if (Pot->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
			Pot->ActorHasTag(TEXT("BotanicusInspectionPreview")))
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
						PlantSubsystem,
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
			PlantSubsystem,
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
