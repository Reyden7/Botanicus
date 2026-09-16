// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visitors/BotanicusVisitorManager.h"

#include "Algo/Reverse.h"
#include "BotanicusGameState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Path/BotanicusPathActor.h"
#include "Path/BotanicusRoadNetwork.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visitors/BotanicusVisitorZoneActor.h"

ABotanicusVisitorManager::ABotanicusVisitorManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = false;
}

void ABotanicusVisitorManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	if (!IsShopOpen())
	{
		SendAllVisitorsHome();
		SpawnRemaining = 0.5f;
		AdmissionRemaining = 0.0f;
		return;
	}

	SpawnRemaining -= DeltaSeconds;
	AdmissionRemaining -= DeltaSeconds;

	TArray<FVector> VisitorCircuit;
	TArray<FVector> ArrivalRoute;
	TArray<FVector> DirectReturnRoute;
	int32 CheckoutWaypointIndex = INDEX_NONE;
	if (!BuildVisitorCircuit(
			VisitorCircuit,
			CheckoutWaypointIndex,
			ArrivalRoute,
			DirectReturnRoute))
	{
		return;
	}

	WaitingVisitors.RemoveAll(
		[](const TWeakObjectPtr<ABotanicusVisitorCharacter>& Visitor)
		{
			return !Visitor.IsValid() || !Visitor->IsQueued();
		});

	int32 TotalVisitorCount = 0;
	int32 VisitorsInside = 0;
	for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(
			 GetWorld());
		 VisitorIt;
		 ++VisitorIt)
	{
		++TotalVisitorCount;
		if (VisitorIt->OccupiesShopCapacity())
		{
			++VisitorsInside;
		}
	}

	const int32 ShopCapacity = GetShopVisitorCapacity();
	if (VisitorsInside < ShopCapacity &&
		WaitingVisitors.Num() > 0 &&
		AdmissionRemaining <= 0.0f)
	{
		TWeakObjectPtr<ABotanicusVisitorCharacter> NextVisitor =
			WaitingVisitors[0];
		WaitingVisitors.RemoveAt(0);
		if (NextVisitor.IsValid())
		{
			NextVisitor->AdmitFromQueue();
			++VisitorsInside;
			AdmissionRemaining =
				FMath::FRandRange(
					VisitorAdmissionInterval * 0.65f,
					VisitorAdmissionInterval * 1.45f);
		}
	}
	RefreshQueuePositions(VisitorCircuit);
	RefreshCheckoutQueue(
		VisitorCircuit,
		CheckoutWaypointIndex);

	if (SpawnRemaining <= 0.0f &&
		TotalVisitorCount < GetTargetVisitorPopulation())
	{
		SpawnQueuedVisitor(
			VisitorCircuit,
			CheckoutWaypointIndex,
			ArrivalRoute,
			DirectReturnRoute);
		SpawnRemaining =
			FMath::FRandRange(
				VisitorSpawnInterval * 0.7f,
				VisitorSpawnInterval * 1.35f) *
			GetReputationSpawnMultiplier();
	}
}

