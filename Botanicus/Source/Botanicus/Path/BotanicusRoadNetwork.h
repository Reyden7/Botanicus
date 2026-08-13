// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ABotanicusVisitorZoneActor;
class UWorld;

/** Builds a navigable graph from every connected PNJ road spline. */
struct FBotanicusRoadNetwork
{
	static bool BuildVisitorCircuit(
		UWorld* World,
		const ABotanicusVisitorZoneActor* ParkingZone,
		const ABotanicusVisitorZoneActor* SalesZone,
		const ABotanicusVisitorZoneActor* CheckoutZone,
		TArray<FVector>& OutCircuit,
		int32& OutCheckoutWaypointIndex,
		TArray<FVector>& OutArrivalRoute,
		TArray<FVector>& OutDirectReturnRoute);
};
