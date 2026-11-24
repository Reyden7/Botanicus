#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusParcelPackaging.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusParcelPackagingSettings.generated.h"

class UAnimSequence;
class USkeletalMesh;
struct FBotanicusItemDefinition;

USTRUCT()
struct FBotanicusParcelModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UAnimSequence> OpeningAnimation;
	/** Bounds of the closed pose in mesh space, before MeshYaw is applied. */
	UPROPERTY(EditAnywhere)
	FVector ClosedCenter = FVector::ZeroVector;
	UPROPERTY(EditAnywhere)
	FVector ClosedHalfExtent = FVector(100,100,90);
	/** Align the imported lid seam with the cutter's local X axis. */
	UPROPERTY(EditAnywhere, meta=(Units="deg"))
	float MeshYaw = 0.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="10", Units="cm"))
	float NominalShortSideCm = 44.f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.1"))
	float MinimumSizeMultiplier = 0.85f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.1"))
	float MaximumSizeMultiplier = 2.7f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.05", Units="kg"))
	float EmptyMassKg = 0.45f;

	FVector LocalHalfExtent() const;
};

USTRUCT()
struct FBotanicusParcelContentOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EBotanicusParcelShape PreferredShape = EBotanicusParcelShape::Automatic;
	/** Full dimensions of a single packed unit, in cm. A zero vector uses the catalogue. */
	UPROPERTY(EditAnywhere, meta=(Units="cm"))
	FVector PackedDimensions = FVector::ZeroVector;
	/** Zero uses the estimated mass from the configured density. */
	UPROPERTY(EditAnywhere, meta=(ClampMin="0", Units="kg"))
	float UnitMassKg = 0.f;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Parcel Packaging"))
class BOTANICUS_API UBotanicusParcelPackagingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBotanicusParcelPackagingSettings();
	virtual FName GetCategoryName() const override { return TEXT("Botanicus"); }

	UPROPERTY(EditAnywhere, Config, Category="Models")
	FBotanicusParcelModel Square;
	UPROPERTY(EditAnywhere, Config, Category="Models")
	FBotanicusParcelModel Rectangular;
	UPROPERTY(EditAnywhere, Config, Category="Packing")
	TMap<FName, FBotanicusParcelContentOverride> ItemOverrides;
	UPROPERTY(EditAnywhere, Config, Category="Packing", meta=(ClampMin="1"))
	float RectangularAspectThreshold = 1.4f;
	UPROPERTY(EditAnywhere, Config, Category="Packing", meta=(ClampMin="0", Units="cm"))
	float PaddingCm = 4.f;
	UPROPERTY(EditAnywhere, Config, Category="Physics", meta=(ClampMin="0"))
	float DefaultContentDensityKgPerM3 = 65.f;
	UPROPERTY(EditAnywhere, Config, Category="Physics", meta=(ClampMin="0.05", Units="kg"))
	float MinimumUnitMassKg = 0.08f;
	/** Maximum diagonal footprint, with clearance for parcels yawing during the existing drop. */
	UPROPERTY(EditAnywhere, Config, Category="Packing", meta=(ClampMin="50", ClampMax="190", Units="cm"))
	float MaximumFootprintDiagonalCm = 180.f;

	const FBotanicusParcelModel& GetModel(EBotanicusParcelShape Shape) const;
	FBotanicusParcelPackaging Resolve(FName ItemKey, int32 Quantity, const FBotanicusItemDefinition* Definition) const;
};
