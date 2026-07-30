// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catalog/BotanicusItemCatalog.h"

const FBotanicusItemDefinition* UBotanicusItemCatalog::FindItem(
	FName ItemKey) const
{
	return Items.FindByPredicate(
		[ItemKey](const FBotanicusItemDefinition& Definition)
		{
			return Definition.ItemKey == ItemKey;
		});
}
