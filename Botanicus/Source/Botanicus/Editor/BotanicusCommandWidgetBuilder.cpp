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
	Backdrop->SetBrushColor(FLinearColor(0.01f, 0.018f, 0.012f, 0.72f));

	// Neutral structural placeholders. The final book, paper, corners and
	// foliage textures can be assigned directly to these WBP elements later.
	UBorder* BookCover = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("BookCover"), FVector2D(170.0f, 42.0f),
		FVector2D(1580.0f, 996.0f), 1);
	BookCover->SetBrushColor(FLinearColor(0.055f, 0.15f, 0.09f, 1.0f));
	UBorder* LeftPage = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("LeftPageBackground"), FVector2D(215.0f, 72.0f),
		FVector2D(715.0f, 925.0f), 2);
	LeftPage->SetBrushColor(FLinearColor(0.78f, 0.70f, 0.52f, 1.0f));
	UBorder* RightPage = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("RightPageBackground"), FVector2D(945.0f, 72.0f),
		FVector2D(715.0f, 925.0f), 2);
	RightPage->SetBrushColor(FLinearColor(0.80f, 0.72f, 0.55f, 1.0f));
	UBorder* Spine = AddCanvasWidget<UBorder>(
		Tree, Root, TEXT("BookSpine"), FVector2D(925.0f, 68.0f),
		FVector2D(40.0f, 935.0f), 3);
	Spine->SetBrushColor(FLinearColor(0.16f, 0.10f, 0.045f, 0.88f));

	UButton* CloseButton = AddCanvasWidget<UButton>(
		Tree, Root, TEXT("CloseButton"), FVector2D(1748.0f, 46.0f),
		FVector2D(125.0f, 48.0f), 6);
	UTextBlock* CloseLabel = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CloseButtonLabel"));
	CloseLabel->SetText(FText::FromString(TEXT("FERMER [I]")));
	CloseLabel->SetJustification(ETextJustify::Center);
	FSlateFontInfo CloseFont = CloseLabel->GetFont();
	CloseFont.Size = 16;
	CloseLabel->SetFont(CloseFont);
	CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CloseButton->AddChild(CloseLabel);

	auto AddPage = [Tree, Root](const TCHAR* Prefix, float X)
	{
		auto Named = [Prefix](const TCHAR* Suffix)
		{
			return FName(*FString::Printf(TEXT("%s%s"), Prefix, Suffix));
		};
		UTextBlock* Name = AddText(Tree, Root, Named(TEXT("PlantNameLabel")),
			TEXT("????????????"), FVector2D(X + 65.0f, 105.0f),
			FVector2D(585.0f, 55.0f), 28);
		Name->SetJustification(ETextJustify::Center);
		Name->SetColorAndOpacity(FSlateColor(FLinearColor(0.20f, 0.14f, 0.07f)));

		UBorder* IllustrationFrame = AddCanvasWidget<UBorder>(
			Tree, Root, Named(TEXT("IllustrationFrame")),
			FVector2D(X + 80.0f, 175.0f), FVector2D(555.0f, 330.0f), 3);
		IllustrationFrame->SetBrushColor(FLinearColor(0.58f, 0.54f, 0.39f, 0.55f));
		UImage* PlantImage = AddCanvasWidget<UImage>(
			Tree, Root, Named(TEXT("PlantImage")),
			FVector2D(X + 95.0f, 190.0f), FVector2D(525.0f, 300.0f), 4);
		PlantImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
		UTextBlock* Illustration = AddText(
			Tree, Root, Named(TEXT("PlantIllustrationLabel")), TEXT("?"),
			FVector2D(X + 125.0f, 255.0f), FVector2D(465.0f, 170.0f), 72);
		Illustration->SetJustification(ETextJustify::Center);
		Illustration->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.35f, 0.31f, 0.22f, 0.75f)));

		const TCHAR* Suffixes[] = {
			TEXT("Element"), TEXT("Temperature"), TEXT("AirHumidity"),
			TEXT("Luminosity"), TEXT("Water"), TEXT("Growth")};
		const TCHAR* Defaults[] = {
			TEXT("Type : ???"), TEXT("Température : ???"), TEXT("Humidité : ???"),
			TEXT("Luminosité : ???"), TEXT("Terreau : ???"), TEXT("Croissance : ???")};
		for (int32 Index = 0; Index < 6; ++Index)
		{
			AddCanvasWidget<UImage>(Tree, Root,
				Named(*FString::Printf(TEXT("%sIcon"), Suffixes[Index])),
				FVector2D(X + 58.0f, 535.0f + Index * 48.0f),
				FVector2D(30.0f, 30.0f), 4);
			UTextBlock* Detail = AddText(Tree, Root,
				Named(*FString::Printf(TEXT("%sLabel"), Suffixes[Index])),
				Defaults[Index], FVector2D(X + 100.0f, 532.0f + Index * 48.0f),
				FVector2D(520.0f, 38.0f), 18);
			Detail->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.18f, 0.12f, 0.055f)));
		}

		UBorder* NoteFrame = AddCanvasWidget<UBorder>(
			Tree, Root, Named(TEXT("NoteFrame")), FVector2D(X + 52.0f, 830.0f),
			FVector2D(610.0f, 110.0f), 3);
		NoteFrame->SetBrushColor(FLinearColor(0.62f, 0.56f, 0.40f, 0.42f));
		UTextBlock* Note = AddText(Tree, Root, Named(TEXT("NoteLabel")),
			TEXT("Note : ???"), FVector2D(X + 75.0f, 852.0f),
			FVector2D(565.0f, 70.0f), 16);
		Note->SetAutoWrapText(true);
		Note->SetColorAndOpacity(FSlateColor(FLinearColor(0.18f, 0.12f, 0.055f)));
	};
	AddPage(TEXT("Left"), 215.0f);
	AddPage(TEXT("Right"), 945.0f);

	auto AddArrowButton = [Tree, Root](const FName Name, const TCHAR* Label,
		const FVector2D Position)
	{
		UButton* Button = AddCanvasWidget<UButton>(
			Tree, Root, Name, Position, FVector2D(72.0f, 72.0f), 6);
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 35;
		Text->SetFont(Font);
		Button->AddChild(Text);
	};
	AddArrowButton(TEXT("PreviousPageButton"), TEXT("<"), FVector2D(238.0f, 905.0f));
	AddArrowButton(TEXT("NextPageButton"), TEXT(">"), FVector2D(1565.0f, 905.0f));

	for (int32 Index = 0; Index < 26; ++Index)
	{
		const TCHAR Letter = static_cast<TCHAR>('A' + Index);
		const FName ButtonName(*FString::Printf(TEXT("Letter%cButton"), Letter));
		UButton* Button = AddCanvasWidget<UButton>(
			Tree, Root, ButtonName, FVector2D(1662.0f, 82.0f + Index * 35.0f),
			FVector2D(48.0f, 33.0f), 5);
		UTextBlock* LetterLabel = Tree->ConstructWidget<UTextBlock>();
		LetterLabel->SetText(FText::FromString(FString::Chr(Letter)));
		LetterLabel->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = LetterLabel->GetFont();
		Font.Size = 15;
		LetterLabel->SetFont(Font);
		LetterLabel->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.18f, 0.12f, 0.055f)));
		Button->AddChild(LetterLabel);
	}

	// The tree is rebuilt programmatically. Reset the editor-only widget GUID map
	// so the compiler can regenerate deterministic GUIDs for every new widget.
	// Keeping the map from the previous layout makes every newly named widget
	// trigger an ensure in WidgetBlueprintCompiler.
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// The previous disconnected widgets still exist as transient objects until
	// the package is reloaded, so the compiler can temporarily add their names
	// back to the map. Persist GUIDs only for widgets reachable from the new root.
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	Tree->ForEachWidget([Blueprint](UWidget* Widget)
	{
		if (Widget)
		{
			Blueprint->WidgetVariableNameToGuidMap.Emplace(
				Widget->GetFName(),
				FGuid::NewDeterministicGuid(Widget->GetPathName()));
		}
	});
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