bool ABotanicusVisitorManager::BuildVisitorCircuit(
	TArray<FVector>& OutCircuit,
	int32& OutCheckoutWaypointIndex,
	TArray<FVector>& OutArrivalRoute,
	TArray<FVector>& OutDirectReturnRoute) const
{
	OutCheckoutWaypointIndex = INDEX_NONE;
	OutArrivalRoute.Reset();
	OutDirectReturnRoute.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	ABotanicusVisitorZoneActor* ParkingZone = nullptr;
	ABotanicusVisitorZoneActor* SalesZone = nullptr;
	ABotanicusVisitorZoneActor* CheckoutZone = nullptr;
	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		switch (ZoneIt->GetZoneType())
		{
		case EBotanicusVisitorZoneType::Parking:
			ParkingZone = *ZoneIt;
			break;
		case EBotanicusVisitorZoneType::SalesArea:
			SalesZone = *ZoneIt;
			break;
		case EBotanicusVisitorZoneType::Checkout:
			CheckoutZone = *ZoneIt;
			break;
		}
	}
	if (!ParkingZone || !SalesZone || !CheckoutZone)
	{
		return false;
	}

	// The graph understands spline intersections, T junctions and routes
	// composed of any number of independently created road actors.
	if (FBotanicusRoadNetwork::BuildVisitorCircuit(
			World,
			ParkingZone,
			SalesZone,
			CheckoutZone,
			OutCircuit,
			OutCheckoutWaypointIndex,
			OutArrivalRoute,
			OutDirectReturnRoute))
	{
		return true;
	}

	const auto FindZonePointOnSegment =
		[](const ABotanicusVisitorZoneActor* Zone,
			const FVector& SegmentStart,
			const FVector& SegmentEnd,
			FVector& OutZonePoint)
		{
			if (Zone->ContainsPoint2D(SegmentEnd))
			{
				OutZonePoint = SegmentEnd;
				return true;
			}

			const FVector2D Start2D(SegmentStart);
			const FVector2D End2D(SegmentEnd);
			const FVector2D Segment = End2D - Start2D;
			const float SegmentSizeSquared = Segment.SizeSquared();
			const float Alpha =
				SegmentSizeSquared > UE_SMALL_NUMBER
					? FMath::Clamp(
						FVector2D::DotProduct(
							FVector2D(Zone->GetActorLocation()) -
								Start2D,
							Segment) /
							SegmentSizeSquared,
						0.0f,
						1.0f)
					: 0.0f;
			OutZonePoint =
				FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
			return Zone->ContainsPoint2D(OutZonePoint);
		};

	float LongestRouteLength = 0.0f;
	TArray<TArray<FVector>> VisitorRouteSegments;
	for (TActorIterator<ABotanicusPathActor> PathIt(World);
		 PathIt;
		 ++PathIt)
	{
		if (!PathIt->IsVisitorRoute() ||
			PathIt->IsPreviewPath())
		{
			continue;
		}
		TArray<FVector> SegmentPoints = PathIt->GetPathWorldPoints();
		if (SegmentPoints.Num() >= 2)
		{
			VisitorRouteSegments.Add(MoveTemp(SegmentPoints));
		}
	}

	constexpr float RouteConnectionTolerance = 275.0f;
	const float RouteConnectionToleranceSquared =
		FMath::Square(RouteConnectionTolerance);

	const auto AdvanceZoneProgress =
		[&](const TArray<FVector>& Points,
			bool bAlreadyReachedSales,
			bool& bOutReachedSales,
			bool& bOutReachedCheckout)
		{
			bOutReachedSales = bAlreadyReachedSales;
			bOutReachedCheckout = false;
			for (int32 PointIndex = 1;
				 PointIndex < Points.Num();
				 ++PointIndex)
			{
				FVector CrossingPoint;
				if (FindZonePointOnSegment(
						SalesZone,
						Points[PointIndex - 1],
						Points[PointIndex],
						CrossingPoint))
				{
					bOutReachedSales = true;
				}
				if (bOutReachedSales &&
					FindZonePointOnSegment(
						CheckoutZone,
						Points[PointIndex - 1],
						Points[PointIndex],
						CrossingPoint))
				{
					bOutReachedCheckout = true;
					return;
				}
			}
		};

	TFunction<bool(
		const TArray<FVector>&,
		bool,
		TSet<int32>&,
		TArray<FVector>&)> FindRouteThroughNetwork;
	FindRouteThroughNetwork =
		[&](const TArray<FVector>& CurrentPoints,
			bool bReachedSales,
			TSet<int32>& VisitedTraversalStates,
			TArray<FVector>& OutFoundPoints)
		{
			for (int32 SegmentIndex = 0;
				 SegmentIndex < VisitorRouteSegments.Num();
				 ++SegmentIndex)
			{
				const TArray<FVector>& Segment =
					VisitorRouteSegments[SegmentIndex];
				for (int32 Orientation = 0;
					 Orientation < 2;
					 ++Orientation)
				{
					const bool bReverseSegment = Orientation == 1;
					const FVector& ConnectionPoint =
						bReverseSegment ? Segment.Last() : Segment[0];
					if (FVector::DistSquared2D(
							CurrentPoints.Last(), ConnectionPoint) >
						RouteConnectionToleranceSquared)
					{
						continue;
					}

					// Keep direction and visit phase in the state. The same
					// physical segment may therefore be used backwards after
					// the shop, which is required by Y-shaped route networks.
					const int32 TraversalState =
						SegmentIndex * 4 + Orientation * 2 +
						(bReachedSales ? 1 : 0);
					if (VisitedTraversalStates.Contains(TraversalState))
					{
						continue;
					}

					TArray<FVector> OrientedSegment = Segment;
					if (bReverseSegment)
					{
						Algo::Reverse(OrientedSegment);
					}
					TArray<FVector> NextPoints = CurrentPoints;
					for (int32 PointIndex = 1;
						 PointIndex < OrientedSegment.Num();
						 ++PointIndex)
					{
						NextPoints.Add(OrientedSegment[PointIndex]);
					}

					bool bNextReachedSales = false;
					bool bNextReachedCheckout = false;
					AdvanceZoneProgress(
						OrientedSegment,
						bReachedSales,
						bNextReachedSales,
						bNextReachedCheckout);
					if (bNextReachedCheckout)
					{
						OutFoundPoints = MoveTemp(NextPoints);
						return true;
					}

					VisitedTraversalStates.Add(TraversalState);
					if (FindRouteThroughNetwork(
							NextPoints,
							bNextReachedSales,
							VisitedTraversalStates,
							OutFoundPoints))
					{
						return true;
					}
					VisitedTraversalStates.Remove(TraversalState);
				}
			}
			return false;
		};

	for (int32 StartSegmentIndex = 0;
		 StartSegmentIndex < VisitorRouteSegments.Num();
		 ++StartSegmentIndex)
	{
		TArray<FVector> CandidatePoints =
			VisitorRouteSegments[StartSegmentIndex];
		const bool bStartsInParking =
			ParkingZone->ContainsPoint2D(CandidatePoints[0]);
		const bool bEndsInParking =
			ParkingZone->ContainsPoint2D(CandidatePoints.Last());
		if (!bStartsInParking && !bEndsInParking)
		{
			continue;
		}
		if (!bStartsInParking)
		{
			Algo::Reverse(CandidatePoints);
		}

		bool bReachedSalesOnStart = false;
		bool bReachedCheckoutOnStart = false;
		AdvanceZoneProgress(
			CandidatePoints,
			false,
			bReachedSalesOnStart,
			bReachedCheckoutOnStart);
		if (!bReachedCheckoutOnStart)
		{
			const bool bStartWasReversed = !bStartsInParking;
			TSet<int32> VisitedTraversalStates;
			VisitedTraversalStates.Add(
				StartSegmentIndex * 4 +
				(bStartWasReversed ? 2 : 0));
			TArray<FVector> ConnectedRoute;
			if (!FindRouteThroughNetwork(
					CandidatePoints,
					bReachedSalesOnStart,
					VisitedTraversalStates,
					ConnectedRoute))
			{
				continue;
			}
			CandidatePoints = MoveTemp(ConnectedRoute);
		}

		bool bHasReachedSalesArea = false;
		int32 CandidateCheckoutIndex = INDEX_NONE;
		for (int32 PointIndex = 1;
			 PointIndex < CandidatePoints.Num();
			 ++PointIndex)
		{
			FVector SalesCrossingPoint;
			if (FindZonePointOnSegment(
					SalesZone,
					CandidatePoints[PointIndex - 1],
					CandidatePoints[PointIndex],
					SalesCrossingPoint))
			{
				bHasReachedSalesArea = true;
			}
			FVector CheckoutCrossingPoint;
			if (bHasReachedSalesArea &&
				FindZonePointOnSegment(
					CheckoutZone,
					CandidatePoints[PointIndex - 1],
					CandidatePoints[PointIndex],
					CheckoutCrossingPoint))
			{
				if (CheckoutZone->ContainsPoint2D(
						CandidatePoints[PointIndex]))
				{
					CandidateCheckoutIndex = PointIndex;
				}
				else
				{
					CandidatePoints.Insert(
						CheckoutCrossingPoint,
						PointIndex);
					CandidateCheckoutIndex = PointIndex;
				}
				break;
			}
		}
		if (CandidateCheckoutIndex == INDEX_NONE)
		{
			continue;
		}

		// A set of connected Blueprint segments only needs to reach the
		// checkout. Visitors follow the same route backwards to return to
		// the parking, so level designers do not need to duplicate it.
		if (!ParkingZone->ContainsPoint2D(CandidatePoints.Last()))
		{
			// UE 5.8 rejects Add(Array[Index]) because Add may reallocate the
			// same array and invalidate the referenced element. Copy the point
			// to independent storage before growing CandidatePoints.
			for (int32 PointIndex = CandidatePoints.Num() - 2;
				 PointIndex >= 0;
				 --PointIndex)
			{
				const FVector ReturnPoint = CandidatePoints[PointIndex];
				CandidatePoints.Add(ReturnPoint);
			}
		}

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(BotanicusVisitorCircuitValidation),
			false);
		for (TActorIterator<ABotanicusPathActor> IgnoredPathIt(World);
			 IgnoredPathIt;
			 ++IgnoredPathIt)
		{
			QueryParams.AddIgnoredActor(*IgnoredPathIt);
		}
		for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(World);
			 VisitorIt;
			 ++VisitorIt)
		{
			QueryParams.AddIgnoredActor(*VisitorIt);
		}
		for (TActorIterator<ABotanicusCashRegisterActor> RegisterIt(
				 World);
			 RegisterIt;
			 ++RegisterIt)
		{
			QueryParams.AddIgnoredActor(*RegisterIt);
		}
		bool bRouteBlocked = false;
		for (int32 PointIndex = 1;
			 PointIndex < CandidatePoints.Num();
			 ++PointIndex)
		{
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(
					Hit,
					CandidatePoints[PointIndex - 1] +
						FVector(0.0f, 0.0f, 80.0f),
					CandidatePoints[PointIndex] +
						FVector(0.0f, 0.0f, 80.0f),
					ECC_Visibility,
					QueryParams))
			{
				bRouteBlocked = true;
				break;
			}
		}
		if (bRouteBlocked)
		{
			continue;
		}

		float CandidateLength = 0.0f;
		for (int32 PointIndex = 1;
			 PointIndex < CandidatePoints.Num();
			 ++PointIndex)
		{
			CandidateLength += FVector::Dist2D(
				CandidatePoints[PointIndex - 1],
				CandidatePoints[PointIndex]);
		}
		if (CandidateLength > LongestRouteLength)
		{
			LongestRouteLength = CandidateLength;
			OutCircuit = MoveTemp(CandidatePoints);
			OutCheckoutWaypointIndex = CandidateCheckoutIndex;
		}
	}
	if (OutCircuit.Num() < 3 ||
		OutCheckoutWaypointIndex == INDEX_NONE)
	{
		return false;
	}

	for (FVector& Point : OutCircuit)
	{
		Point.Z += 84.0f;
	}
	return true;
}

