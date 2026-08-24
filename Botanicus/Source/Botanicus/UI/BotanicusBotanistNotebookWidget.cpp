// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusBotanistNotebookWidget.h"

#include "BotanicusPlayerController.h"
#include "BotanicusGameState.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
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

FString EscapeRichText(FString Text)
{
	Text.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	Text.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	Text.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	return Text;
}

float DiseaseDelaySeconds(
	const FBotanicusPlantDefinition& Plant,
	const FBotanicusPlantDiseaseDefinition& Disease)
{
	if (Disease.bElementSpecific)
	{
		return Plant.DiseaseSusceptibility.ElementNeglectDelaySeconds;
	}
	if (Disease.DiseaseKey == TEXT("Disease_Overwatering"))
	{
		return Plant.DiseaseSusceptibility.OverwateringDelaySeconds;
	}
	if (Disease.DiseaseKey == TEXT("Disease_Humidity"))
	{
		return Plant.DiseaseSusceptibility.IncorrectHumidityDelaySeconds;
	}
	if (Disease.DiseaseKey == TEXT("Disease_ExcessLight"))
	{
		return Plant.DiseaseSusceptibility.ExcessLightDelaySeconds;
	}
	return 0.0f;
}

UTexture2D* PlantElementIcon(EBotanicusPlantElement Element)
{
	const TCHAR* AssetName = TEXT("T_PlantElement_Normal");
	switch (Element)
	{
	case EBotanicusPlantElement::Fire: AssetName = TEXT("T_PlantElement_Fire"); break;
	case EBotanicusPlantElement::Water: AssetName = TEXT("T_PlantElement_Water"); break;
	case EBotanicusPlantElement::Ice: AssetName = TEXT("T_PlantElement_Ice"); break;
	case EBotanicusPlantElement::Shadow: AssetName = TEXT("T_PlantElement_Shadow"); break;
	default: break;
	}
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(
		TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/%s.%s"),
		AssetName, AssetName));
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
	auto FindImage = [this, Prefix](const TCHAR* Suffix)
	{
		return Cast<UImage>(GetWidgetFromName(
			FName(*FString::Printf(TEXT("%s%s"), Prefix, Suffix))));
	};
	auto FindRichText = [this, Prefix](const TCHAR* Suffix)
	{
		return Cast<URichTextBlock>(GetWidgetFromName(
			FName(*FString::Printf(TEXT("%s%s"), Prefix, Suffix))));
	};
	OutWidgets.Name = FindText(TEXT("PlantNameLabel"));
	OutWidgets.Illustration = FindText(TEXT("PlantIllustrationLabel"));
	OutWidgets.IllustrationImage = FindImage(TEXT("PlantImage"));
	OutWidgets.ElementIcon = FindImage(TEXT("ElementIcon"));
	OutWidgets.TemperatureIcon = FindImage(TEXT("TemperatureIcon"));
	OutWidgets.AirHumidityIcon = FindImage(TEXT("AirHumidityIcon"));
	OutWidgets.LuminosityIcon = FindImage(TEXT("LuminosityIcon"));
	OutWidgets.WaterIcon = FindImage(TEXT("WaterIcon"));
	OutWidgets.GrowthIcon = FindImage(TEXT("GrowthIcon"));
	OutWidgets.Element = FindText(TEXT("ElementLabel"));
	OutWidgets.Temperature = FindText(TEXT("TemperatureLabel"));
	OutWidgets.AirHumidity = FindText(TEXT("AirHumidityLabel"));
	OutWidgets.Luminosity = FindText(TEXT("LuminosityLabel"));
	OutWidgets.Water = FindText(TEXT("WaterLabel"));
	OutWidgets.Growth = FindText(TEXT("GrowthLabel"));
	OutWidgets.Note = FindRichText(TEXT("NoteLabel"));
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

#define BOTANICUS_BIND_NOTEBOOK_ELEMENT(Name) \
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Element" #Name "Button")))) \
	{ \
		Button->OnClicked.AddUniqueDynamic(this, \
			&UBotanicusBotanistNotebookWidget::HandleElement##Name##Clicked); \
		ElementButtons.Add(Button); \
	}
	BOTANICUS_BIND_NOTEBOOK_ELEMENT(All)
	BOTANICUS_BIND_NOTEBOOK_ELEMENT(Normal)
	BOTANICUS_BIND_NOTEBOOK_ELEMENT(Fire)
	BOTANICUS_BIND_NOTEBOOK_ELEMENT(Water)
	BOTANICUS_BIND_NOTEBOOK_ELEMENT(Shadow)
	BOTANICUS_BIND_NOTEBOOK_ELEMENT(Ice)
#undef BOTANICUS_BIND_NOTEBOOK_ELEMENT
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
	RebuildFilteredPlantIndices();
	CurrentPageStartIndex = FMath::Clamp(
		CurrentPageStartIndex, 0,
		FMath::Max(0, FilteredPlantIndices.Num() - 1));
	RefreshCurrentSpread();
}

