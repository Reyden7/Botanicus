#pragma once

#include "CoreMinimal.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "Components/BoxComponent.h"
#include "BotanicusPlacementSurfaceComponent.generated.h"

UCLASS(ClassGroup=(Botanicus), meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusPlacementSurfaceComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UBotanicusPlacementSurfaceComponent();
	static constexpr ECollisionChannel PlacementChannel = ECC_GameTraceChannel2;

	/** The top face of the box is the support plane. Use Box Extent to size it. */
	UPROPERTY(EditAnywhere, Category="Placement")
	TArray<EBotanicusItemCategory> AllowedCategories;

	/** An empty list allows all item keys in the selected categories. */
	UPROPERTY(EditAnywhere, Category="Placement")
	TArray<FName> AllowedItemKeys;

	/** Supports placed parcels and other physics bodies on meshes without a physical top. */
	UPROPERTY(EditAnywhere, Category="Placement")
	bool bSupportPhysics = true;

	static bool HasSupportedItems(const AActor* Furniture);
	bool Accepts(FName Key, EBotanicusItemCategory Category) const;

protected:
	virtual void OnRegister() override;
};
