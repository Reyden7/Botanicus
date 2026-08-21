// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/BotanicusCommandWidgetBuilder.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "UObject/SavePackage.h"
#include "UI/BotanicusOrderCatalogWidget.h"
#include "UI/BotanicusBotanistNotebookWidget.h"
#include "WidgetBlueprint.h"

namespace
{
UTexture2D* CommandTexture(const TCHAR* Name)
{
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(
		TEXT("/Game/Botanicus/UI/Command/Textures/%s.%s"), Name, Name));
}

template <typename T>
T* AddCanvasWidget(UWidgetTree* Tree, UCanvasPanel* Canvas, const FName Name,
	const FVector2D Position, const FVector2D Size, int32 ZOrder = 1)
{
	T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), Name);
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(ZOrder);
	return Widget;
}

UImage* AddImage(UWidgetTree* Tree, UCanvasPanel* Canvas, const FName Name,
	const TCHAR* Texture, const FVector2D Position, const FVector2D Size, int32 ZOrder = 1)
{
	UImage* Image = AddCanvasWidget<UImage>(Tree, Canvas, Name, Position, Size, ZOrder);
	if (UTexture2D* Asset = CommandTexture(Texture))
	{
		Image->SetBrushFromTexture(Asset, true);
	}
	return Image;
}

UTextBlock* AddText(UWidgetTree* Tree, UCanvasPanel* Canvas, const FName Name,
	const TCHAR* Text, const FVector2D Position, const FVector2D Size, int32 FontSize = 18)
{
	UTextBlock* Label = AddCanvasWidget<UTextBlock>(Tree, Canvas, Name, Position, Size, 3);
	Label->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.60f)));
	return Label;
}

UButton* AddVisualButton(UWidgetTree* Tree, UCanvasPanel* Canvas, const FName Name,
	const TCHAR* Texture, const TCHAR* Text, const FVector2D Position, const FVector2D Size)
{
	UButton* Button = AddCanvasWidget<UButton>(Tree, Canvas, Name, Position, Size, 2);
	FButtonStyle Style = Button->GetStyle();
	FSlateBrush Brush;
	Brush.SetResourceObject(CommandTexture(Texture));
	Brush.ImageSize = Size;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Style.SetNormal(Brush);
	Style.SetHovered(Brush);
	Style.SetPressed(Brush);
	Button->SetStyle(Style);
	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(Text));
	Label->SetJustification(ETextJustify::Center);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.60f)));
	Button->AddChild(Label);
	return Button;
}
}
#endif

