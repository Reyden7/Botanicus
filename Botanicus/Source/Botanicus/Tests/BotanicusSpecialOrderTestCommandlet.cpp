#include "Tests/BotanicusSpecialOrderTestCommandlet.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

UBotanicusSpecialOrderTestCommandlet::UBotanicusSpecialOrderTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UBotanicusSpecialOrderTestCommandlet::Main(const FString& Params)
{
#if WITH_DEV_AUTOMATION_TESTS
	int32 Failed = 0;
	auto& Framework = FAutomationTestFramework::Get();
	for (const FString Name : {FString(TEXT("FBotanicusSpecialOrderCriteriaTest")), FString(TEXT("FBotanicusSpecialOrderLifecycleTest"))})
	{
		UE_LOG(LogTemp, Display, TEXT("SPECIAL_ORDER_TEST_START %s"), *Name);
		if (!Framework.ContainsTest(Name))
		{
			++Failed;
			UE_LOG(LogTemp, Error, TEXT("Could not start %s"), *Name);
			continue;
		}
		Framework.StartTestByName(Name, 0);
		FAutomationTestExecutionInfo Info;
		const bool bPassed = Framework.StopTest(Info);
		for (const auto& Entry : Info.GetEntries())
			UE_LOG(LogTemp, Display, TEXT("%s"), *Entry.Event.Message);
		Failed += bPassed ? 0 : 1;
		UE_LOG(LogTemp, Display, TEXT("SPECIAL_ORDER_TEST_RESULT %s %s errors=%d warnings=%d"),
			*Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), Info.GetErrorTotal(), Info.GetWarningTotal());
	}
	return Failed == 0 ? 0 : 1;
#else
	UE_LOG(LogTemp, Error, TEXT("Special order tests require a development editor build."));
	return 1;
#endif
}