void UBotanicusBotanistNotebookWidget::RebuildFilteredPlantIndices()
{
	FilteredPlantIndices.Reset();
	for (int32 Index = 0; Index < PlantDefinitions.Num(); ++Index)
	{
		if (!ActiveElementFilter.IsSet() ||
			PlantDefinitions[Index].Element == ActiveElementFilter.GetValue())
		{
			FilteredPlantIndices.Add(Index);
		}
	}
}

void UBotanicusBotanistNotebookWidget::ApplyLetterFilter(TCHAR Letter)
{
	const TCHAR RequestedLetter = FChar::ToUpper(Letter);
	RebuildFilteredPlantIndices();
	int32 BestPosition = INDEX_NONE;
	for (int32 Position = 0; Position < FilteredPlantIndices.Num(); ++Position)
	{
		FString Name = PlantDefinitions[FilteredPlantIndices[Position]].DisplayName.ToString();
		Name.TrimStartAndEndInline();
		if (!Name.IsEmpty() && FChar::ToUpper(Name[0]) >= RequestedLetter)
		{
			BestPosition = Position;
			break;
		}
	}
	if (BestPosition == INDEX_NONE && !FilteredPlantIndices.IsEmpty())
	{
		BestPosition = FMath::Max(0, FilteredPlantIndices.Num() - 2);
	}
	CurrentPageStartIndex = FMath::Max(0, BestPosition);
	RefreshCurrentSpread();
}

void UBotanicusBotanistNotebookWidget::ApplyElementFilter(
	EBotanicusPlantElement Element)
{
	ActiveElementFilter = Element;
	RebuildFilteredPlantIndices();
	CurrentPageStartIndex = 0;
	RefreshCurrentSpread();
}