bool UBotanicusCommandWidgetBuilder::RebuildCommandComputerWidget()
{
#if WITH_EDITOR
	const TCHAR* AssetPath = TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer.WBP_CommandComputer");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, AssetPath);
	if (!Blueprint)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer"));
		Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			UBotanicusOrderCatalogWidget::StaticClass(), Package, TEXT("WBP_CommandComputer"),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	}
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CommandDesignerRoot"));
	Tree->RootWidget = Root;
	AddImage(Tree, Root, TEXT("ComputerFrame"), TEXT("T_Command_Frame"), FVector2D::ZeroVector, FVector2D(1920, 1080), 0);
	AddImage(Tree, Root, TEXT("ScreenBackground"), TEXT("T_Command_ShopPanel"), FVector2D(78, 56), FVector2D(1724, 986), 1);
	AddImage(Tree, Root, TEXT("Logo"), TEXT("T_Command_Nature"), FVector2D(96, 78), FVector2D(48, 48), 2);
	AddText(Tree, Root, TEXT("TitleLabel"), TEXT("PANNEAU DE COMMANDE"), FVector2D(150, 72), FVector2D(570, 48), 30);
	AddText(Tree, Root, TEXT("SubtitleLabel"), TEXT("TERMINAL DE GESTION - BOTANICUS"), FVector2D(150, 114), FVector2D(400, 26), 14);
	AddImage(Tree, Root, TEXT("CreditsCard"), TEXT("T_Command_CreditsBackground"), FVector2D(1115, 78), FVector2D(158, 50), 2);
	AddText(Tree, Root, TEXT("Designer_FundsLabel"), TEXT("746\nCREDITS"), FVector2D(1155, 83), FVector2D(105, 42), 15);
	AddImage(Tree, Root, TEXT("LevelCard"), TEXT("T_Command_LevelBackground"), FVector2D(1280, 78), FVector2D(168, 50), 2);
	AddText(Tree, Root, TEXT("Designer_LevelLabel"), TEXT("NIVEAU 1\nBOUTIQUE"), FVector2D(1320, 83), FVector2D(120, 42), 15);
	UButton* ShopButton = AddVisualButton(Tree, Root, TEXT("Designer_ShopOpenButton"), TEXT("T_Command_ShopClosedBackground"), TEXT("MAGASIN FERME"), FVector2D(1455, 73), FVector2D(190, 60));
	AddImage(Tree, Root, TEXT("Designer_ShopStateBackground"), TEXT("T_Command_ShopClosedBackground"), FVector2D(1455, 73), FVector2D(190, 60), 1)->SetVisibility(ESlateVisibility::Collapsed);
	AddImage(Tree, Root, TEXT("Designer_ShopStateIcon"), TEXT("T_Command_ShopClosedIcon"), FVector2D(1468, 89), FVector2D(28, 28), 3)->SetVisibility(ESlateVisibility::Collapsed);
	AddText(Tree, Root, TEXT("Designer_ShopOpenButtonLabel"), TEXT("MAGASIN FERME"), FVector2D(1498, 88), FVector2D(130, 30), 13)->SetVisibility(ESlateVisibility::Collapsed);
	AddVisualButton(Tree, Root, TEXT("CloseButton"), TEXT("T_Command_CloseNormal"), TEXT("FERMER"), FVector2D(1655, 73), FVector2D(125, 60));

	const TCHAR* TabNames[] = {TEXT("SeedsTab"), TEXT("ToolsTab"), TEXT("PreparationTab"), TEXT("SalesTab"), TEXT("BuildingsTab")};
	const TCHAR* TabLabels[] = {TEXT("GRAINES"), TEXT("OUTILS DE JARDINAGE"), TEXT("PREPARATION"), TEXT("VENTE"), TEXT("BATIMENTS")};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		AddVisualButton(Tree, Root, TabNames[Index], TEXT("T_Command_TabBackground"), TabLabels[Index], FVector2D(104 + Index * 336, 154), FVector2D(310, 58));
	}

	UScrollBox* ItemsScroll = AddCanvasWidget<UScrollBox>(Tree, Root, TEXT("Designer_ItemsScroll"), FVector2D(100, 230), FVector2D(1380, 775), 2);
	ItemsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* ItemsBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Designer_ItemsBox"));
	ItemsScroll->AddChild(ItemsBox);
	AddCanvasWidget<USlider>(Tree, Root, TEXT("Designer_CatalogScrollSlider"), FVector2D(1490, 475), FVector2D(26, 360), 3)->SetOrientation(Orient_Vertical);
	AddVisualButton(Tree, Root, TEXT("ScrollUpButton"), TEXT("T_Command_TabBackground"), TEXT("^"), FVector2D(1487, 440), FVector2D(32, 32));
	AddVisualButton(Tree, Root, TEXT("ScrollDownButton"), TEXT("T_Command_TabBackground"), TEXT("v"), FVector2D(1487, 840), FVector2D(32, 32));

	AddImage(Tree, Root, TEXT("DeliveryPanel"), TEXT("T_Command_ShopPanel"), FVector2D(1530, 230), FVector2D(250, 775), 1);
	AddText(Tree, Root, TEXT("DeliveryTitle"), TEXT("LIVRAISONS EN COURS"), FVector2D(1550, 250), FVector2D(210, 30), 14);
	AddText(Tree, Root, TEXT("Designer_PendingOrdersLabel"), TEXT("Aucune livraison en attente."), FVector2D(1550, 286), FVector2D(210, 35), 11);
	UVerticalBox* Pending = AddCanvasWidget<UVerticalBox>(Tree, Root, TEXT("Designer_PendingOrdersBox"), FVector2D(1545, 325), FVector2D(220, 650), 3);
	Pending->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Blueprint);
	return UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *FPackageName::LongPackageNameToFilename(
		TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer"), FPackageName::GetAssetPackageExtension()), FSavePackageArgs());
#else
	return false;
#endif
}

