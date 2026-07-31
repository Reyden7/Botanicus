// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusBuildingCatalog.h"

const FBotanicusBuildingDefinition*
UBotanicusBuildingCatalog::FindBuilding(FName BuildingKey) const
{
	return Buildings.FindByPredicate(
		[BuildingKey](const FBotanicusBuildingDefinition& Definition)
		{
			return Definition.BuildingKey == BuildingKey;
		});
}
