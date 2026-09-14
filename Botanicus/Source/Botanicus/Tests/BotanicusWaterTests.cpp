// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Water/BotanicusWaterSettings.h"
#include "Water/BotanicusWaterSubsystem.h"
#include "Water/BotanicusWaterTrajectory.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusWaterTrajectoryMathTest,
	"Botanicus.Water.TrajectoryMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusWaterTrajectoryMathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FBotanicusWaterStreamParams Stream;
	Stream.Origin = FVector(10.0f, 20.0f, 30.0f);
	Stream.Direction = FVector::ForwardVector;
	Stream.InitialSpeed = 100.0f;
	Stream.Gravity = FVector(0.0f, 0.0f, -980.0f);

	TestTrue(
		TEXT("Position follows the shared ballistic equation"),
		BotanicusWaterTrajectory::PositionAtTime(Stream, 1.0f).
			Equals(FVector(110.0f, 20.0f, -460.0f), 0.001f));
	TestTrue(
		TEXT("Velocity follows the derivative of the shared equation"),
		BotanicusWaterTrajectory::VelocityAtTime(Stream, 1.0f).
			Equals(FVector(100.0f, 0.0f, -980.0f), 0.001f));

	FBotanicusWaterStreamParams RaisedStream = Stream;
	RaisedStream.Direction = FVector(1.0f, 0.0f, 0.45f).GetSafeNormal();
	FBotanicusWaterStreamParams LoweredStream = Stream;
	LoweredStream.Direction = FVector(1.0f, 0.0f, -0.45f).GetSafeNormal();
	TestTrue(
		TEXT("Raising the nozzle immediately raises the shared trajectory"),
		BotanicusWaterTrajectory::PositionAtTime(RaisedStream, 0.15f).Z >
			BotanicusWaterTrajectory::PositionAtTime(LoweredStream, 0.15f).Z);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusWaterAdaptiveSegmentsTest,
	"Botanicus.Water.AdaptiveSegments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusWaterAdaptiveSegmentsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FBotanicusWaterStreamParams ShortStream;
	ShortStream.InitialSpeed = 500.0f;
	ShortStream.Gravity = FVector(0.0f, 0.0f, -980.0f);
	ShortStream.MaxSimulationTime = 0.10f;
	ShortStream.MaxDistance = 1000.0f;
	ShortStream.TargetSegmentLength = 70.0f;
	ShortStream.MaxCurveDeviation = 2.0f;
	ShortStream.MaxTraceSegments = 24;

	FBotanicusWaterStreamParams LongStream = ShortStream;
	LongStream.MaxSimulationTime = 1.0f;

	TArray<FBotanicusWaterTrajectorySample> ShortSamples;
	TArray<FBotanicusWaterTrajectorySample> LongSamples;
	BotanicusWaterTrajectory::BuildSamples(ShortStream, ShortSamples);
	BotanicusWaterTrajectory::BuildSamples(LongStream, LongSamples);

	TestTrue(
		TEXT("A short stream uses fewer trace segments than a long stream"),
		ShortSamples.Num() < LongSamples.Num());
	TestTrue(
		TEXT("The configured trace budget is respected"),
		LongSamples.Num() <= LongStream.MaxTraceSegments + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusWaterSurfaceClassificationTest,
	"Botanicus.Water.SurfaceClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusWaterSurfaceClassificationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const UBotanicusWaterSettings* Settings =
		GetDefault<UBotanicusWaterSettings>();
	TestEqual(
		TEXT("An upward floor can accumulate a puddle"),
		BotanicusWaterSurface::ClassifySurface(FVector::UpVector, *Settings),
		EBotanicusWaterSurfaceClass::Horizontal);
	TestEqual(
		TEXT("A wall is classified as vertical"),
		BotanicusWaterSurface::ClassifySurface(FVector::ForwardVector, *Settings),
		EBotanicusWaterSurfaceClass::Vertical);
	const FBotanicusWaterReceiveResult WallResult =
		BotanicusWaterSurface::DistributeImpactWater(
			1.0f,
			FVector::ForwardVector,
			*Settings);
	TestFalse(TEXT("A wall cannot create a puddle"), WallResult.bCanCreatePuddle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBotanicusWaterConservationTest,
	"Botanicus.Water.Conservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBotanicusWaterConservationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UBotanicusWaterSettings* Settings =
		GetDefault<UBotanicusWaterSettings>();
	for (const FVector Normal : {
		FVector::UpVector,
		FVector(1.0f, 0.0f, 0.5f).GetSafeNormal(),
		FVector::ForwardVector})
	{
		const FBotanicusWaterReceiveResult Result =
			BotanicusWaterSurface::DistributeImpactWater(
				0.37f,
				Normal,
				*Settings);
		TestTrue(
			TEXT("Every emitted amount is distributed exactly once"),
			FMath::IsNearlyEqual(
				Result.Emitted,
				Result.GetDistributedTotal(),
				KINDA_SMALL_NUMBER));
	}
	return true;
}

#endif
