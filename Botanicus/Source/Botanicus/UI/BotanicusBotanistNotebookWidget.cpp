// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusBotanistNotebookWidget.h"

#include "BotanicusPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace
{
FString ElementDisplayName(EBotanicusPlantElement Element)
{
	switch (Element)
	{
	case EBotanicusPlantElement::Fire: return TEXT("Feu");
	case EBotanicusPlantElement::Water: return TEXT("Eau");
	case EBotanicusPlantElement::Ice: return TEXT("Glace");
	case EBotanicusPlantElement::Shadow: return TEXT("Ténèbres");
	default: return TEXT("Normal");
	}
}

FString GrowthDurationDisplay(float Seconds)
{
	return Seconds >= 60.0f
		? FString::Printf(TEXT("%.0f min"), Seconds / 60.0f)
		: FString::Printf(TEXT("%.0f s"), Seconds);
}
}

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
	if (InKeyEvent.GetKey() == EKeys::I || InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (BotanicusController)
		{
			BotanicusController->ToggleBotanistNotebook();
		}
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Left)
	{
		HandlePreviousPageClicked();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Right)
	{
		HandleNextPageClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBotanicusBotanistNotebookWidget::BindPageWidgets(
	const TCHAR* Prefix,
	FPlantPageWidgets& OutWidgets)
{
	auto FindText = [this, Prefix](const TCHAR* Suffix)
	{
		return Cast<UTextBlock>(GetWidgetFromName(
			FName(*FString::Printf(TEXT("%s%s"), Prefix, Suffix))));
	};
	OutWidgets.Name = FindText(TEXT("PlantNameLabel"));
	OutWidgets.Illustration = FindText(TEXT("PlantIllustrationLabel"));
	OutWidgets.Element = FindText(TEXT("ElementLabel"));
	OutWidgets.Temperature = FindText(TEXT("TemperatureLabel"));
	OutWidgets.AirHumidity = FindText(TEXT("AirHumidityLabel"));
	OutWidgets.Luminosity = FindText(TEXT("LuminosityLabel"));
	OutWidgets.Water = FindText(TEXT("WaterLabel"));
	OutWidgets.Growth = FindText(TEXT("GrowthLabel"));
	OutWidgets.Note = FindText(TEXT("NoteLabel"));
}

void UBotanicusBotanistNotebookWidget::BindDesignerWidgets()
{
	CloseButtonWidget = Cast<UButton>(GetWidgetFromName(TEXT("CloseButton")));
	PreviousPageButtonWidget = Cast<UButton>(GetWidgetFromName(TEXT("PreviousPageButton")));
	NextPageButtonWidget = Cast<UButton>(GetWidgetFromName(TEXT("NextPageButton")));
	BindPageWidgets(TEXT("Left"), LeftPageWidgets);
	BindPageWidgets(TEXT("Right"), RightPageWidgets);

	if (CloseButtonWidget)
	{
		CloseButtonWidget->OnClicked.AddUniqueDynamic(
			this, &UBotanicusBotanistNotebookWidget::HandleCloseClicked);
	}
	if (PreviousPageButtonWidget)
	{
		PreviousPageButtonWidget->OnClicked.AddUniqueDynamic(
			this, &UBotanicusBotanistNotebookWidget::HandlePreviousPageClicked);
	}
	if (NextPageButtonWidget)
	{
		NextPageButtonWidget->OnClicked.AddUniqueDynamic(
			this, &UBotanicusBotanistNotebookWidget::HandleNextPageClicked);
	}

#define BOTANICUS_BIND_NOTEBOOK_LETTER(Letter) \
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Letter" #Letter "Button")))) \
	{ \
		Button->OnClicked.AddUniqueDynamic(this, \
			&UBotanicusBotanistNotebookWidget::HandleLetter##Letter##Clicked); \
		AlphabetButtons.Add(Button); \
	}
	BOTANICUS_BIND_NOTEBOOK_LETTER(A)
	BOTANICUS_BIND_NOTEBOOK_LETTER(B)
	BOTANICUS_BIND_NOTEBOOK_LETTER(C)
	BOTANICUS_BIND_NOTEBOOK_LETTER(D)
	BOTANICUS_BIND_NOTEBOOK_LETTER(E)
	BOTANICUS_BIND_NOTEBOOK_LETTER(F)
	BOTANICUS_BIND_NOTEBOOK_LETTER(G)
	BOTANICUS_BIND_NOTEBOOK_LETTER(H)
	BOTANICUS_BIND_NOTEBOOK_LETTER(I)
	BOTANICUS_BIND_NOTEBOOK_LETTER(J)
	BOTANICUS_BIND_NOTEBOOK_LETTER(K)
	BOTANICUS_BIND_NOTEBOOK_LETTER(L)
	BOTANICUS_BIND_NOTEBOOK_LETTER(M)
	BOTANICUS_BIND_NOTEBOOK_LETTER(N)
	BOTANICUS_BIND_NOTEBOOK_LETTER(O)
	BOTANICUS_BIND_NOTEBOOK_LETTER(P)
	BOTANICUS_BIND_NOTEBOOK_LETTER(Q)
	BOTANICUS_BIND_NOTEBOOK_LETTER(R)
	BOTANICUS_BIND_NOTEBOOK_LETTER(S)
	BOTANICUS_BIND_NOTEBOOK_LETTER(T)
	BOTANICUS_BIND_NOTEBOOK_LETTER(U)
	BOTANICUS_BIND_NOTEBOOK_LETTER(V)
	BOTANICUS_BIND_NOTEBOOK_LETTER(W)
	BOTANICUS_BIND_NOTEBOOK_LETTER(X)
	BOTANICUS_BIND_NOTEBOOK_LETTER(Y)
	BOTANICUS_BIND_NOTEBOOK_LETTER(Z)
#undef BOTANICUS_BIND_NOTEBOOK_LETTER
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
		else if (Pot && !Pot->GetPlantKey().IsNone())
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
	PlantDefinitions.StableSort([](
		const FBotanicusPlantDefinition& Left,
		const FBotanicusPlantDefinition& Right)
	{
		return Left.DisplayName.ToString().Compare(
			Right.DisplayName.ToString(), ESearchCase::IgnoreCase) < 0;
	});
	ApplyLetterFilter(ActiveLetterFilter);
}

void UBotanicusBotanistNotebookWidget::ApplyLetterFilter(TCHAR Letter)
{
	ActiveLetterFilter = Letter == 0 ? 0 : FChar::ToUpper(Letter);
	FilteredPlantIndices.Reset();

	for (int32 Index = 0; Index < PlantDefinitions.Num(); ++Index)
	{
		FString Name = PlantDefinitions[Index].DisplayName.ToString();
		Name.TrimStartAndEndInline();
		if (ActiveLetterFilter == 0 ||
			(!Name.IsEmpty() && FChar::ToUpper(Name[0]) == ActiveLetterFilter))
		{
			FilteredPlantIndices.Add(Index);
		}
	}

	CurrentPageStartIndex = 0;
	RefreshCurrentSpread();
}

void UBotanicusBotanistNotebookWidget::RefreshCurrentSpread()
{
	const int32 LeftPlantIndex = FilteredPlantIndices.IsValidIndex(
		CurrentPageStartIndex)
		? FilteredPlantIndices[CurrentPageStartIndex]
		: INDEX_NONE;
	const int32 RightPlantIndex = FilteredPlantIndices.IsValidIndex(
		CurrentPageStartIndex + 1)
		? FilteredPlantIndices[CurrentPageStartIndex + 1]
		: INDEX_NONE;
	ShowPlantOnPage(LeftPageWidgets, LeftPlantIndex);
	ShowPlantOnPage(RightPageWidgets, RightPlantIndex);
	if (PreviousPageButtonWidget)
	{
		PreviousPageButtonWidget->SetIsEnabled(CurrentPageStartIndex > 0);
	}
	if (NextPageButtonWidget)
	{
		NextPageButtonWidget->SetIsEnabled(
			CurrentPageStartIndex + 2 < FilteredPlantIndices.Num());
	}
}

void UBotanicusBotanistNotebookWidget::ShowPlantOnPage(
	const FPlantPageWidgets& Page,
	int32 PlantIndex) const
{
	auto ClearPage = [this, &Page]()
	{
		SetDetailText(Page.Name, FText::GetEmpty());
		SetDetailText(Page.Illustration, FText::GetEmpty());
		SetDetailText(Page.Element, FText::GetEmpty());
		SetDetailText(Page.Temperature, FText::GetEmpty());
		SetDetailText(Page.AirHumidity, FText::GetEmpty());
		SetDetailText(Page.Luminosity, FText::GetEmpty());
		SetDetailText(Page.Water, FText::GetEmpty());
		SetDetailText(Page.Growth, FText::GetEmpty());
		SetDetailText(Page.Note, FText::GetEmpty());
	};
	if (!PlantDefinitions.IsValidIndex(PlantIndex))
	{
		ClearPage();
		return;
	}

	const FBotanicusPlantDefinition& Definition = PlantDefinitions[PlantIndex];
	if (!DiscoveredPlantKeys.Contains(Definition.PlantKey))
	{
		SetDetailText(Page.Name, FText::FromString(TEXT("????????????")));
		SetDetailText(Page.Illustration, FText::FromString(TEXT("?")));
		SetDetailText(Page.Element, FText::FromString(TEXT("Type : ???")));
		SetDetailText(Page.Temperature, FText::FromString(TEXT("Température : ???")));
		SetDetailText(Page.AirHumidity, FText::FromString(TEXT("Humidité : ???")));
		SetDetailText(Page.Luminosity, FText::FromString(TEXT("Luminosité : ???")));
		SetDetailText(Page.Water, FText::FromString(TEXT("Terreau : ???")));
		SetDetailText(Page.Growth, FText::FromString(TEXT("Croissance : ???")));
		SetDetailText(Page.Note, FText::FromString(TEXT("Note : ???")));
		return;
	}

	SetDetailText(Page.Name, Definition.DisplayName);
	SetDetailText(Page.Illustration, FText::FromString(TEXT("ILLUSTRATION\nÀ AJOUTER")));
	SetDetailText(Page.Element, FText::FromString(FString::Printf(
		TEXT("Type : %s"), *ElementDisplayName(Definition.Element))));
	SetDetailText(Page.Temperature, FText::FromString(FString::Printf(
		TEXT("Température : %.0f - %.0f °C"),
		Definition.Environment.MinimumIdealTemperatureCelsius,
		Definition.Environment.MaximumIdealTemperatureCelsius)));
	SetDetailText(Page.AirHumidity, FText::FromString(FString::Printf(
		TEXT("Humidité : %.0f - %.0f %%"),
		Definition.Environment.MinimumIdealAirHumidityPercent,
		Definition.Environment.MaximumIdealAirHumidityPercent)));
	SetDetailText(Page.Luminosity, FText::FromString(FString::Printf(
		TEXT("Luminosité : %.0f - %.0f %%"),
		Definition.Environment.MinimumIdealLuminosityPercent,
		Definition.Environment.MaximumIdealLuminosityPercent)));
	SetDetailText(Page.Water, FText::FromString(FString::Printf(
		TEXT("Terreau : %.0f - %.0f %%"),
		Definition.MinimumHealthyWater * 100.0f,
		Definition.MaximumHealthyWater * 100.0f)));
	SetDetailText(Page.Growth, FText::FromString(FString::Printf(
		TEXT("Croissance : %s"), *GrowthDurationDisplay(Definition.GrowthDurationSeconds))));
	SetDetailText(Page.Note, BuildPlantNote(Definition));
}

FText UBotanicusBotanistNotebookWidget::BuildPlantNote(
	const FBotanicusPlantDefinition& Definition) const
{
	if (!Definition.CompatibleNeighbourPlantKeys.IsEmpty())
	{
		return FText::FromString(
			TEXT("Note : apprécie la présence de plantes compatibles à proximité."));
	}
	if (!Definition.IncompatibleNeighbourPlantKeys.IsEmpty())
	{
		return FText::FromString(
			TEXT("Note : préfère être éloignée de certaines espèces."));
	}
	return FText::FromString(TEXT("Note : aucune interaction particulière connue."));
}

void UBotanicusBotanistNotebookWidget::SetDetailText(
	const TWeakObjectPtr<UTextBlock>& Label,
	const FText& Text) const
{
	if (Label.IsValid())
	{
		Label->SetText(Text);
	}
}

void UBotanicusBotanistNotebookWidget::HandlePreviousPageClicked()
{
	CurrentPageStartIndex = FMath::Max(0, CurrentPageStartIndex - 2);
	RefreshCurrentSpread();
}

void UBotanicusBotanistNotebookWidget::HandleNextPageClicked()
{
	if (CurrentPageStartIndex + 2 < FilteredPlantIndices.Num())
	{
		CurrentPageStartIndex += 2;
		RefreshCurrentSpread();
	}
}

void UBotanicusBotanistNotebookWidget::JumpToLetter(TCHAR Letter)
{
	ApplyLetterFilter(Letter);
}

void UBotanicusBotanistNotebookWidget::HandleCloseClicked()
{
	if (BotanicusController)
	{
		BotanicusController->ToggleBotanistNotebook();
	}
}

#define BOTANICUS_DEFINE_NOTEBOOK_LETTER(Letter) \
	void UBotanicusBotanistNotebookWidget::HandleLetter##Letter##Clicked() \
	{ \
		JumpToLetter(TEXT(#Letter)[0]); \
	}
BOTANICUS_DEFINE_NOTEBOOK_LETTER(A)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(B)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(C)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(D)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(E)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(F)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(G)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(H)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(I)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(J)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(K)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(L)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(M)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(N)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(O)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(P)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(Q)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(R)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(S)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(T)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(U)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(V)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(W)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(X)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(Y)
BOTANICUS_DEFINE_NOTEBOOK_LETTER(Z)
#undef BOTANICUS_DEFINE_NOTEBOOK_LETTER
