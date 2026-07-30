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
	OnePlayerCarry,
	TwoPlayerCarry
};

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery", meta=(ClampMin="1"))
	int32 DeliveryQuantity = 1;

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
