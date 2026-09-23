// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/CapsuleComponent.h"
#include "IllegalTrade/BotanicusIllegalCustomerCharacter.h"
#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"
#include "UI/BotanicusIllegalOrderWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusIllegalTradeScheduleTest,
	"Botanicus.IllegalTrade.Schedule.OvernightWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusIllegalTradeScheduleTest::RunTest(const FString& Parameters)
{
	const UBotanicusIllegalTradeSettings* Settings =
		GetDefault<UBotanicusIllegalTradeSettings>();
	TestTrue(TEXT("The illegal trade starts at 21:00"), Settings->IsIllegalTradeMinute(21.0f * 60.0f));
	TestTrue(TEXT("The overnight window includes 04:59"), Settings->IsIllegalTradeMinute(4.0f * 60.0f + 59.0f));
	TestFalse(TEXT("The illegal trade ends at 05:00"), Settings->IsIllegalTradeMinute(5.0f * 60.0f));
	TestFalse(TEXT("The illegal trade is inactive at 20:00"), Settings->IsIllegalTradeMinute(20.0f * 60.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusIllegalTradeDefaultsTest,
	"Botanicus.IllegalTrade.Data.DefaultNoctiflore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusIllegalTradeDefaultsTest::RunTest(const FString& Parameters)
{
	const UBotanicusIllegalTradeSettings* Settings =
		GetDefault<UBotanicusIllegalTradeSettings>();
	const FBotanicusIllegalPlantDefinition* Plant = Settings->FindPlant(TEXT("Noctiflore"));
	TestNotNull(TEXT("Noctiflore is configured"), Plant);
	if (Plant)
	{
		TestEqual(TEXT("Noctiflore seed key"), Plant->SeedItemKey, FName(TEXT("SeedPacket_Noctiflore")));
		TestEqual(TEXT("Noctiflore product key"), Plant->ProductItemKey, FName(TEXT("IllegalPlantProduct_Noctiflore")));
		TestTrue(TEXT("Growth duration is positive"), Plant->GrowthDurationSeconds > 0.0f);
		TestTrue(TEXT("Noctiflore uses less than the former 0.0025/s water rate"),
			Plant->WaterConsumptionPerSecond > 0.0f &&
			Plant->WaterConsumptionPerSecond < 0.0025f);
		TestTrue(TEXT("Three distinct growth skins are configured"),
			!Plant->SeedlingMesh.IsNull() && !Plant->YoungMesh.IsNull() &&
			!Plant->MatureMesh.IsNull() &&
			Plant->SeedlingMesh != Plant->YoungMesh &&
			Plant->YoungMesh != Plant->MatureMesh);
		TestNotNull(TEXT("Seedling skin loads"), Plant->SeedlingMesh.LoadSynchronous());
		TestNotNull(TEXT("Young skin loads"), Plant->YoungMesh.LoadSynchronous());
		TestNotNull(TEXT("Mature skin loads"), Plant->MatureMesh.LoadSynchronous());
		TestTrue(TEXT("Harvest gives at least one product"), Plant->HarvestQuantity > 0);
		TestEqual(TEXT("Noctiflore unit sale value"), Plant->UnitSaleValue, 150);
	}
	const FBotanicusIllegalPlanterLevelDefinition& Level = Settings->GetPlanterLevel(1);
	TestEqual(TEXT("MVP planter has four slots"), Level.SlotCount, 4);
	TestEqual(TEXT("MVP planter requires four soil units"), Level.RequiredSoilUnits, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusIllegalTradeAssetsTest,
	"Botanicus.IllegalTrade.Assets.BlueprintsLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusIllegalTradeAssetsTest::RunTest(const FString& Parameters)
{
	UClass* PlanterClass = LoadClass<ABotanicusIllegalPlanterActor>(nullptr,
		TEXT("/Game/Botanicus/blueprints/BP_Item_IllegalPlanter.BP_Item_IllegalPlanter_C"));
	UClass* OrderWidgetClass = LoadClass<UBotanicusIllegalOrderWidget>(nullptr,
		TEXT("/Game/Botanicus/UI/IllegalTrade/WBP_IllegalCustomerOrder.WBP_IllegalCustomerOrder_C"));
	UClass* CustomerClass = LoadClass<ABotanicusIllegalCustomerCharacter>(nullptr,
		TEXT("/Game/Botanicus/blueprints/BP_IllegalCustomer.BP_IllegalCustomer_C"));
	TestNotNull(TEXT("Editable clandestine planter Blueprint loads"), PlanterClass);
	TestNotNull(TEXT("Customer order widget Blueprint loads"), OrderWidgetClass);
	TestNotNull(TEXT("Editable clandestine customer Blueprint loads"), CustomerClass);
	const ABotanicusIllegalCustomerCharacter* CustomerDefaults =
		GetDefault<ABotanicusIllegalCustomerCharacter>();
	TestEqual(TEXT("Customer can be aimed at with visibility traces"),
		CustomerDefaults->GetCapsuleComponent()->GetCollisionResponseToChannel(
			ECC_Visibility), ECR_Block);
	if (CustomerClass)
	{
		const ABotanicusIllegalCustomerCharacter* BlueprintDefaults =
			CustomerClass->GetDefaultObject<ABotanicusIllegalCustomerCharacter>();
		TestEqual(TEXT("Customer Blueprint keeps visibility targeting"),
			BlueprintDefaults->GetCapsuleComponent()->GetCollisionResponseToChannel(
				ECC_Visibility), ECR_Block);
	}
	return true;
}

#endif
