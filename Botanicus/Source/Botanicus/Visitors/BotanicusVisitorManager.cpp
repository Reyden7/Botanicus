// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visitors/BotanicusVisitorManager.h"

#include "Algo/Reverse.h"
#include "BotanicusGameState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Path/BotanicusPathActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
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
	int32 CheckoutWaypointIndex = INDEX_NONE;
	if (!BuildVisitorCircuit(
			VisitorCircuit,
			CheckoutWaypointIndex))
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

	if (SpawnRemaining <= 0.0f &&
		TotalVisitorCount < GetTargetVisitorPopulation())
	{
		SpawnQueuedVisitor(
			VisitorCircuit,
			CheckoutWaypointIndex);
		SpawnRemaining =
			FMath::FRandRange(
				VisitorSpawnInterval * 0.7f,
				VisitorSpawnInterval * 1.35f) *
			GetReputationSpawnMultiplier();
	}
}

bool ABotanicusVisitorManager::BuildVisitorCircuit(
	TArray<FVector>& OutCircuit,
	int32& OutCheckoutWaypointIndex) const
{
	OutCheckoutWaypointIndex = INDEX_NONE;
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
	for (TActorIterator<ABotanicusPathActor> PathIt(World);
		 PathIt;
		 ++PathIt)
	{
		if (!PathIt->IsVisitorRoute() ||
			PathIt->IsPreviewPath())
		{
			continue;
		}
		TArray<FVector> CandidatePoints =
			PathIt->GetPathWorldPoints();
		if (CandidatePoints.Num() < 3)
		{
			continue;
		}
		const bool bFirstPointInParking =
			ParkingZone->ContainsPoint2D(CandidatePoints[0]);
		const bool bLastPointInParking =
			ParkingZone->ContainsPoint2D(CandidatePoints.Last());
		if (!bFirstPointInParking && bLastPointInParking)
		{
			Algo::Reverse(CandidatePoints);
		}
		if (!ParkingZone->ContainsPoint2D(CandidatePoints[0]) ||
			!ParkingZone->ContainsPoint2D(CandidatePoints.Last()))
		{
			continue;
		}

		bool bHasReachedSalesArea = false;
		int32 CandidateCheckoutIndex = INDEX_NONE;
		for (int32 PointIndex = 1;
			 PointIndex < CandidatePoints.Num() - 1;
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

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(BotanicusVisitorCircuitValidation),
			false);
		for (TActorIterator<ABotanicusPathActor> IgnoredPathIt(World);
			 IgnoredPathIt;
			 ++IgnoredPathIt)
		{
			QueryParams.AddIgnoredActor(*IgnoredPathIt);
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

void ABotanicusVisitorManager::SpawnQueuedVisitor(
	const TArray<FVector>& VisitorCircuit,
	int32 CheckoutWaypointIndex)
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
			QueueLocation);
		WaitingVisitors.Add(Visitor);
	}
}
