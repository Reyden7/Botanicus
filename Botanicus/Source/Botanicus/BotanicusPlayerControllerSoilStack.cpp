#include "BotanicusPlayerController.h"

#include "BotanicusGameMode.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "Components/StaticMeshComponent.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ItemDataAsset.h"
#include "ItemDataSubsystem.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Storage/BotanicusStorageShelfActor.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"

namespace
{
	const FName SoilBagItemKey(TEXT("PottingSoil"));
	constexpr float GroundGap = 0.25f;
	constexpr float ContactTolerance = 5.f;

	bool IsStackableWorldBag(const ABotanicusPlaceableItemActor* Bag)
	{
		return ABotanicusStorageShelfActor::IsStoredWorldItem(Bag) &&
			Bag->GetItemKey() == SoilBagItemKey && IsValid(Bag->Mesh) &&
			Bag->Mesh->GetStaticMesh() && !Bag->Mesh->IsSimulatingPhysics() &&
			FMath::Abs(Bag->GetActorRotation().GetNormalized().Pitch) < 5.f &&
			FMath::Abs(Bag->GetActorRotation().GetNormalized().Roll) < 5.f;
	}

	FBox BagBounds(const ABotanicusPlaceableItemActor& Bag)
	{
		return Bag.Mesh->CalcBounds(Bag.Mesh->GetComponentTransform()).GetBox();
	}

	FCollisionObjectQueryParams StackCollisionObjects()
	{
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		return Objects;
	}

	using FNearbyBags = TArray<const ABotanicusPlaceableItemActor*, TInlineAllocator<16>>;

	FNearbyBags FindBagsInColumn(const ABotanicusPlaceableItemActor& Bag, double MinZ, double MaxZ)
	{
		const FBox Bounds = BagBounds(Bag);
		const FVector Center = Bounds.GetCenter();
		const FVector Extent = Bounds.GetExtent();
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusSoilBagColumn), false, &Bag);
		Bag.GetWorld()->OverlapMultiByObjectType(Overlaps,
			FVector(Center.X, Center.Y, (MinZ + MaxZ) * 0.5), FQuat::Identity,
			StackCollisionObjects(),
			FCollisionShape::MakeBox(FVector(Extent.X + 3.f, Extent.Y + 3.f, (MaxZ - MinZ) * 0.5)),
			Query);

		FNearbyBags Bags;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (const auto* Other = Cast<ABotanicusPlaceableItemActor>(Overlap.GetActor());
				IsStackableWorldBag(Other))
			{
				Bags.AddUnique(Other);
			}
		}
		return Bags;
	}

	const ABotanicusPlaceableItemActor* FindAdjacentBag(const ABotanicusPlaceableItemActor& Bag,
		const FNearbyBags& Candidates, bool bAbove)
	{
		const FBox Bounds = BagBounds(Bag);
		const FVector Center = Bounds.GetCenter();
		const ABotanicusPlaceableItemActor* Closest = nullptr;
		double BestGap = ContactTolerance;
		for (const ABotanicusPlaceableItemActor* Candidate : Candidates)
		{
			if (Candidate == &Bag)
			{
				continue;
			}
			const FBox Other = BagBounds(*Candidate);
			const FVector OtherCenter = Other.GetCenter();
			if ((bAbove ? OtherCenter.Z <= Center.Z : OtherCenter.Z >= Center.Z) ||
				FVector::DistSquared2D(Center, OtherCenter) > FMath::Square(3.f))
			{
				continue;
			}
			const double Gap = bAbove ? Other.Min.Z - Bounds.Max.Z : Bounds.Min.Z - Other.Max.Z;
			if (FMath::Abs(Gap) <= BestGap)
			{
				BestGap = FMath::Abs(Gap);
				Closest = Candidate;
			}
		}
		return Closest;
	}

	FBox ResolveBagGeometry(const UObject& Context, const FBotanicusItemDefinition& Definition,
		const ABotanicusPlaceableItemActor& Defaults)
	{
		if (!Defaults.Mesh)
		{
			return FBox(ForceInit);
		}

		const UStaticMesh* Mesh = Defaults.Mesh->GetStaticMesh();
		FTransform MeshTransform = Defaults.Mesh->GetRelativeTransform();
		if (!Defaults.UsesBlueprintAppearance())
		{
			const UItemDataAsset* Data = UItemDataSubsystem::Get(&Context).GetItemDataAsset(SoilBagItemKey);
			const UStaticMesh* ItemMesh = Data ? Data->GetItemStaticMesh().LoadSynchronous() : nullptr;
			if (const UStaticMesh* Resolved = ItemMesh ? ItemMesh : Definition.WorldMesh.LoadSynchronous())
			{
				Mesh = Resolved;
			}
			MeshTransform.SetScale3D(ItemMesh ? FVector::OneVector : Definition.WorldScale);
		}
		return Mesh ? Mesh->GetBounds().GetBox().TransformBy(MeshTransform) : FBox(ForceInit);
	}
}

