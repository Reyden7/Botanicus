// Copyright Epic Games, Inc. All Rights Reserved.

#include "Path/BotanicusRoadNetwork.h"

#include "Algo/Reverse.h"
#include "EngineUtils.h"
#include "Path/BotanicusPathActor.h"
#include "Visitors/BotanicusVisitorZoneActor.h"

namespace
{
	struct FRoadNode
	{
		FVector Position = FVector::ZeroVector;
		TArray<int32> Neighbours;
		int32 PathIndex = INDEX_NONE;
	};

	void AddUniqueEdge(TArray<FRoadNode>& Nodes, int32 A, int32 B)
	{
		if (A == B || !Nodes.IsValidIndex(A) || !Nodes.IsValidIndex(B))
		{
			return;
		}
		Nodes[A].Neighbours.AddUnique(B);
		Nodes[B].Neighbours.AddUnique(A);
	}

	FVector ClosestPointOnSegment2D(
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd)
	{
		const FVector2D Start2D(SegmentStart);
		const FVector2D Segment2D =
			FVector2D(SegmentEnd) - Start2D;
		const float SegmentLengthSquared = Segment2D.SizeSquared();
		const float Alpha =
			SegmentLengthSquared > UE_SMALL_NUMBER
				? FMath::Clamp(
					FVector2D::DotProduct(
						FVector2D(Point) - Start2D,
						Segment2D) /
						SegmentLengthSquared,
					0.0f,
					1.0f)
				: 0.0f;
		return FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
	}

	void ConnectPathEndpointsToNeighbouringSegments(
		TArray<FRoadNode>& Nodes,
		const TArray<TArray<int32>>& PathNodeIndices)
	{
		// A road surface can visibly touch another road even when their spline
		// centres do not share an identical control point. Explicitly project
		// both ends of every independently drawn road onto all neighbouring
		// road segments. This creates real T junctions and lets a player resume
		// drawing from an existing road without having to redraw the full route.
		constexpr float EndpointJoinDistance = 360.0f;
		const float EndpointJoinDistanceSquared =
			FMath::Square(EndpointJoinDistance);

		for (int32 SourcePathIndex = 0;
			 SourcePathIndex < PathNodeIndices.Num();
			 ++SourcePathIndex)
		{
			const TArray<int32>& SourcePath =
				PathNodeIndices[SourcePathIndex];
			if (SourcePath.Num() < 2)
			{
				continue;
			}

			const int32 Endpoints[2] =
			{
				SourcePath[0],
				SourcePath.Last()
			};
			for (const int32 EndpointNode : Endpoints)
			{
				for (int32 TargetPathIndex = 0;
					 TargetPathIndex < PathNodeIndices.Num();
					 ++TargetPathIndex)
				{
					if (TargetPathIndex == SourcePathIndex)
					{
						continue;
					}

					const TArray<int32>& TargetPath =
						PathNodeIndices[TargetPathIndex];
					float BestDistanceSquared =
						EndpointJoinDistanceSquared;
					int32 BestSegmentStart = INDEX_NONE;
					int32 BestSegmentEnd = INDEX_NONE;
					for (int32 SegmentIndex = 1;
						 SegmentIndex < TargetPath.Num();
						 ++SegmentIndex)
					{
						const int32 SegmentStart =
							TargetPath[SegmentIndex - 1];
						const int32 SegmentEnd =
							TargetPath[SegmentIndex];
						const FVector ClosestPoint =
							ClosestPointOnSegment2D(
								Nodes[EndpointNode].Position,
								Nodes[SegmentStart].Position,
								Nodes[SegmentEnd].Position);
						const float DistanceSquared =
							FVector::DistSquared2D(
								Nodes[EndpointNode].Position,
								ClosestPoint);
						if (DistanceSquared <= BestDistanceSquared)
						{
							BestDistanceSquared = DistanceSquared;
							BestSegmentStart = SegmentStart;
							BestSegmentEnd = SegmentEnd;
						}
					}

					if (BestSegmentStart != INDEX_NONE)
					{
						// Joining both ends of the target edge is equivalent to
						// splitting it at the projected junction, while keeping the
						// graph compact and the rendered splines untouched.
						AddUniqueEdge(
							Nodes, EndpointNode, BestSegmentStart);
						AddUniqueEdge(
							Nodes, EndpointNode, BestSegmentEnd);
					}
				}
			}
		}
	}

