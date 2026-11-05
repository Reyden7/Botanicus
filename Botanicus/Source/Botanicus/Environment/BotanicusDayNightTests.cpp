#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "BotanicusGameState.h"
#include "Environment/BotanicusAmbienceDirector.h"
#include "Environment/BotanicusDayNightMath.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusDayNightPhaseTest,
	"Botanicus.Lighting.SeasonalPhases", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBotanicusDayNightPhaseTest::RunTest(const FString& Parameters)
{
	for (const FVector2D Times : { FVector2D(360,1200), FVector2D(300,1260), FVector2D(420,1080), FVector2D(480,1020) })
	{
		const float Rise = Times.X, Set = Times.Y;
		TestTrue(TEXT("Sun rises on the horizon"), FMath::IsNearlyZero(BotanicusDayNight::Evaluate(Rise,Rise,Set).Daylight));
		TestTrue(TEXT("Sun sets on the horizon"), BotanicusDayNight::Evaluate(Set,Rise,Set).Daylight < 0.0001f);
		TestEqual(TEXT("No sun at 02:00"), BotanicusDayNight::Evaluate(120,Rise,Set).Daylight,0.0f);
		TestEqual(TEXT("No sun at 22:00"), BotanicusDayNight::Evaluate(1320,Rise,Set).Daylight,0.0f);
		TestTrue(TEXT("Midday sun is positive"), BotanicusDayNight::Evaluate(720,Rise,Set).Daylight > 0.8f);
		TestTrue(TEXT("Sky is dark at midnight"), BotanicusDayNight::Evaluate(0,Rise,Set).SkyBlend < 0.01f);
		TestTrue(TEXT("Continuous sky at sunrise"), FMath::Abs(BotanicusDayNight::Evaluate(Rise-0.01f,Rise,Set).SkyBlend-BotanicusDayNight::Evaluate(Rise+0.01f,Rise,Set).SkyBlend)<0.001f);
		TestTrue(TEXT("Continuous sky at sunset"), FMath::Abs(BotanicusDayNight::Evaluate(Set-0.01f,Rise,Set).SkyBlend-BotanicusDayNight::Evaluate(Set+0.01f,Rise,Set).SkyBlend)<0.001f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusDayNightSmoothingTest,
	"Botanicus.Lighting.ClockSampling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBotanicusDayNightSmoothingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Midnight delta is forward two minutes"), BotanicusDayNight::SignedMinuteDelta(1439,1),2.0f);
	const float Smoothed = BotanicusDayNight::SmoothMinutes(1439,1,0.1f,0.12f,15.0f);
	TestTrue(TEXT("Smooth across midnight without traversing midday"), Smoothed<2.0f || Smoothed>1439.0f);
	TestEqual(TEXT("Explicit hour jumps snap"), BotanicusDayNight::SmoothMinutes(720,1320,0.016f,0.12f,15),1320.0f);
	TestEqual(TEXT("No independent advance without a new sample"), BotanicusDayNight::SmoothMinutes(120,120,60,0.12f,15),120.0f);
	TestEqual(TEXT("Zero elapsed time holds"), BotanicusDayNight::SmoothMinutes(120,121,0,0.12f,15),120.0f);
	TestTrue(TEXT("Fast game time converges to supplied sample"), FMath::IsNearlyEqual(BotanicusDayNight::SmoothMinutes(120,125,10,0.12f,15),125.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotanicusDayNightIntegrationTest,
	"Botanicus.Lighting.GameStateAndLegacySky", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBotanicusDayNightIntegrationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	ABotanicusAmbienceDirector* Director = World->SpawnActor<ABotanicusAmbienceDirector>();
	Director->Sun = World->SpawnActor<ADirectionalLight>();
	Director->SkyLight = World->SpawnActor<ASkyLight>();
	Director->ExposureVolume = World->SpawnActor<APostProcessVolume>();
	UClass* SkyClass = LoadClass<AActor>(nullptr,TEXT("/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C"));
	TestNotNull(TEXT("Existing engine sky loads"),SkyClass);
	if (!SkyClass) return false;
	Director->LegacySkySphere = World->SpawnActor<AActor>(SkyClass);
	Director->RefreshFromGameState(1.0f); // GameState may arrive after the actor.
	ABotanicusGameState* State = World->SpawnActor<ABotanicusGameState>();
	World->SetGameState(State);
	State->SetDayTimeMinutesForDevelopment(1320.0f);
	const int32 OriginalFunds=State->GetSharedFunds();
	Director->RefreshFromGameState(0.0f);
	TestEqual(TEXT("Late initial sample is applied immediately"),Director->GetVisualMinutes(),1320.0f);
	TestEqual(TEXT("Night sun is off"),Director->Sun->GetLightComponent()->Intensity,0.0f);
	TestTrue(TEXT("Night sun below horizon"),Director->Sun->GetActorRotation().Pitch>0.0f);
	TestTrue(TEXT("Night ambient is limited"),Director->SkyLight->GetLightComponent()->Intensity<0.2f);
	FNumericProperty* SunHeight=FindFProperty<FNumericProperty>(SkyClass,TEXT("Sun height"));
	TestNotNull(TEXT("Legacy sky exposes sun height"),SunHeight);
	if (SunHeight) TestTrue(TEXT("Sky Blueprint updated to night"),SunHeight->GetFloatingPointPropertyValue(SunHeight->ContainerPtrToValuePtr<void>(Director->LegacySkySphere.Get()))<0.0);
	for (const float Hour : {6.0f,12.0f,18.0f,22.0f,2.0f})
	{
		State->SetDayTimeMinutesForDevelopment(Hour*60.0f);
		Director->RefreshFromGameState(0.1f);
		TestTrue(TEXT("Presentation follows server sample"), FMath::Abs(BotanicusDayNight::SignedMinuteDelta(Director->GetVisualMinutes(),Hour*60))<0.01f);
	}
	TestEqual(TEXT("Presentation does not spend funds"),State->GetSharedFunds(),OriginalFunds);
	// SetDayTime itself may advance the commercial calendar; presentation must not.
	const int32 DayBeforeRefresh=State->GetCurrentDayNumber();
	Director->RefreshFromGameState(10.0f);
	TestEqual(TEXT("Presentation never advances calendar"),State->GetCurrentDayNumber(),DayBeforeRefresh);
	Director->MinimumEV100=9.0f; Director->MaximumEV100=1.0f;
	Director->ApplyExposureSettings();
	TestTrue(TEXT("Reversed exposure bounds become ordered"),Director->ExposureVolume->Settings.AutoExposureMinBrightness<Director->ExposureVolume->Settings.AutoExposureMaxBrightness);
	TestFalse(TEXT("Director does not replicate"),Director->GetIsReplicated());
	TestFalse(TEXT("No dedicated server tick"),Director->PrimaryActorTick.bAllowTickOnDedicatedServer);
	World->SetGameState(nullptr);
	return true;
}
#endif