int32 ABotanicusVisitorManager::GetShopVisitorCapacity() const
{
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	return GameState
		? GameState->GetMainShopVisitorCapacity()
		: 4;
}

int32 ABotanicusVisitorManager::GetTargetVisitorPopulation() const
{
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	return GameState
		? GameState->GetTargetVisitorPopulation()
		: 16;
}

float ABotanicusVisitorManager::GetReputationSpawnMultiplier() const
{
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const float ReputationAlpha =
		GameState
			? FMath::Clamp(
				(GameState->GetShopReputationStars() - 1.0f) /
					4.0f,
				0.0f,
				1.0f)
			: 0.5f;
	return FMath::Lerp(1.20f, 0.65f, ReputationAlpha);
}

bool ABotanicusVisitorManager::IsShopOpen() const
{
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	return GameState ? GameState->IsMainShopOpen() : true;
}

void ABotanicusVisitorManager::SendAllVisitorsHome()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(World);
		 VisitorIt;
		 ++VisitorIt)
	{
		VisitorIt->BeginShopClosureDeparture();
	}
	WaitingVisitors.Reset();
}

void ABotanicusVisitorManager::RefreshQueuePositions(
	const TArray<FVector>& VisitorCircuit)
{
	if (VisitorCircuit.Num() < 2)
	{
		return;
	}
	FVector QueueDirection =
		VisitorCircuit[0] - VisitorCircuit[1];
	QueueDirection.Z = 0.0f;
	QueueDirection = QueueDirection.GetSafeNormal();
	if (QueueDirection.IsNearlyZero())
	{
		QueueDirection = FVector::BackwardVector;
	}

	ABotanicusVisitorZoneActor* ParkingZone = nullptr;
	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(GetWorld());
		 ZoneIt;
		 ++ZoneIt)
	{
		if (ZoneIt->GetZoneType() ==
			EBotanicusVisitorZoneType::Parking)
		{
			ParkingZone = *ZoneIt;
			break;
		}
	}
	const int32 VisibleQueueLength = 6;
	const int32 ParkingVisitorCount =
		FMath::Max(0, WaitingVisitors.Num() - VisibleQueueLength);
	const FVector ParkingExtent =
		ParkingZone
			? ParkingZone->GetZoneExtent()
			: FVector::ZeroVector;
	const float ParkingAspect =
		ParkingExtent.Y > 1.0f
			? ParkingExtent.X / ParkingExtent.Y
			: 1.0f;
	const int32 ParkingColumnCount =
		FMath::Max(
			1,
			FMath::CeilToInt(
				FMath::Sqrt(
					ParkingVisitorCount *
					FMath::Max(0.25f, ParkingAspect))));
	const int32 ParkingRowCount =
		FMath::Max(
			1,
			FMath::CeilToInt(
				static_cast<float>(ParkingVisitorCount) /
				ParkingColumnCount));
	for (int32 QueueIndex = 0;
		 QueueIndex < WaitingVisitors.Num();
		 ++QueueIndex)
	{
		if (WaitingVisitors[QueueIndex].IsValid())
		{
			FVector Destination;
			if (QueueIndex < VisibleQueueLength || !ParkingZone)
			{
				Destination =
					VisitorCircuit[0] +
					QueueDirection *
						(110.0f + QueueIndex * 95.0f);
			}
			else
			{
				const int32 ParkingIndex =
					QueueIndex - VisibleQueueLength;
				const int32 Column =
					ParkingIndex % ParkingColumnCount;
				const int32 Row =
					ParkingIndex / ParkingColumnCount;
				const float XAlpha =
					(Column + 0.5f) / ParkingColumnCount;
				const float YAlpha =
					(Row + 0.5f) / ParkingRowCount;
				const FVector LocalOffset(
					FMath::Lerp(
						-ParkingExtent.X + 70.0f,
						ParkingExtent.X - 70.0f,
						XAlpha),
					FMath::Lerp(
						-ParkingExtent.Y + 70.0f,
						ParkingExtent.Y - 70.0f,
						YAlpha),
					84.0f);
				Destination =
					ParkingZone->GetActorTransform().
						TransformPosition(LocalOffset);
			}
			WaitingVisitors[QueueIndex]->SetQueueDestination(
				WaitingVisitors[QueueIndex]->
					GetVariedQueueDestination(
						Destination,
						QueueDirection));
		}
	}
}

