#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Audio/BotanicusAudioCatalog.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusAudioStagesTest,"Botanicus.Audio.StageCrossings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBotanicusAudioStagesTest::RunTest(const FString&)
{
	using Audio=UBotanicusAudioWorldSubsystem;
	TestTrue(TEXT("No stage while growth unchanged"),Audio::GrowthEvent(.4f,.4f).IsNone());
	TestTrue(TEXT("No stage when restoring backwards"),Audio::GrowthEvent(.8f,.2f).IsNone());
	TestEqual(TEXT("Young threshold"),Audio::GrowthEvent(.29f,.3f),FName("GrowthYoung"));
	TestEqual(TEXT("Large threshold"),Audio::GrowthEvent(.69f,.7f),FName("GrowthLarge"));
	TestEqual(TEXT("Maturity once"),Audio::GrowthEvent(.99f,1.f),FName("Mature"));
	TestTrue(TEXT("Mature does not repeat"),Audio::GrowthEvent(1.f,1.f).IsNone());
	TestEqual(TEXT("Skipped stages coalesce to highest"),Audio::GrowthEvent(.1f,1.f),FName("Mature"));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusAudioRegionTest,"Botanicus.Audio.RegionTransitions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBotanicusAudioRegionTest::RunTest(const FString&)
{
	FBotanicusAudioRegion Region; Region.Centre=FVector(100,200,300);Region.Extent=FVector(100,100,100);Region.BlendDistance=200;
	TestEqual(TEXT("Inside"),Region.WeightAt(Region.Centre),1.f);
	TestEqual(TEXT("Boundary"),Region.WeightAt(FVector(200,200,300)),1.f);
	TestEqual(TEXT("Soft boundary halfway"),Region.WeightAt(FVector(300,200,300)),.5f);
	TestEqual(TEXT("Outside"),Region.WeightAt(FVector(400,200,300)),0.f);
	TestEqual(TEXT("Vertical leakage excluded"),Region.WeightAt(FVector(100,200,700)),0.f);
	return true;
}
#endif
