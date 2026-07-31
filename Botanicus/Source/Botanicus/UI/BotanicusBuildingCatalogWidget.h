// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Catalog/BotanicusBuildingCatalog.h"
#include "BotanicusBuildingCatalogWidget.generated.h"

class ABotanicusPlayerController;
class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS()
class BOTANICUS_API UBotanicusBuildingCatalogRowWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRow(
		ABotanicusPlayerController* InController,
		const FBotanicusBuildingDefinition& InDefinition);
	void RefreshAvailability(
		int32 AvailableFunds,
		int32 DevelopmentLevel);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandlePurchaseClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PriceLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PurchaseButton;

	FName BuildingKey = NAME_None;
	int32 Price = 0;
	int32 RequiredDevelopmentLevel = 1;
	bool bUnlocked = false;
};

UCLASS()
class BOTANICUS_API UBotanicusBuildingCatalogWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithController(
		ABotanicusPlayerController* InController);
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();
	void RebuildRows();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BuildingsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBotanicusBuildingCatalogRowWidget>> Rows;
};
