// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusBotanistNotebookWidget.h"

#include "BotanicusPlayerController.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UBotanicusBotanistNotebookWidget::InitializeWithController(
	ABotanicusPlayerController* InController)
{
	BotanicusController = InController;
	RefreshNotebook();
}

void UBotanicusBotanistNotebookWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BindDesignerWidgets();
}

FReply UBotanicusBotanistNotebookWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::I ||
		InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (BotanicusController)
		{
			BotanicusController->ToggleBotanistNotebook();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBotanicusBotanistNotebookWidget::BindDesignerWidgets()
{
	PlantSelectorWidget = Cast<UComboBoxString>(GetWidgetFromName(TEXT("PlantSelector")));
	CloseButtonWidget = Cast<UButton>(GetWidgetFromName(TEXT("CloseButton")));
	PlantNameLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlantNameLabel")));
	PlantElementLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlantElementLabel")));
	TemperatureLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("TemperatureLabel")));
	AirHumidityLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("AirHumidityLabel")));
	LuminosityLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("LuminosityLabel")));
	WaterLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("WaterLabel")));
	GrowthLabelWidget = Cast<UTextBlock>(GetWidgetFromName(TEXT("GrowthLabel")));

	if (PlantSelectorWidget)
	{
		PlantSelectorWidget->OnSelectionChanged.AddDynamic(
			this,
			&UBotanicusBotanistNotebookWidget::HandlePlantSelectionChanged);
	}
	if (CloseButtonWidget)
	{
		CloseButtonWidget->OnClicked.AddDynamic(
			this,
			&UBotanicusBotanistNotebookWidget::HandleCloseClicked);
	}
}

void UBotanicusBotanistNotebookWidget::DiscoverPlantsInWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ABotanicusPlantPotActor> It(World); It; ++It)
	{
		ABotanicusPlantPotActor* Pot = *It;
		if (!Pot)
		{
			continue;
		}
		if (const ABotanicusMultiPlantPotActor* MultiPot =
				Cast<ABotanicusMultiPlantPotActor>(Pot))
		{
			for (const FBotanicusMultiPlantSlotState& PlantSlot :
				MultiPot->GetPlantSlots())
			{
				if (!PlantSlot.PlantKey.IsNone())
				{
					DiscoveredPlantKeys.Add(PlantSlot.PlantKey);
				}
			}
		}
		else if (!Pot->GetPlantKey().IsNone())
		{
			DiscoveredPlantKeys.Add(Pot->GetPlantKey());
		}
	}
}

void UBotanicusBotanistNotebookWidget::RefreshNotebook()
{
	DiscoverPlantsInWorld();
	UGameInstance* GameInstance = GetGameInstance();
	UBotanicusPlantSubsystem* Plants = GameInstance
		? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
		: nullptr;
	PlantDefinitions = Plants
		? Plants->GetAllPlants()
		: TArray<FBotanicusPlantDefinition>();

	if (!PlantSelectorWidget)
	{
		return;
	}
	PlantSelectorWidget->ClearOptions();
	for (int32 Index = 0; Index < PlantDefinitions.Num(); ++Index)
	{
		const FBotanicusPlantDefinition& Definition = PlantDefinitions[Index];
		const bool bDiscovered = DiscoveredPlantKeys.Contains(Definition.PlantKey);
		PlantSelectorWidget->AddOption(
			bDiscovered
				? Definition.DisplayName.ToString()
				: FString::Printf(TEXT("??? [%02d]"), Index + 1));
	}
	if (!PlantDefinitions.IsEmpty())
	{
		PlantSelectorWidget->SetSelectedIndex(0);
		ShowPlantAtIndex(0);
	}
}

void UBotanicusBotanistNotebookWidget::ShowPlantAtIndex(int32 PlantIndex)
{
	if (!PlantDefinitions.IsValidIndex(PlantIndex))
	{
		return;
	}
	const FBotanicusPlantDefinition& Definition = PlantDefinitions[PlantIndex];
	if (!DiscoveredPlantKeys.Contains(Definition.PlantKey))
	{
		SetDetailText(PlantNameLabelWidget, FText::FromString(TEXT("ESPÈCE INCONNUE")));
		SetDetailText(PlantElementLabelWidget, FText::FromString(TEXT("Élément : ???")));
		SetDetailText(TemperatureLabelWidget, FText::FromString(TEXT("Température idéale : ???")));
		SetDetailText(AirHumidityLabelWidget, FText::FromString(TEXT("Humidité idéale : ???")));
		SetDetailText(LuminosityLabelWidget, FText::FromString(TEXT("Luminosité idéale : ???")));
		SetDetailText(WaterLabelWidget, FText::FromString(TEXT("Humidité du terreau : ???")));
		SetDetailText(GrowthLabelWidget, FText::FromString(TEXT("Temps de croissance : ???")));
		return;
	}

	const TCHAR* ElementName = TEXT("Normal");
	switch (Definition.Element)
	{
	case EBotanicusPlantElement::Fire: ElementName = TEXT("Feu"); break;
	case EBotanicusPlantElement::Water: ElementName = TEXT("Eau"); break;
	case EBotanicusPlantElement::Ice: ElementName = TEXT("Glace"); break;
	case EBotanicusPlantElement::Shadow: ElementName = TEXT("Ténèbres"); break;
	default: break;
	}
	SetDetailText(PlantNameLabelWidget, Definition.DisplayName);
	SetDetailText(PlantElementLabelWidget, FText::FromString(FString::Printf(
		TEXT("Élément : %s"), ElementName)));
	SetDetailText(TemperatureLabelWidget, FText::FromString(FString::Printf(
		TEXT("Température idéale : %.1f à %.1f °C"),
		Definition.Environment.MinimumIdealTemperatureCelsius,
		Definition.Environment.MaximumIdealTemperatureCelsius)));
	SetDetailText(AirHumidityLabelWidget, FText::FromString(FString::Printf(
		TEXT("Humidité de l'air idéale : %.0f à %.0f %%"),
		Definition.Environment.MinimumIdealAirHumidityPercent,
		Definition.Environment.MaximumIdealAirHumidityPercent)));
	SetDetailText(LuminosityLabelWidget, FText::FromString(FString::Printf(
		TEXT("Luminosité idéale : %.0f à %.0f %%"),
		Definition.Environment.MinimumIdealLuminosityPercent,
		Definition.Environment.MaximumIdealLuminosityPercent)));
	SetDetailText(WaterLabelWidget, FText::FromString(FString::Printf(
		TEXT("Humidité du terreau : %.0f à %.0f %%"),
		Definition.MinimumHealthyWater * 100.0f,
		Definition.MaximumHealthyWater * 100.0f)));
	SetDetailText(GrowthLabelWidget, FText::FromString(FString::Printf(
		TEXT("Temps de croissance : %.0f secondes"),
		Definition.GrowthDurationSeconds)));
}

void UBotanicusBotanistNotebookWidget::SetDetailText(
	UTextBlock* Label,
	const FText& Text) const
{
	if (Label)
	{
		Label->SetText(Text);
	}
}

void UBotanicusBotanistNotebookWidget::HandlePlantSelectionChanged(
	FString SelectedItem,
	ESelectInfo::Type SelectionType)
{
	(void)SelectedItem;
	(void)SelectionType;
	if (PlantSelectorWidget)
	{
		ShowPlantAtIndex(PlantSelectorWidget->GetSelectedIndex());
	}
}

void UBotanicusBotanistNotebookWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleBotanistNotebook();
	}
}
