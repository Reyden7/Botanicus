// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "BotanicusBotanistNotebookWidget.generated.h"

class ABotanicusPlayerController;
class UButton;
class UTextBlock;

/** Designer-authored botanist notebook opened with I. */
UCLASS()
class BOTANICUS_API UBotanicusBotanistNotebookWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithController(ABotanicusPlayerController* InController);
	void RefreshNotebook();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

private:
	void BindDesignerWidgets();
	void DiscoverPlantsInWorld();
	void ShowPlantAtIndex(int32 PlantIndex);
	void SetDetailText(UTextBlock* Label, const FText& Text) const;

	UFUNCTION()
	void HandlePlantSelectionChanged(
		FString SelectedItem,
		ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<ABotanicusPlayerController> BotanicusController;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> PlantSelectorWidget;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButtonWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlantNameLabelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlantElementLabelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TemperatureLabelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AirHumidityLabelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LuminosityLabelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaterLabelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GrowthLabelWidget;

	UPROPERTY(Transient)
	TArray<FBotanicusPlantDefinition> PlantDefinitions;

	TSet<FName> DiscoveredPlantKeys;
};
