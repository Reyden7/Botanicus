// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BotanicusCommandPanelSettings.generated.h"

/** Editable size and visual offset for one command-panel element. */
USTRUCT(BlueprintType)
struct FBotanicusCommandElementLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Disposition", meta = (ClampMin = "1.0"))
	FVector2D Size = FVector2D(100.0f, 50.0f);

	UPROPERTY(EditAnywhere, Config, Category = "Disposition")
	FVector2D Offset = FVector2D::ZeroVector;
};

/**
 * Visual layout controls for the order computer.
 * Available in Project Settings > Botanicus > Interface de l'ordinateur.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Interface de l'ordinateur"))
class BOTANICUS_API UBotanicusCommandPanelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Botanicus"); }

	UPROPERTY(EditAnywhere, Config, Category = "Ecran", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D ScreenMinAnchor = FVector2D(0.063f, 0.060f);

	UPROPERTY(EditAnywhere, Config, Category = "Ecran", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D ScreenMaxAnchor = FVector2D(0.938f, 0.895f);

	UPROPERTY(EditAnywhere, Config, Category = "Entete")
	FBotanicusCommandElementLayout Logo = {FVector2D(42.0f, 42.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Entete")
	FVector2D TitleOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Config, Category = "Entete")
	FBotanicusCommandElementLayout Credits = {FVector2D(158.0f, 50.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Entete")
	FBotanicusCommandElementLayout Level = {FVector2D(168.0f, 50.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Entete")
	FBotanicusCommandElementLayout ShopState = {FVector2D(184.0f, 60.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Entete")
	FBotanicusCommandElementLayout CloseButton = {FVector2D(125.0f, 60.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Onglets")
	FBotanicusCommandElementLayout Tab = {FVector2D(270.0f, 50.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Onglets", meta = (ClampMin = "8.0"))
	float TabIconSize = 36.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Catalogue")
	FBotanicusCommandElementLayout ItemIcon = {FVector2D(48.0f, 48.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Catalogue")
	FBotanicusCommandElementLayout ElementBadge = {FVector2D(92.0f, 29.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Catalogue")
	FBotanicusCommandElementLayout PriceArea = {FVector2D(132.0f, 32.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Catalogue")
	FBotanicusCommandElementLayout OrderButton = {FVector2D(166.0f, 42.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Catalogue")
	FBotanicusCommandElementLayout Scrollbar = {FVector2D(24.0f, 420.0f), FVector2D::ZeroVector};

	UPROPERTY(EditAnywhere, Config, Category = "Livraisons", meta = (ClampMin = "120.0"))
	float DeliveryPanelWidth = 238.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Livraisons")
	FVector2D DeliveryPanelOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Config, Category = "Livraisons")
	FBotanicusCommandElementLayout ParcelIcon = {FVector2D(42.0f, 42.0f), FVector2D::ZeroVector};
};