	int32 AddZoneHub(
		TArray<FRoadNode>& Nodes,
		const ABotanicusVisitorZoneActor* Zone,
		const TArray<int32>& AnchorNodes,
		int32 HubPathIndex)
	{
		if (!Zone || AnchorNodes.IsEmpty())
		{
			return INDEX_NONE;
		}

		FVector HubPosition = Zone->GetActorLocation();
		float AverageHeight = 0.0f;
		int32 ValidAnchorCount = 0;
		for (const int32 AnchorNode : AnchorNodes)
		{
			if (Nodes.IsValidIndex(AnchorNode))
			{
				AverageHeight += Nodes[AnchorNode].Position.Z;
				++ValidAnchorCount;
			}
		}
		if (ValidAnchorCount == 0)
		{
			return INDEX_NONE;
		}
		HubPosition.Z = AverageHeight / ValidAnchorCount;

		const int32 HubNode =
			Nodes.Add({HubPosition, {}, HubPathIndex});
		for (const int32 AnchorNode : AnchorNodes)
		{
			AddUniqueEdge(Nodes, HubNode, AnchorNode);
		}
		return HubNode;
	}

	bool FindShortestPath(
		const TArray<FRoadNode>& Nodes,
		const TArray<int32>& Starts,
		const TSet<int32>& Goals,
		TArray<int32>& OutPath,
		const TSet<int32>* ForbiddenNodes = nullptr)
	{
		OutPath.Reset();
		if (Starts.IsEmpty() || Goals.IsEmpty())
		{
			return false;
		}

		TArray<float> Distances;
		Distances.Init(TNumericLimits<float>::Max(), Nodes.Num());
		TArray<int32> Previous;
		Previous.Init(INDEX_NONE, Nodes.Num());
		TSet<int32> Open;
		for (const int32 Start : Starts)
		{
			if (Nodes.IsValidIndex(Start) &&
				(!ForbiddenNodes || !ForbiddenNodes->Contains(Start)))
			{
				Distances[Start] = 0.0f;
				Open.Add(Start);
			}
		}

		int32 ReachedGoal = INDEX_NONE;
		while (!Open.IsEmpty())
		{
			int32 Current = INDEX_NONE;
			float BestDistance = TNumericLimits<float>::Max();
			for (const int32 Candidate : Open)
			{
				if (Distances[Candidate] < BestDistance)
				{
					BestDistance = Distances[Candidate];
					Current = Candidate;
				}
			}
			Open.Remove(Current);
			if (Goals.Contains(Current))
			{
				ReachedGoal = Current;
				break;
			}

			for (const int32 Neighbour : Nodes[Current].Neighbours)
			{
				if (ForbiddenNodes &&
					ForbiddenNodes->Contains(Neighbour) &&
					!Goals.Contains(Neighbour))
				{
					continue;
				}
				const float NewDistance = Distances[Current] +
					FVector::Dist2D(
						Nodes[Current].Position,
						Nodes[Neighbour].Position);
				if (NewDistance < Distances[Neighbour])
				{
					Distances[Neighbour] = NewDistance;
					Previous[Neighbour] = Current;
					Open.Add(Neighbour);
				}
			}
		}

		if (ReachedGoal == INDEX_NONE)
		{
			return false;
		}
		for (int32 Node = ReachedGoal;
			 Node != INDEX_NONE;
			 Node = Previous[Node])
		{
			OutPath.Add(Node);
		}
		Algo::Reverse(OutPath);
		return true;
	}
}