bool UBotanicusCommandWidgetBuilder::RebuildBotanistNotebookWidget()
{
#if WITH_EDITOR
	const TCHAR* ObjectPath =
		TEXT("/Game/Botanicus/UI/Botanist/WBP_BotanistNotebook.WBP_BotanistNotebook");
	const TCHAR* PackagePath =
		TEXT("/Game/Botanicus/UI/Botanist/WBP_BotanistNotebook");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, ObjectPath);
	if (!Blueprint)
	{
		UPackage* Package = CreatePackage(PackagePath);
		Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			UBotanicusBotanistNotebookWidget::StaticClass(),
			Package,
			TEXT("WBP_BotanistNotebook"),
			BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(),
			UWidgetBlueprintGeneratedClass::StaticClass()));
	}
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BotanistNotebookDesignerRoot"));
	Tree->RootWidget = Root;

	UBorder* Backdrop = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("Backdrop"), FVector2D::ZeroVector,
		FVector2D(1920.0f, 1080.0f), 0);
	Backdrop->SetBrushColor(FLinearColor(0.01f, 0.018f, 0.012f, 0.82f));

	UBorder* NotebookPanel = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("NotebookPanel"), FVector2D(185.0f, 80.0f),
		FVector2D(1550.0f, 920.0f), 1);
	NotebookPanel->SetBrushColor(FLinearColor(0.055f, 0.12f, 0.075f, 0.98f));

	UTextBlock* Title = AddText(
		Tree, Root, TEXT("NotebookTitle"), TEXT("CARNET DE BOTANISTE"),
		FVector2D(255.0f, 125.0f), FVector2D(800.0f, 70.0f), 42);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.84f, 0.48f)));
	AddText(Tree, Root, TEXT("NotebookSubtitle"),
		TEXT("Les espèces observées apparaissent progressivement dans le carnet."),
		FVector2D(258.0f, 190.0f), FVector2D(950.0f, 38.0f), 19);

	UButton* CloseButton = AddCanvasWidget<UButton>(
		Tree, Root, TEXT("CloseButton"), FVector2D(1450.0f, 125.0f),
		FVector2D(205.0f, 62.0f), 3);
	UTextBlock* CloseLabel = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CloseButtonLabel"));
	CloseLabel->SetText(FText::FromString(TEXT("FERMER  [I]")));
	CloseLabel->SetJustification(ETextJustify::Center);
	FSlateFontInfo CloseFont = CloseLabel->GetFont();
	CloseFont.Size = 22;
	CloseLabel->SetFont(CloseFont);
	CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CloseButton->AddChild(CloseLabel);

	AddText(Tree, Root, TEXT("SpeciesListTitle"), TEXT("ESPÈCES"),
		FVector2D(265.0f, 280.0f), FVector2D(480.0f, 45.0f), 25);
	UComboBoxString* Selector = AddCanvasWidget<UComboBoxString>(
		Tree, Root, TEXT("PlantSelector"), FVector2D(260.0f, 335.0f),
		FVector2D(520.0f, 58.0f), 3);
	Selector->AddOption(TEXT("??? [01]"));
	Selector->SetSelectedIndex(0);

	UBorder* DetailPanel = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("DetailPanel"), FVector2D(825.0f, 270.0f),
		FVector2D(815.0f, 620.0f), 2);
	DetailPanel->SetBrushColor(FLinearColor(0.025f, 0.055f, 0.037f, 0.95f));

	UTextBlock* PlantName = AddText(Tree, Root, TEXT("PlantNameLabel"),
		TEXT("ESPÈCE INCONNUE"), FVector2D(885.0f, 315.0f),
		FVector2D(680.0f, 62.0f), 32);
	PlantName->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.95f, 0.56f)));
	AddText(Tree, Root, TEXT("PlantElementLabel"), TEXT("Élément : ???"),
		FVector2D(890.0f, 395.0f), FVector2D(650.0f, 42.0f), 21);
	AddText(Tree, Root, TEXT("TemperatureLabel"), TEXT("Température idéale : ???"),
		FVector2D(890.0f, 470.0f), FVector2D(650.0f, 42.0f), 21);
	AddText(Tree, Root, TEXT("AirHumidityLabel"), TEXT("Humidité idéale : ???"),
		FVector2D(890.0f, 535.0f), FVector2D(650.0f, 42.0f), 21);
	AddText(Tree, Root, TEXT("LuminosityLabel"), TEXT("Luminosité idéale : ???"),
		FVector2D(890.0f, 600.0f), FVector2D(650.0f, 42.0f), 21);
	AddText(Tree, Root, TEXT("WaterLabel"), TEXT("Humidité du terreau : ???"),
		FVector2D(890.0f, 665.0f), FVector2D(650.0f, 42.0f), 21);
	AddText(Tree, Root, TEXT("GrowthLabel"), TEXT("Temps de croissance : ???"),
		FVector2D(890.0f, 730.0f), FVector2D(650.0f, 42.0f), 21);
	AddText(Tree, Root, TEXT("NotebookHint"),
		TEXT("Placez une plante dans un pot pour découvrir sa fiche."),
		FVector2D(265.0f, 870.0f), FVector2D(1120.0f, 42.0f), 18);

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Blueprint);
	return UPackage::SavePackage(
		Blueprint->GetOutermost(), Blueprint,
		*FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension()),
		FSavePackageArgs());
#else
	return false;
#endif
}

bool UBotanicusCommandWidgetBuilder::MakeAnimeWaterLocalSpace()
{
#if WITH_EDITOR
	const TCHAR* AssetPath =
		TEXT("/Game/Botanicus/VFX/Watering/NS_AnimeWater.NS_AnimeWater");
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, AssetPath);
	if (!System)
	{
		return false;
	}

	System->Modify();
	bool bFoundEmitter = false;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (!EmitterData)
		{
			continue;
		}

		bFoundEmitter = true;
		if (UNiagaraEmitter* Emitter =
				Cast<UNiagaraEmitter>(Handle.GetEmitterBase()))
		{
			Emitter->Modify();
		}
		EmitterData->bLocalSpace = true;
	}

	if (!bFoundEmitter)
	{
		return false;
	}

	System->RequestCompile(true);
	System->WaitForCompilationComplete(true, false);
	System->MarkPackageDirty();
	const FString PackageName = System->GetOutermost()->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(
		System->GetOutermost(),
		System,
		*PackageFilename,
		FSavePackageArgs());
#else
	return false;
#endif
}

