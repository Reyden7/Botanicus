// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusWorkbenchUpgradeWidget.generated.h"

class ABotanicusPlayerController;
class ABotanicusPreparationWorkbenchActor;
class UButton;
class UTextBlock;
class UVerticalBox;

/** One selectable workbench level and its price/refund state. */
UCLASS()
class BOTANICUS_API UBotanicusWorkbenchLevelRowWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRow(
		ABotanicusPlayerController* InController,
		ABotanicusPreparationWorkbenchActor* InWorkbench,
		int32 InTargetLevel);
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleLevelClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPreparationWorkbenchActor> Workbench;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PriceLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LevelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelButtonLabel;

	int32 TargetLevel = 1;
};

/** Temporary native panel opened from the cube on a preparation workbench. */
UCLASS()
class BOTANICUS_API UBotanicusWorkbenchUpgradeWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithWorkbench(
		ABotanicusPlayerController* InController,
		ABotanicusPreparationWorkbenchActor* InWorkbench);
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void BuildLayout();
	void RebuildLevelRows();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPreparationWorkbenchActor> Workbench;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LevelsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBotanicusWorkbenchLevelRowWidget>>
		LevelRows;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	float RefreshAccumulator = 0.0f;
};