bool FBotanicusRoadNetwork::BuildVisitorCircuit(
	UWorld* World,
	const ABotanicusVisitorZoneActor* ParkingZone,
	const ABotanicusVisitorZoneActor* SalesZone,
	const ABotanicusVisitorZoneActor* CheckoutZone,
	TArray<FVector>& OutCircuit,
	int32& OutCheckoutWaypointIndex,
	TArray<FVector>& OutArrivalRoute,
	TArray<FVector>& OutDirectReturnRoute)
{
	OutCircuit.Reset();
	OutArrivalRoute.Reset();
	OutDirectReturnRoute.Reset();
	OutCheckoutWaypointIndex = INDEX_NONE;
	if (!World || !ParkingZone || !SalesZone || !CheckoutZone)
	{
		return false;
	}

	TArray<FRoadNode> Nodes;
	TArray<TArray<int32>> PathNodeIndices;
	for (TActorIterator<ABotanicusPathActor> It(World); It; ++It)
	{
		if (!It->IsVisitorRoute() || It->IsPreviewPath())
		{
			continue;
		}
		const TArray<FVector> Samples =
			It->GetNavigationWorldPoints(100.0f);
		if (Samples.Num() < 2)
		{
			continue;
		}
		const int32 PathIndex = PathNodeIndices.Num();
		TArray<int32>& Indices = PathNodeIndices.AddDefaulted_GetRef();
		for (const FVector& Sample : Samples)
		{
			Indices.Add(Nodes.Add({Sample, {}, PathIndex}));
		}
		for (int32 Index = 1; Index < Indices.Num(); ++Index)
		{
			AddUniqueEdge(Nodes, Indices[Index - 1], Indices[Index]);
		}
	}

	if (Nodes.Num() < 2)
	{
		return false;
	}

	ConnectPathEndpointsToNeighbouringSegments(Nodes, PathNodeIndices);

	// Densified samples also make crossings through the middle of two
	// splines real graph junctions. Only compare different road actors:
	// consecutive points of one spline are already linked above.
	constexpr float JunctionDistanceSquared = 140.0f * 140.0f;
	for (int32 A = 0; A < Nodes.Num(); ++A)
	{
		for (int32 B = A + 1; B < Nodes.Num(); ++B)
		{
			if (Nodes[A].PathIndex != Nodes[B].PathIndex &&
				FVector::DistSquared2D(
					Nodes[A].Position,
					Nodes[B].Position) <= JunctionDistanceSquared)
			{
				AddUniqueEdge(Nodes, A, B);
			}
		}
	}

	TArray<int32> ParkingAnchorNodes;
	TArray<int32> SalesAnchorNodes;
	TArray<int32> CheckoutAnchorNodes;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (ParkingZone->ContainsPoint2D(Nodes[Index].Position))
		{
			ParkingAnchorNodes.Add(Index);
		}
		if (SalesZone->ContainsPoint2D(Nodes[Index].Position))
		{
			SalesAnchorNodes.Add(Index);
		}
		if (CheckoutZone->ContainsPoint2D(Nodes[Index].Position))
		{
			CheckoutAnchorNodes.Add(Index);
		}
	}

	// Each visitor zone is a virtual piece of road. Every spline sample that
	// enters the zone becomes an automatic anchor, and the invisible hub joins
	// all those anchors together. Roads may consequently enter and leave a
	// parking, sales or checkout zone through different independently drawn
	// splines without requiring one monolithic loop.
	const int32 ParkingHub = AddZoneHub(
		Nodes, ParkingZone, ParkingAnchorNodes, -2);
	const int32 SalesHub = AddZoneHub(
		Nodes, SalesZone, SalesAnchorNodes, -3);
	const int32 CheckoutHub = AddZoneHub(
		Nodes, CheckoutZone, CheckoutAnchorNodes, -4);
	if (ParkingHub == INDEX_NONE ||
		SalesHub == INDEX_NONE ||
		CheckoutHub == INDEX_NONE)
	{
		return false;
	}

	const TArray<int32> ParkingStarts = {ParkingHub};
	const TSet<int32> ParkingGoals = {ParkingHub};
	const TSet<int32> SalesGoals = {SalesHub};
	TSet<int32> CheckoutForbiddenNodes(CheckoutAnchorNodes);
	CheckoutForbiddenNodes.Add(CheckoutHub);
	const TSet<int32> CheckoutGoals = {CheckoutHub};

	TArray<int32> ToSales;
	if (!FindShortestPath(
			Nodes,
			ParkingStarts,
			SalesGoals,
			ToSales,
			&CheckoutForbiddenNodes))
	{
		return false;
	}
	TArray<int32> SalesStart = {ToSales.Last()};
	TArray<int32> DirectReturn;
	if (!FindShortestPath(
			Nodes,
			SalesStart,
			ParkingGoals,
			DirectReturn,
			&CheckoutForbiddenNodes))
	{
		return false;
	}
	TArray<int32> ToCheckout;
	if (!FindShortestPath(
			Nodes,
			SalesStart,
			CheckoutGoals,
			ToCheckout))
	{
		return false;
	}
	TArray<int32> CheckoutStart = {ToCheckout.Last()};
	TArray<int32> ToParking;
	if (!FindShortestPath(
			Nodes,
			CheckoutStart,
			ParkingGoals,
			ToParking))
	{
		return false;
	}

	const auto AppendPath =
		[&](const TArray<int32>& Path, bool bSkipFirst)
		{
			for (int32 Index = bSkipFirst ? 1 : 0;
				 Index < Path.Num();
				 ++Index)
			{
				OutCircuit.Add(Nodes[Path[Index]].Position);
			}
		};
	AppendPath(ToSales, false);
	for (const int32 NodeIndex : ToSales)
	{
		OutArrivalRoute.Add(Nodes[NodeIndex].Position);
	}
	for (const int32 NodeIndex : DirectReturn)
	{
		OutDirectReturnRoute.Add(Nodes[NodeIndex].Position);
	}
	AppendPath(ToCheckout, true);
	OutCheckoutWaypointIndex = OutCircuit.Num() - 1;
	AppendPath(ToParking, true);

	for (FVector& Point : OutCircuit)
	{
		Point.Z += 84.0f;
	}
	for (FVector& Point : OutArrivalRoute)
	{
		Point.Z += 84.0f;
	}
	for (FVector& Point : OutDirectReturnRoute)
	{
		Point.Z += 84.0f;
	}
	return OutCircuit.Num() >= 3 &&
		OutCircuit.IsValidIndex(OutCheckoutWaypointIndex) &&
		OutArrivalRoute.Num() >= 2 &&
		OutDirectReturnRoute.Num() >= 2;
}