void ABotanicusVisitorManager::RefreshCheckoutQueue(
	const TArray<FVector>& VisitorCircuit,
	int32 CheckoutWaypointIndex)
{
	if (!GetWorld() ||
		!VisitorCircuit.IsValidIndex(CheckoutWaypointIndex) ||
		CheckoutWaypointIndex <= 0)
	{
		return;
	}

	TArray<ABotanicusVisitorCharacter*> WaitingForCheckout;
	for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(
			 GetWorld());
		 VisitorIt;
		 ++VisitorIt)
	{
		if (VisitorIt->IsWaitingForCheckoutAssignment())
		{
			WaitingForCheckout.Add(*VisitorIt);
		}
	}
	WaitingForCheckout.Sort(
		[](const ABotanicusVisitorCharacter& Left,
		   const ABotanicusVisitorCharacter& Right)
		{
			return Left.GetCheckoutQueueArrivalTime() <
				Right.GetCheckoutQueueArrivalTime();
		});

	for (TActorIterator<ABotanicusSelfCheckoutActor> CheckoutIt(
			 GetWorld());
		 CheckoutIt && WaitingForCheckout.Num() > 0;
		 ++CheckoutIt)
	{
		if (!CheckoutIt->IsOperational() ||
			CheckoutIt->GetAssignedVisitor())
		{
			continue;
		}
		for (int32 VisitorIndex = 0;
			 VisitorIndex < WaitingForCheckout.Num();
			 ++VisitorIndex)
		{
			if (WaitingForCheckout[VisitorIndex]->
					AssignSelfCheckout(*CheckoutIt))
			{
				WaitingForCheckout.RemoveAt(VisitorIndex);
				break;
			}
		}
	}

	TArray<ABotanicusVisitorCharacter*> CheckoutVisitors;
	for (TActorIterator<ABotanicusVisitorCharacter> VisitorIt(
			 GetWorld());
		 VisitorIt;
		 ++VisitorIt)
	{
		if (VisitorIt->IsInCheckoutQueue())
		{
			CheckoutVisitors.Add(*VisitorIt);
		}
	}
	CheckoutVisitors.Sort(
		[](const ABotanicusVisitorCharacter& Left,
		   const ABotanicusVisitorCharacter& Right)
		{
			return Left.GetCheckoutQueueArrivalTime() <
				Right.GetCheckoutQueueArrivalTime();
		});

	FVector QueueDirection =
		VisitorCircuit[CheckoutWaypointIndex - 1] -
		VisitorCircuit[CheckoutWaypointIndex];
	QueueDirection.Z = 0.0f;
	QueueDirection = QueueDirection.GetSafeNormal();
	if (QueueDirection.IsNearlyZero())
	{
		QueueDirection = FVector::BackwardVector;
	}

	FVector CheckoutLocation =
		VisitorCircuit[CheckoutWaypointIndex];
	float BestRegisterDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ABotanicusCashRegisterActor> RegisterIt(
			 GetWorld());
		 RegisterIt;
		 ++RegisterIt)
	{
		if (RegisterIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}
		if (!RegisterIt->IsOperational())
		{
			continue;
		}
		const FVector CandidateLocation =
			RegisterIt->GetCustomerStandLocation();
		const float DistanceSquared = FVector::DistSquared2D(
			CandidateLocation,
			VisitorCircuit[CheckoutWaypointIndex]);
		if (DistanceSquared < BestRegisterDistanceSquared)
		{
			BestRegisterDistanceSquared = DistanceSquared;
			CheckoutLocation = CandidateLocation;
		}
	}
	for (int32 QueueIndex = 0;
		 QueueIndex < CheckoutVisitors.Num();
		 ++QueueIndex)
	{
		const float SideOffset =
			QueueIndex > 0
				? (QueueIndex % 2 == 0 ? -12.0f : 12.0f)
				: 0.0f;
		const FVector QueueRight(
			-QueueDirection.Y,
			QueueDirection.X,
			0.0f);
		CheckoutVisitors[QueueIndex]->ConfigureCheckoutQueue(
			CheckoutLocation +
				QueueDirection * (QueueIndex * 125.0f) +
				QueueRight * SideOffset,
			QueueIndex == 0);
	}
}

