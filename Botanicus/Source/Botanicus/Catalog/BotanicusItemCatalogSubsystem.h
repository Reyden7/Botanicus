// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BotanicusItemCatalogSubsystem.generated.h"

/** Runtime resolver for the configured catalogue plus native test fallbacks. */
UCLASS()
class BOTANICUS_API UBotanicusItemCatalogSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(
		FSubsystemCollectionBase& Collection) override;

	const FBotanicusItemDefinition* FindItem(FName ItemKey) const;

	UFUNCTION(BlueprintCallable, Category="Botanicus|Catalog")
	bool GetItemDefinition(
		FName ItemKey,
		FBotanicusItemDefinition& OutDefinition) const;

	/** Returns the editable catalogue followed by non-duplicate native fallbacks. */
	UFUNCTION(BlueprintCallable, Category="Botanicus|Catalog")
	TArray<FBotanicusItemDefinition> GetAllItems() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBotanicusItemCatalog> LoadedCatalog;

	UPROPERTY(Transient)
	TArray<FBotanicusItemDefinition> NativeFallbackItems;
};
