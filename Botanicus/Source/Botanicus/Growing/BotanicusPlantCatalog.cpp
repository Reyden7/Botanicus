// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantCatalog.h"

const FBotanicusPlantDefinition* UBotanicusPlantCatalog::FindPlant(
	FName PlantKey) const
{
	return Plants.FindByPredicate(
		[PlantKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.PlantKey == PlantKey;
		});
}

const FBotanicusPlantDefinition*
UBotanicusPlantCatalog::FindPlantBySeed(FName SeedItemKey) const
{
	return Plants.FindByPredicate(
		[SeedItemKey](const FBotanicusPlantDefinition& Definition)
		{
			return Definition.SeedItemKey == SeedItemKey;
		});
}
