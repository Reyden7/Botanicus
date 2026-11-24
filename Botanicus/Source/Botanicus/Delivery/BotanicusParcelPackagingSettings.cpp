#include "Delivery/BotanicusParcelPackagingSettings.h"

#include "Catalog/BotanicusItemCatalog.h"
#include "Engine/StaticMesh.h"

FVector FBotanicusParcelModel::LocalHalfExtent() const
{
	const FBox ClosedBox(-ClosedHalfExtent.GetAbs(), ClosedHalfExtent.GetAbs());
	return ClosedBox.TransformBy(FTransform(FRotator(0.f, MeshYaw, 0.f))).GetExtent().ComponentMax(FVector(1.0));
}

UBotanicusParcelPackagingSettings::UBotanicusParcelPackagingSettings()
{
	Square.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/carr\u00e9/Carton_carr\u00e9.Carton_carr\u00e9")));
	Square.OpeningAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/carr\u00e9/open.open")));
	Rectangular.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/rectangle/carton_rig.carton_rig")));
	Rectangular.OpeningAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/carton2/rectangle/open.open")));
	Rectangular.EmptyMassKg = 0.65f;

	// Closed poses of the imported rigs, measured in their native mesh spaces.
	Square.ClosedCenter = FVector(-.590, -.949, 98.317);
	Square.ClosedHalfExtent = FVector(100.590, 101.003, 98.842);
	Rectangular.ClosedCenter = FVector(-100.590, -.924, 89.748);
	Rectangular.ClosedHalfExtent = FVector(100.590, 161.994, 91.292);
	Rectangular.MeshYaw = 90.f;
}

const FBotanicusParcelModel& UBotanicusParcelPackagingSettings::GetModel(EBotanicusParcelShape Shape) const
{
	return Shape == EBotanicusParcelShape::Rectangular ? Rectangular : Square;
}

FBotanicusParcelPackaging UBotanicusParcelPackagingSettings::Resolve(
	FName ItemKey, int32 Quantity, const FBotanicusItemDefinition* Definition) const
{
	const FBotanicusParcelContentOverride* Override = ItemOverrides.Find(ItemKey);
	FVector Dimensions = Override ? Override->PackedDimensions : FVector::ZeroVector;
	if (Dimensions.GetMin() <= 0.0 && Definition)
	{
		Dimensions = Definition->CollisionHalfExtentOverride * 2.0;
		if (Dimensions.GetMin() <= 0.0)
		{
			if (const UStaticMesh* Mesh = Definition->WorldMesh.LoadSynchronous())
				Dimensions = Mesh->GetBounds().BoxExtent * Definition->WorldScale.GetAbs() * 2.0;
		}
	}
	if (Dimensions.GetMin() <= 0.0)
		Dimensions = FVector(24, 20, 16);

	const double UnitVolumeCm3 = Dimensions.X * Dimensions.Y * Dimensions.Z;
	const double UnitMass = Override && Override->UnitMassKg > 0.f ? Override->UnitMassKg :
		FMath::Max<double>(MinimumUnitMassKg, UnitVolumeCm3 * 1.e-6 * DefaultContentDensityKgPerM3);
	const EBotanicusParcelShape Preferred = Override ? Override->PreferredShape : EBotanicusParcelShape::Automatic;
	const double Sides[] = {Dimensions.X, Dimensions.Y, Dimensions.Z};
	const double MiddleSide = Dimensions.X + Dimensions.Y + Dimensions.Z - Dimensions.GetMax() - Dimensions.GetMin();
	const double FootprintRatio = Dimensions.GetMax() / MiddleSide;
	const EBotanicusParcelShape Shape = Preferred != EBotanicusParcelShape::Automatic ? Preferred :
		(FootprintRatio >= RectangularAspectThreshold ? EBotanicusParcelShape::Rectangular : EBotanicusParcelShape::Square);
	const FBotanicusParcelModel& Model = GetModel(Shape);

	const FVector NativeHalf = Model.LocalHalfExtent();
	const FVector NominalSize = NativeHalf * (Model.NominalShortSideCm / FMath::Min(NativeHalf.X, NativeHalf.Y));
	double RequiredScale = TNumericLimits<double>::Max();
	// Try all six axis-aligned orientations. Use the same padded envelope for
	// the unit-size check and total volume, including every item in the order.
	const int32 Count = FMath::Max(1, Quantity);
	const double TotalVolume = (Sides[0]+PaddingCm)*(Sides[1]+PaddingCm)*(Sides[2]+PaddingCm)*Count;
	const double VolumeScale = FMath::Pow(TotalVolume / (NominalSize.X*NominalSize.Y*NominalSize.Z), 1.0/3.0);
	for (int32 X=0; X<3; ++X)
		for (int32 Y=0; Y<3; ++Y)
		{
			if (X == Y) continue;
			const FVector Padded(Sides[X]+PaddingCm, Sides[Y]+PaddingCm, Sides[3-X-Y]+PaddingCm);
			RequiredScale = FMath::Min(RequiredScale, FMath::Max((Padded / NominalSize).GetMax(), VolumeScale));
		}

	const double FootprintLimit = MaximumFootprintDiagonalCm / FVector2D(NominalSize.X, NominalSize.Y).Size();
	const double MaxScale = FMath::Min<double>(Model.MaximumSizeMultiplier, FootprintLimit);
	const double Scale = FMath::Clamp(RequiredScale, FMath::Min<double>(Model.MinimumSizeMultiplier, MaxScale), MaxScale);
	const double EmptyMass = Model.EmptyMassKg * FMath::Square(Scale);
	FBotanicusParcelPackaging Result;
	Result.Shape = Shape;
	Result.HalfExtent = NominalSize * (0.5 * Scale);
	Result.MassKg = FMath::Clamp(EmptyMass + UnitMass * Count, 0.1, 1000.);
	return Result;
}