const ABotanicusPlaceableItemActor* ABotanicusPlayerController::GetSoilBagDefaults(
	const FBotanicusItemDefinition* Definition)
{
	const UClass* Class = Definition ? Definition->WorldActorClass.LoadSynchronous() : nullptr;
	const auto* Item = Class ? Cast<ABotanicusPlaceableItemActor>(Class->GetDefaultObject()) : nullptr;
	return Item ? Item : GetDefault<ABotanicusPlaceableItemActor>();
}

int32 ABotanicusPlayerController::GetSoilBagPlacementLimit(const FBotanicusItemDefinition* Definition)
{
	const auto* Bag = GetSoilBagDefaults(Definition);
	return Bag->bCanStackSoilBags ? FMath::Max(1, Bag->MaxSoilBagStackCount) : 1;
}

bool ABotanicusPlayerController::HasSoilBagAbove(const ABotanicusPlaceableItemActor* Bag)
{
	if (!IsStackableWorldBag(Bag))
	{
		return false;
	}
	const FBox Bounds = BagBounds(*Bag);
	return FindAdjacentBag(*Bag, FindBagsInColumn(*Bag, Bounds.Max.Z - ContactTolerance,
		Bounds.Max.Z + Bounds.GetSize().Z + ContactTolerance), true) != nullptr;
}

bool ABotanicusPlayerController::ResolveSoilBagPlacement(const FBotanicusItemDefinition& Definition,
	const FVector& RequestedLocation, float RequestedYaw, int32 RequestedQuantity,
	FTransform& OutTransform, FSoilBagPlacement& OutPlan) const
{
	OutPlan = FSoilBagPlacement{};
	OutTransform = FTransform::Identity;
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn || RequestedLocation.ContainsNaN() || !FMath::IsFinite(RequestedYaw) ||
		FVector::DistSquared2D(ControlledPawn->GetActorLocation(), RequestedLocation) >
			FMath::Square(MaximumQuickBarItemPlacementDistance))
	{
		return false;
	}

	OutTransform = FTransform(FRotator(0.f, RequestedYaw, 0.f), RequestedLocation);
	const ABotanicusPlaceableItemActor* BagDefaults = GetSoilBagDefaults(&Definition);
	const FBox Geometry = ResolveBagGeometry(*this, Definition, *BagDefaults);
	if (!Geometry.IsValid || Geometry.GetSize().Z <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const int32 Limit = GetSoilBagPlacementLimit(&Definition);
	const float Gap = FMath::Clamp(BagDefaults->StackVerticalOffset, -2.f, 4.f);
	OutPlan.Step = FMath::Max(1., Geometry.GetSize().Z + Gap);

	FCollisionQueryParams TraceQuery(SCENE_QUERY_STAT(BotanicusSoilBagSupport), false, ControlledPawn);
	if (IsValid(LocalQuickBarItemPreview))
	{
		TraceQuery.AddIgnoredActor(LocalQuickBarItemPreview);
	}

	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	FHitResult Hit;
	World->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 700.f, ECC_Visibility, TraceQuery);
	const auto* AimedBag = Cast<ABotanicusPlaceableItemActor>(Hit.GetActor());
	const bool bAimingAtSoil = AimedBag && AimedBag->GetItemKey() == SoilBagItemKey;
	const ABotanicusPlaceableItemActor* Support = nullptr;
	int32 AvailableSpace = Limit;
	if (bAimingAtSoil)
	{
		if (!IsStackableWorldBag(AimedBag))
		{
			return false;
		}
		const FBox Bounds = BagBounds(*AimedBag);
		const FVector Center = Bounds.GetCenter();
		const FQuat Rotation = FRotator(0.f, AimedBag->GetActorRotation().Yaw, 0.f).Quaternion();
		const FVector HorizontalOffset = Rotation.RotateVector(FVector(Geometry.GetCenter().X, Geometry.GetCenter().Y, 0.f));
		OutTransform = FTransform(Rotation, FVector(Center.X - HorizontalOffset.X,
			Center.Y - HorizontalOffset.Y, Bounds.Max.Z - Geometry.Min.Z + Gap));

		const double TopEdgeDepth = FMath::Min(10., Bounds.GetSize().Z * 0.35);
		const bool bAimsAtTop = Hit.ImpactNormal.Z >= 0.4f ||
			(Hit.ImpactNormal.Z >= -0.2f && Hit.ImpactPoint.Z >= Bounds.Max.Z - TopEdgeDepth);
		if (!BagDefaults->bCanStackSoilBags || !AimedBag->bCanStackSoilBags ||
			!bAimsAtTop ||
			FVector::DistSquared2D(Hit.ImpactPoint, Center) > FMath::Square(AimedBag->StackSnapDistance) ||
			FVector::DistSquared2D(ControlledPawn->GetActorLocation(), OutTransform.GetLocation()) >
				FMath::Square(MaximumQuickBarItemPlacementDistance))
		{
			return false;
		}

		const double ScanHeight = FMath::Max(Geometry.GetSize().Z, Bounds.GetSize().Z) + ContactTolerance;
		const FNearbyBags Bags = FindBagsInColumn(*AimedBag,
			Bounds.Min.Z - ScanHeight * Limit, Bounds.Max.Z + ScanHeight);
		if (FindAdjacentBag(*AimedBag, Bags, true))
		{
			return false;
		}

		Support = AimedBag;
		int32 StackSize = 0;
		for (const ABotanicusPlaceableItemActor* Bag = Support; Bag && StackSize < Limit;
			Bag = FindAdjacentBag(*Bag, Bags, false))
		{
			StackSize += FMath::Min(Bag->GetQuantity(), Limit - StackSize);
		}
		AvailableSpace = FMath::Min(Limit, FMath::Max(1, Support->MaxSoilBagStackCount)) - StackSize;
	}
	else
	{
		const int32 FurnitureCount = FMath::Min(RequestedQuantity, AvailableSpace);
		if (const auto Furniture = ResolveFurniturePlacement(Definition.ItemKey, RequestedYaw, [&Geometry] { return Geometry; },
			FVector::OneVector, nullptr, OutTransform, FurnitureCount, OutPlan.Step); Furniture.IsSet())
		{
			OutPlan.Count = Furniture.GetValue() ? FurnitureCount : 0;
			return Furniture.GetValue();
		}
		FHitResult Floor;
		const double PawnZ = ControlledPawn->GetActorLocation().Z;
		World->LineTraceSingleByChannel(Floor, FVector(RequestedLocation.X, RequestedLocation.Y, PawnZ + 140.f),
			FVector(RequestedLocation.X, RequestedLocation.Y, PawnZ - 450.f), ECC_Visibility, TraceQuery);
		if (!Floor.bBlockingHit || Floor.ImpactNormal.Z < 0.7f ||
			IsValid(Cast<ABotanicusPlaceableItemActor>(Floor.GetActor())))
		{
			return false;
		}
		OutTransform.SetLocation(FVector(RequestedLocation.X, RequestedLocation.Y,
			Floor.ImpactPoint.Z - Geometry.Min.Z + GroundGap));
	}

	const int32 Count = FMath::Min(RequestedQuantity, AvailableSpace);
	if (Count <= 0)
	{
		return false;
	}
	FCollisionQueryParams OverlapQuery = TraceQuery;
	// The small configurable intersection is allowed against the selected
	// support only. All other geometry, including other bags, is tested.
	if (Support)
	{
		OverlapQuery.AddIgnoredActor(Support);
	}
	const FVector Extent = (Geometry.GetExtent() - FVector(0.35f)).ComponentMax(FVector(0.1f));
	const FVector Center = OutTransform.TransformPosition(Geometry.GetCenter());
	const FQuat Rotation = OutTransform.GetRotation();
	const FCollisionObjectQueryParams Objects = StackCollisionObjects();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (World->OverlapAnyTestByObjectType(Center + FVector(0.f, 0.f, Index * OutPlan.Step),
			Rotation, Objects, FCollisionShape::MakeBox(Extent), OverlapQuery))
		{
			return false;
		}
	}
	OutPlan.Count = Count;
	return true;
}