void ABotanicusVisitorManager::SpawnQueuedVisitor(
	const TArray<FVector>& VisitorCircuit,
	int32 CheckoutWaypointIndex,
	const TArray<FVector>& ArrivalRoute,
	const TArray<FVector>& DirectReturnRoute)
{
	UWorld* World = GetWorld();
	if (!World ||
		VisitorCircuit.Num() < 2 ||
		!VisitorCircuit.IsValidIndex(CheckoutWaypointIndex))
	{
		return;
	}

	FVector QueueDirection =
		VisitorCircuit[0] - VisitorCircuit[1];
	QueueDirection.Z = 0.0f;
	QueueDirection = QueueDirection.GetSafeNormal();
	if (QueueDirection.IsNearlyZero())
	{
		QueueDirection = FVector::BackwardVector;
	}
	FVector QueueLocation =
		VisitorCircuit[0] +
		QueueDirection *
			(110.0f + WaitingVisitors.Num() * 95.0f);
	if (WaitingVisitors.Num() >= 6)
	{
		for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
			 ZoneIt;
			 ++ZoneIt)
		{
			if (ZoneIt->GetZoneType() ==
				EBotanicusVisitorZoneType::Parking)
			{
				QueueLocation =
					ZoneIt->GetActorLocation() +
					FVector(0.0f, 0.0f, 84.0f);
				break;
			}
		}
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusVisitorCharacter* Visitor =
		World->SpawnActor<ABotanicusVisitorCharacter>(
			ABotanicusVisitorCharacter::StaticClass(),
			QueueLocation,
			(VisitorCircuit[1] - VisitorCircuit[0]).Rotation(),
			SpawnParameters);
	if (Visitor)
	{
		Visitor->InitializeQueuedCircuit(
			VisitorCircuit,
			CheckoutWaypointIndex,
			QueueLocation,
			ArrivalRoute,
			DirectReturnRoute);
		WaitingVisitors.Add(Visitor);
	}
}
