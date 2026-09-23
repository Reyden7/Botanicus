// Copyright Epic Games, Inc. All Rights Reserved.

#include "IllegalTrade/BotanicusIllegalTradeSettings.h"

DEFINE_LOG_CATEGORY(LogBotanicusIllegalTrade);

UBotanicusIllegalTradeSettings::UBotanicusIllegalTradeSettings()
{
	FBotanicusIllegalPlantDefinition& Noctiflore = Plants.AddDefaulted_GetRef();
	const FString MeshRoot =
		TEXT("/Game/Botanicus/Items/itemsMesh/Trafic/plantes/");
	Noctiflore.SeedlingMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(MeshRoot + TEXT("1/1.1")));
	Noctiflore.YoungMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(MeshRoot + TEXT("2/2.2")));
	Noctiflore.MatureMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(MeshRoot + TEXT("3/3.3")));
	PlanterLevels.AddDefaulted();
	CustomerClass = TSoftClassPtr<ABotanicusIllegalCustomerCharacter>(FSoftClassPath(
		TEXT("/Game/Botanicus/blueprints/BP_IllegalCustomer.BP_IllegalCustomer_C")));
}

const FBotanicusIllegalPlantDefinition*
UBotanicusIllegalTradeSettings::FindPlant(FName PlantId) const
{
	return Plants.FindByPredicate(
		[PlantId](const FBotanicusIllegalPlantDefinition& Plant)
		{
			return Plant.PlantId == PlantId;
		});
}

const FBotanicusIllegalPlantDefinition*
UBotanicusIllegalTradeSettings::FindPlantBySeed(FName SeedItemKey) const
{
	return Plants.FindByPredicate(
		[SeedItemKey](const FBotanicusIllegalPlantDefinition& Plant)
		{
			return Plant.SeedItemKey == SeedItemKey;
		});
}

const FBotanicusIllegalPlanterLevelDefinition&
UBotanicusIllegalTradeSettings::GetPlanterLevel(int32 Level) const
{
	if (const FBotanicusIllegalPlanterLevelDefinition* Found =
			PlanterLevels.FindByPredicate(
				[Level](const FBotanicusIllegalPlanterLevelDefinition& Entry)
				{
					return Entry.Level == Level;
				}))
	{
		return *Found;
	}
	static const FBotanicusIllegalPlanterLevelDefinition DefaultLevel;
	return DefaultLevel;
}

bool UBotanicusIllegalTradeSettings::IsIllegalTradeMinute(float DayMinute) const
{
	const float Minute = FMath::Fmod(FMath::Max(0.0f, DayMinute), 1440.0f);
	return TradeStartMinute <= TradeEndMinute
		? Minute >= TradeStartMinute && Minute < TradeEndMinute
		: Minute >= TradeStartMinute || Minute < TradeEndMinute;
}
