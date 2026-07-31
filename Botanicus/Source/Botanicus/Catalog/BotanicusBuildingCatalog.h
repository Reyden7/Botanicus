// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusBuildingCatalog.generated.h"

class UTexture2D;
class ABotanicusCatalogBuildingActor;

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusBuildingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FName BuildingKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Persistent actor tag identifying one seed in the template group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Template")
	FName TemplateTag = NAME_None;

	/**
	 * Fallback index among complete template groups, ordered by distance from
	 * the purchasing player. Used by legacy maps that predate template tags.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Template", meta=(ClampMin="0"))
	int32 LegacyTemplateGroupIndex = 0;

	/** Complete native prefab used when the map has no EBS template group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Template")
	TSoftClassPtr<ABotanicusCatalogBuildingActor> FallbackPrefabClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Economy", meta=(ClampMin="0"))
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
	bool bUnlockedByDefault = true;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category="Progression",
		meta=(ClampMin="1"))
	int32 RequiredDevelopmentLevel = 1;

	/** Offset applied before the purchased group enters placement mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	FVector PreviewOffset = FVector(3500.0f, 0.0f, 0.0f);
};

/** Editable collection containing every purchasable building. */
UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusBuildingCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	const FBotanicusBuildingDefinition* FindBuilding(
		FName BuildingKey) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Catalog")
	TArray<FBotanicusBuildingDefinition> Buildings;
};

/** Project Settings entry selecting the building catalogue used by the game. */
UCLASS(
	Config=Game,
	DefaultConfig,
	meta=(DisplayName="Botanicus Building Catalog"))
class BOTANICUS_API UBotanicusBuildingCatalogSettings
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, Category="Catalog")
	TSoftObjectPtr<UBotanicusBuildingCatalog> Catalog;
};