void ABotanicusPlayerController::PlaceSoilBags(UBotanicusQuickBarComponent& QuickBar,
	int32 SlotIndex, FGuid InstanceId, const FBotanicusItemDefinition& Definition,
	const FTransform& FirstTransform, const FSoilBagPlacement& Plan, int32 RequestedQuantity)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TArray<ABotanicusPlaceableItemActor*, TInlineAllocator<10>> SpawnedBags;
	UClass* BagClass = GetSoilBagDefaults(&Definition)->GetClass();
	for (int32 Index = 0; Index < Plan.Count; ++Index)
	{
		FTransform Transform = FirstTransform;
		Transform.AddToTranslation(FVector(0.f, 0.f, Index * Plan.Step));
		auto* Bag = World->SpawnActor<ABotanicusPlaceableItemActor>(BagClass, Transform, Parameters);
		if (!IsValid(Bag))
		{
			break;
		}
		Bag->InitializePlacedItem(SoilBagItemKey, 1);
		SpawnedBags.Add(Bag);
	}

	const FBotanicusQuickBarSlot Slot = QuickBar.GetSlot(SlotIndex);
	if (SpawnedBags.Num() != Plan.Count || Slot.InstanceId != InstanceId ||
		Slot.ItemKey != SoilBagItemKey || !QuickBar.RemoveQuantity(SlotIndex, Plan.Count))
	{
		for (auto* Bag : SpawnedBags)
		{
			Bag->Destroy();
		}
		ClientMessage(TEXT("Placement refuse : la pile n'a pas pu etre creee."));
		return;
	}

	for (int32 Index = 0; Index < SpawnedBags.Num(); ++Index)
	{
		UBotanicusJuiceWorldSubsystem::PlacementSucceeded(SpawnedBags[Index], GetPawn(), false, Index == 0);
	}
	if (Plan.Count < RequestedQuantity)
	{
		ClientMessage(FString::Printf(TEXT("%d sac(s) poses, %d conserve(s) dans l'inventaire."),
			Plan.Count, RequestedQuantity - Plan.Count));
	}
	if (auto* GameMode = World->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}
