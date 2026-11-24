#include "BotanicusPlayerController.h"

#include "Building/BotanicusPlacementSurfaceComponent.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

TOptional<bool> ABotanicusPlayerController::ResolveFurniturePlacement(
	FName Key, float Yaw, TFunctionRef<FBox()> GetBounds, const FVector& Scale, const AActor* MovingItem,
	FTransform& OutTransform, int32 Count, float StackStep) const
{
	UWorld* World = GetWorld();
	if (!World || !GetPawn() || !FMath::IsFinite(Yaw))
		return {};

	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	const FVector End = Eye + View.Vector() * MaximumQuickBarItemPlacementDistance;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusFurniturePlacement), false, GetPawn());
	Query.AddIgnoredActor(LocalQuickBarItemPreview.Get());
	Query.AddIgnoredActor(MovingItem);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Eye, End, UBotanicusPlacementSurfaceComponent::PlacementChannel, Query))
		return {};

	const auto* Surface = Cast<UBotanicusPlacementSurfaceComponent>(Hit.GetComponent());
	if (!Surface)
		return {};


	const FBox Bounds = GetBounds();
	if (!Bounds.IsValid)
		return false;
	const FVector Up = Surface->GetUpVector();
	const FVector Top = Surface->GetComponentLocation() + Up * Surface->GetScaledBoxExtent().Z;
	const FQuat Rotation = FRotator(0, Yaw, 0).Quaternion();
	const FVector XY(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Top.Z);
	OutTransform = FTransform(Rotation, XY + FVector(0, 0, 0.35 - Bounds.Min.Z), Scale);
	FHitResult Obstacle;
	if (World->LineTraceSingleByChannel(Obstacle, Eye, Hit.ImpactPoint, ECC_Visibility, Query) &&
		FVector::DistSquared(Obstacle.ImpactPoint, Hit.ImpactPoint) > FMath::Square(0.75))
	{
		// A placed object blocks the aimed support, even when it hides the plate.
		if (IsValid(Obstacle.GetActor()) && (Obstacle.GetActor()->IsA<ABotanicusPlaceableItemActor>() ||
			Obstacle.GetActor()->IsA<ABotanicusDeliveryParcelActor>()))
		{
			return false;
		}
		return {};
	}

	const auto* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>() : nullptr;
	const auto* Definition = Catalog ? Catalog->FindItem(Key) : nullptr;
	if (Count <= 0 || Up.Z < 0.995 || FVector::DotProduct(Hit.ImpactNormal, Up) < 0.98 ||
		!Surface->Accepts(Key, Definition ? Definition->Category : EBotanicusItemCategory::Supply))
		return false;

	double HighestSupportZ = -TNumericLimits<double>::Max();
	const FVector BoxExtent = Surface->GetUnscaledBoxExtent();
	for (const double X : {Bounds.Min.X, Bounds.Max.X})
	{
		for (const double Y : {Bounds.Min.Y, Bounds.Max.Y})
		{
			FVector Point = XY + Rotation.RotateVector(FVector(X, Y, 0));
			Point.Z = Top.Z - FVector::DotProduct(Up, Point - Top) / Up.Z;
			HighestSupportZ = FMath::Max(HighestSupportZ, Point.Z);
			const FVector Local = Surface->GetComponentTransform().InverseTransformPosition(Point);
			if (FMath::Abs(Local.X) > BoxExtent.X - 0.25 || FMath::Abs(Local.Y) > BoxExtent.Y - 0.25)
				return false;
		}
	}
	OutTransform.SetLocation(FVector(XY.X, XY.Y, HighestSupportZ - Bounds.Min.Z + 0.35));

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	const FVector Center = OutTransform.GetLocation() + Rotation.RotateVector(Bounds.GetCenter());
	const FVector Extent = (Bounds.GetExtent() - FVector(0.12)).ComponentMax(FVector(0.08));
	TArray<FOverlapResult> Overlaps;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Overlaps.Reset();
		World->OverlapMultiByObjectType(Overlaps, Center + FVector(0, 0, StackStep * Index), Rotation,
			Objects, FCollisionShape::MakeBox(Extent), Query);
		for (const auto& Overlap : Overlaps)
		{
			if (IsValid(Overlap.GetComponent()) && !Overlap.GetComponent()->IsA<UBotanicusPlacementSurfaceComponent>())
				return false;
		}
	}
	return true;
}
