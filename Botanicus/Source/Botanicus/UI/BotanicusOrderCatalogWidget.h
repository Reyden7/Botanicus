// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "BotanicusOrderCatalogWidget.generated.h"

class ABotanicusPlayerController;
class UButton;
class UTextBlock;
class UVerticalBox;

/** One catalogue entry with its server-authoritative order action. */
UCLASS()
class BOTANICUS_API UBotanicusOrderItemRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRow(
		ABotanicusPlayerController* InController,
		const FBotanicusItemDefinition& InDefinition);
	void RefreshAvailability(int32 AvailableFunds);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleOrderClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PriceLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OrderButton;

	FName ItemKey = NAME_None;
	int32 Price = 0;
};

/** Full native catalogue screen populated from the configured item data asset. */
UCLASS()
class BOTANICUS_API UBotanicusOrderCatalogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithController(
		ABotanicusPlayerController* InController);
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RebuildItemRows();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FundsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PendingOrdersLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ItemsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBotanicusOrderItemRowWidget>> ItemRows;

	float RefreshAccumulator = 0.0f;
};
