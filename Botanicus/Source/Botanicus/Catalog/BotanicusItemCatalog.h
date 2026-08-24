// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusItemCatalog.generated.h"

class AActor;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EBotanicusItemCategory : uint8
{
	Supply,
	Equipment,
	Decoration,
	Tool
};

UENUM(BlueprintType)
enum class EBotanicusItemWeightClass : uint8
{
	Hotbar,
	/** World tool carried directly in the player's hand, never in the hotbar. */
	Handheld,
	OnePlayerCarry,
	TwoPlayerCarry
};

UENUM(BlueprintType, meta=(Bitflags))
enum class EBotanicusCatalogTab : uint8
{
	None = 0 UMETA(Hidden),
	Seeds = 1 << 0,
	GardeningTools = 1 << 1,
	Preparation = 1 << 2,
	Sales = 1 << 3,
	CareAndMaintenance = 1 << 4
};
ENUM_CLASS_FLAGS(EBotanicusCatalogTab);

UENUM(BlueprintType, meta=(Bitflags))
enum class EBotanicusPlacementSurface : uint8
{
	None = 0 UMETA(Hidden),
	Floor = 1 << 0,
	Table = 1 << 1,
	Wall = 1 << 2
};
ENUM_CLASS_FLAGS(EBotanicusPlacementSurface);

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusItemDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FName ItemKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	EBotanicusItemCategory Category =
		EBotanicusItemCategory::Decoration;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Identity",
		meta=(Bitmask, BitmaskEnum="/Script/Botanicus.EBotanicusCatalogTab"))
	int32 CatalogTabs =
		static_cast<int32>(EBotanicusCatalogTab::Preparation);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	FVector WorldScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World")
	TSoftClassPtr<AActor> WorldActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1"))
	int32 MaximumStack = 1;

	/** True for a complete nursery plant that can be placed on a sales display. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bWholePlant = false;

	/** This supply can fill a sale pot before a compatible plant is repotted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bSaleSoil = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	EBotanicusItemWeightClass WeightClass =
		EBotanicusItemWeightClass::Hotbar;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Inventory",
		meta=(ClampMin="0.1", ClampMax="1.0"))
	float CarryMovementSpeedMultiplier = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery", meta=(ClampMin="0"))
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery")
	bool bPurchasable = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sales", meta=(ClampMin="0"))
	int32 SalePrice = 0;

	/** Visitor-facing colour family, for example Green, Red or Pink. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sales")
	FName PlantColorTag = NAME_None;

	/** Visitor-facing plant family, for example Aromatic or Flowering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sales")
	FName PlantTypeTag = NAME_None;

	/** Care quality retained by a harvested plant: Standard, Beautiful or Exceptional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sales")
	FName PlantQualityTag = TEXT("Standard");

	/** Base desirability before a visitor's personal preferences are applied. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Sales",
		meta=(ClampMin="0", ClampMax="100"))
	int32 VisitorAppeal = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery", meta=(ClampMin="1"))
	int32 DeliveryQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery", meta=(ClampMin="0.1", Units="s"))
	float DeliveryDelaySeconds = 5.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Placement",
		meta=(Bitmask, BitmaskEnum="/Script/Botanicus.EBotanicusPlacementSurface"))
	int32 AllowedPlacementSurfaces =
		static_cast<int32>(EBotanicusPlacementSurface::Floor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float RotationStep = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float FineRotationStep = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	bool bShowAlignmentGuides = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float AlignmentEdgeTolerance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1", ClampMax="10.0"))
	float AlignmentAngleTolerance = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	FVector CollisionHalfExtentOverride = FVector::ZeroVector;

	bool CanBePlacedOn(EBotanicusPlacementSurface Surface) const
	{
		// Inventory items can always be placed back on the floor. The
		// bitmask only adds optional destinations such as shelves or work
		// surfaces; it must never make an item impossible to put down.
		if (Surface == EBotanicusPlacementSurface::Floor &&
			(WeightClass == EBotanicusItemWeightClass::Hotbar ||
				WeightClass == EBotanicusItemWeightClass::Handheld))
		{
			return true;
		}

		return (AllowedPlacementSurfaces &
			static_cast<int32>(Surface)) != 0;
	}
};

/** Editable collection containing every purchasable or inventory item. */
UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusItemCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	const FBotanicusItemDefinition* FindItem(FName ItemKey) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Catalog")
	TArray<FBotanicusItemDefinition> Items;
};

/** Project Settings entry selecting the catalogue asset used by the game. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Item Catalog"))
class BOTANICUS_API UBotanicusItemCatalogSettings
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, Category="Catalog")
	TSoftObjectPtr<UBotanicusItemCatalog> Catalog;
};