void UBotanicusBotanistNotebookWidget::UpdateTabVisuals()
{
	bool bAvailableLetters[26] = {};
	for (const FBotanicusPlantDefinition& Plant : PlantDefinitions)
	{
		if (ActiveElementFilter.IsSet() &&
			Plant.Element != ActiveElementFilter.GetValue())
		{
			continue;
		}

		FString Name = Plant.DisplayName.ToString();
		Name.TrimStartAndEndInline();
		if (!Name.IsEmpty())
		{
			const TCHAR FirstLetter = FChar::ToUpper(Name[0]);
			if (FirstLetter >= TEXT('A') && FirstLetter <= TEXT('Z'))
			{
				bAvailableLetters[FirstLetter - TEXT('A')] = true;
			}
		}
	}

	for (int32 Index = 0; Index < AlphabetButtons.Num(); ++Index)
	{
		if (AlphabetButtons[Index].IsValid())
		{
			const bool bAvailable = Index < 26 && bAvailableLetters[Index];
			const bool bActive = ActiveLetterFilter != 0 &&
				Index == static_cast<int32>(ActiveLetterFilter - TEXT('A'));
			AlphabetButtons[Index]->SetIsEnabled(bAvailable);
			AlphabetButtons[Index]->SetBackgroundColor(!bAvailable
				? FLinearColor(0.28f, 0.28f, 0.28f, 0.72f)
				: bActive
					? FLinearColor(0.55f, 0.82f, 0.42f, 1.0f)
					: FLinearColor::White);
		}
	}
	const EBotanicusPlantElement Elements[] = {
		EBotanicusPlantElement::Normal, EBotanicusPlantElement::Fire,
		EBotanicusPlantElement::Water, EBotanicusPlantElement::Shadow,
		EBotanicusPlantElement::Ice};
	for (int32 Index = 0; Index < ElementButtons.Num(); ++Index)
	{
		if (ElementButtons[Index].IsValid())
		{
			const bool bActive = Index == 0
				? !ActiveElementFilter.IsSet()
				: ActiveElementFilter.IsSet() &&
					ActiveElementFilter.GetValue() == Elements[Index - 1];
			ElementButtons[Index]->SetBackgroundColor(bActive
				? FLinearColor(0.55f, 0.82f, 0.42f, 1.0f)
				: FLinearColor::White);
		}
	}
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
	ActiveLetterFilter = 0;
	if (PlantDefinitions.IsValidIndex(LeftPlantIndex))
	{
		FString Name = PlantDefinitions[LeftPlantIndex].DisplayName.ToString();
		Name.TrimStartAndEndInline();
		if (!Name.IsEmpty())
		{
			ActiveLetterFilter = FChar::ToUpper(Name[0]);
		}
	}
	UpdateTabVisuals();
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
		SetNoteText(Page.Note, FText::GetEmpty());
		const TWeakObjectPtr<UImage> Images[] = {
			Page.IllustrationImage, Page.ElementIcon, Page.TemperatureIcon,
			Page.AirHumidityIcon, Page.LuminosityIcon, Page.WaterIcon,
			Page.GrowthIcon};
		for (const TWeakObjectPtr<UImage>& Image : Images)
		{
			if (Image.IsValid()) Image->SetVisibility(ESlateVisibility::Collapsed);
		}
	};
	if (!PlantDefinitions.IsValidIndex(PlantIndex))
	{
		ClearPage();
		return;
	}

	const FBotanicusPlantDefinition& Definition = PlantDefinitions[PlantIndex];
	const TWeakObjectPtr<UImage> DetailIcons[] = {
		Page.ElementIcon, Page.TemperatureIcon, Page.AirHumidityIcon,
		Page.LuminosityIcon, Page.WaterIcon, Page.GrowthIcon};
	for (const TWeakObjectPtr<UImage>& Icon : DetailIcons)
	{
		if (Icon.IsValid()) Icon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
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
		SetNoteText(Page.Note, FText::FromString(TEXT("Note : ???")));
		if (Page.IllustrationImage.IsValid())
		{
			Page.IllustrationImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		return;
	}

	if (Page.IllustrationImage.IsValid())
	{
		Page.IllustrationImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Page.ElementIcon.IsValid())
	{
		Page.ElementIcon->SetBrushFromTexture(PlantElementIcon(Definition.Element), true);
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
	SetNoteText(Page.Note, BuildPlantNote(Definition));
}

FText UBotanicusBotanistNotebookWidget::BuildPlantNote(
	const FBotanicusPlantDefinition& Definition) const
{
	TArray<FString> NoteLines;
	if (!Definition.CompatibleNeighbourPlantKeys.IsEmpty())
	{
		NoteLines.Add(
			TEXT("Note : apprécie la présence de plantes compatibles à proximité."));
	}
	else if (!Definition.IncompatibleNeighbourPlantKeys.IsEmpty())
	{
		NoteLines.Add(
			TEXT("Note : préfère être éloignée de certaines espèces."));
	}
	else
	{
		NoteLines.Add(TEXT("Note : aucune interaction particulière connue."));
	}

	const ABotanicusGameState* GameState = GetWorld()
		? GetWorld()->GetGameState<ABotanicusGameState>()
		: nullptr;
	const UBotanicusItemCatalogSubsystem* ItemCatalog = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>()
		: nullptr;
	if (GameState)
	{
		for (const FBotanicusPlantDiseaseDefinition& Disease :
			GetBotanicusPlantDiseaseDefinitions())
		{
			const bool bRelevantToPlant =
				!Disease.bElementSpecific || Disease.Element == Definition.Element;
			if (!bRelevantToPlant ||
				!GameState->IsDiseaseDiscovered(Disease.DiseaseKey))
			{
				continue;
			}
			FString ProductName = Disease.TreatmentItemKey.ToString();
			if (ItemCatalog)
			{
				if (const FBotanicusItemDefinition* Product =
					ItemCatalog->FindItem(Disease.TreatmentItemKey))
				{
					ProductName = Product->DisplayName.ToString();
				}
			}
			const float DelaySeconds = DiseaseDelaySeconds(Definition, Disease);
			NoteLines.Add(FString::Printf(
				TEXT("Maladie : <b>%s</>\nCause : %s\nDurée avant apparition : <b>%s</>\nTraitement : <b>%s</>\nProduit : <b>%s</>"),
				*EscapeRichText(Disease.DisplayName.ToString()),
				*EscapeRichText(Disease.CauseDescription.ToString()),
				*EscapeRichText(GrowthDurationDisplay(DelaySeconds)),
				*EscapeRichText(Disease.TreatmentDescription.ToString()),
				*EscapeRichText(ProductName)));
		}
	}
	return FText::FromString(FString::Join(NoteLines, TEXT("\n\n")));
}

void UBotanicusBotanistNotebookWidget::SetNoteText(
	const TWeakObjectPtr<URichTextBlock>& Label,
	const FText& Text) const
{
	if (Label.IsValid())
	{
		Label->SetText(Text);
	}
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

void UBotanicusBotanistNotebookWidget::HandleElementAllClicked()
{
	ActiveElementFilter.Reset();
	RebuildFilteredPlantIndices();
	CurrentPageStartIndex = 0;
	RefreshCurrentSpread();
}

void UBotanicusBotanistNotebookWidget::HandleElementNormalClicked()
{
	ApplyElementFilter(EBotanicusPlantElement::Normal);
}

void UBotanicusBotanistNotebookWidget::HandleElementFireClicked()
{
	ApplyElementFilter(EBotanicusPlantElement::Fire);
}

void UBotanicusBotanistNotebookWidget::HandleElementWaterClicked()
{
	ApplyElementFilter(EBotanicusPlantElement::Water);
}

void UBotanicusBotanistNotebookWidget::HandleElementShadowClicked()
{
	ApplyElementFilter(EBotanicusPlantElement::Shadow);
}

void UBotanicusBotanistNotebookWidget::HandleElementIceClicked()
{
	ApplyElementFilter(EBotanicusPlantElement::Ice);
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
