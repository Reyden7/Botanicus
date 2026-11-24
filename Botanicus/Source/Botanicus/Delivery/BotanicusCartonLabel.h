#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "BotanicusCartonLabel.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

USTRUCT(BlueprintType)
struct FBotanicusCartonLabelCategory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Label") FText DisplayName;
	UPROPERTY(EditAnywhere, Category="Label") TObjectPtr<UTexture2D> CategoryIcon;
};

USTRUCT(BlueprintType)
struct FBotanicusCartonLabelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Label") FName Category;
	UPROPERTY(EditAnywhere, Category="Label") TObjectPtr<UTexture2D> CategoryIcon;
	UPROPERTY(EditAnywhere, Category="Label") FString ItemReference;
	UPROPERTY(EditAnywhere, Category="Label") FString NCCode;
	UPROPERTY(EditAnywhere, Category="Label", meta=(ToolTip="Code 128 B: jusqu'à 39 caractères ASCII imprimables. Vide : encode la référence.")) FString BarcodeValue;
	UPROPERTY(EditAnywhere, Category="Label", meta=(MultiLine=true)) FText HandlingInstructions;
};

USTRUCT(BlueprintType)
struct FBotanicusCartonLabelRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Label") FString ItemIdPrefix;
	UPROPERTY(EditAnywhere, Category="Label") FName PlantTypeTag;
	UPROPERTY(EditAnywhere, Category="Label") FName Category;
};

UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusCartonLabelCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// The map key is the stable ItemId from the item catalog.
	UPROPERTY(EditAnywhere, Category="Label") TMap<FName, FBotanicusCartonLabelEntry> Items;
	UPROPERTY(EditAnywhere, Category="Label") TMap<FName, FBotanicusCartonLabelCategory> Categories;
	/** First matching rule supplies the category when an item has no explicit category. */
	UPROPERTY(EditAnywhere, Category="Label") TArray<FBotanicusCartonLabelRule> CategoryRules;
	UPROPERTY(EditAnywhere, Category="Artwork") TObjectPtr<UTexture2D> Background;
	UPROPERTY(EditAnywhere, Category="Artwork") TObjectPtr<UTexture2D> Logo;
	UPROPERTY(EditAnywhere, Category="Artwork") TObjectPtr<UMaterialInterface> SurfaceMaterial;

	static UBotanicusCartonLabelCatalog* Get();
};

/** Local rendering cache. Live surfaces own their material and render target. */
UCLASS()
class UBotanicusCartonLabelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static void Apply(UStaticMeshComponent* Surface, FName ItemId, int32 Quantity);
	static void SuppressImportedLabel(class USkeletalMeshComponent* Carton);
	static void ConfigureSurface(UStaticMeshComponent* Surface);
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	UMaterialInstanceDynamic* Resolve(FName ItemId, int32 Quantity);
	UPROPERTY(Transient) TObjectPtr<UBotanicusCartonLabelCatalog> PreloadedCatalog;
	TMap<FString, TWeakObjectPtr<UMaterialInstanceDynamic>> Materials;
};
