// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/BotanicusCommandWidgetBuilder.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "UObject/SavePackage.h"
#include "UI/BotanicusOrderCatalogWidget.h"
#include "UI/BotanicusBotanistNotebookWidget.h"
#include "UI/BotanicusNotebookBoldDecorator.h"
#include "WidgetBlueprint.h"

namespace
{
UTexture2D* CommandTexture(const TCHAR* Name)
{
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(
		TEXT("/Game/Botanicus/UI/Command/Textures/%s.%s"), Name, Name));
}

UTexture2D* NotebookTexture(const TCHAR* Name)
{
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(
		TEXT("/Game/Botanicus/UI/Botanist/Textures/%s.%s"), Name, Name));
}

UObject* NotebookFont()
{
	return LoadObject<UObject>(nullptr,
		TEXT("/Game/Botanicus/UI/Botanist/Fonts/F_Notebook.F_Notebook"));
}

UFont* EnsureNotebookFontAsset()
{
	const TCHAR* FontPath =
		TEXT("/Game/Botanicus/UI/Botanist/Fonts/F_Notebook.F_Notebook");
	const TCHAR* PackagePath =
		TEXT("/Game/Botanicus/UI/Botanist/Fonts/F_Notebook");
	UFontFace* Face = LoadObject<UFontFace>(nullptr,
		TEXT("/Game/Botanicus/UI/Botanist/Fonts/Notebook.Notebook"));
	if (!Face)
	{
		return nullptr;
	}

	UFont* Font = LoadObject<UFont>(nullptr, FontPath);
	const bool bCreated = Font == nullptr;
	if (!Font)
	{
		UPackage* Package = CreatePackage(PackagePath);
		Font = NewObject<UFont>(Package, TEXT("F_Notebook"),
			RF_Public | RF_Standalone);
	}
	if (!Font)
	{
		return nullptr;
	}

	Font->Modify();
	Font->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	Composite.DefaultTypeface.Fonts.Reset();
	FTypefaceEntry Typeface;
	Typeface.Name = TEXT("Regular");
	Typeface.Font = FFontData(Face);
	Composite.DefaultTypeface.Fonts.Add(MoveTemp(Typeface));
	Font->PostEditChange();
	Font->MarkPackageDirty();
	if (bCreated)
	{
		FAssetRegistryModule::AssetCreated(Font);
	}
	const FString Filename = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(Font->GetOutermost(), Font, *Filename,
		FSavePackageArgs()) ? Font : nullptr;
}

FSlateFontInfo NotebookFontInfo(int32 Size)
{
	FSlateFontInfo Font;
	Font.FontObject = NotebookFont();
	Font.Size = Size;
	return Font;
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

UButton* AddVisualIconButton(
	UWidgetTree* Tree,
	UCanvasPanel* Canvas,
	const FName Name,
	const TCHAR* BackgroundTexture,
	const TCHAR* IconTexture,
	const FText& Tooltip,
	const FVector2D Position,
	const FVector2D Size)
{
	UButton* Button = AddCanvasWidget<UButton>(
		Tree, Canvas, Name, Position, Size, 2);
	FButtonStyle Style = Button->GetStyle();
	FSlateBrush BackgroundBrush;
	BackgroundBrush.SetResourceObject(CommandTexture(BackgroundTexture));
	BackgroundBrush.ImageSize = Size;
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	Style.SetNormal(BackgroundBrush);
	Style.SetHovered(BackgroundBrush);
	Style.SetPressed(BackgroundBrush);
	Button->SetStyle(Style);
	Button->SetToolTipText(Tooltip);

	UImage* Icon = Tree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		FName(*FString::Printf(TEXT("%sIcon"), *Name.ToString())));
	if (UTexture2D* IconAsset = CommandTexture(IconTexture))
	{
		Icon->SetBrushFromTexture(IconAsset, true);
	}
	Icon->SetDesiredSizeOverride(FVector2D(48.0f, 48.0f));
	if (UButtonSlot* IconSlot = Cast<UButtonSlot>(Button->AddChild(Icon)))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Button;
}

UImage* AddNotebookImage(UWidgetTree* Tree, UCanvasPanel* Canvas,
	const FName Name, const TCHAR* Texture, const FVector2D Position,
	const FVector2D Size, int32 ZOrder = 1)
{
	UImage* Image = AddCanvasWidget<UImage>(
		Tree, Canvas, Name, Position, Size, ZOrder);
	if (UTexture2D* Asset = NotebookTexture(Texture))
	{
		Image->SetBrushFromTexture(Asset, true);
	}
	return Image;
}

UTextBlock* AddNotebookText(UWidgetTree* Tree, UCanvasPanel* Canvas,
	const FName Name, const TCHAR* Text, const FVector2D Position,
	const FVector2D Size, int32 FontSize, int32 ZOrder = 4)
{
	UTextBlock* Label = AddCanvasWidget<UTextBlock>(
		Tree, Canvas, Name, Position, Size, ZOrder);
	Label->SetText(FText::FromString(Text));
	Label->SetFont(NotebookFontInfo(FontSize));
	Label->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.23f, 0.16f, 0.075f, 1.0f)));
	return Label;
}

UButton* AddNotebookButton(UWidgetTree* Tree, UCanvasPanel* Canvas,
	const FName Name, const TCHAR* NormalTexture, const TCHAR* PressedTexture,
	const FVector2D Position, const FVector2D Size, int32 ZOrder = 7)
{
	UButton* Button = AddCanvasWidget<UButton>(
		Tree, Canvas, Name, Position, Size, ZOrder);
	auto MakeBrush = [Size](UTexture2D* Texture)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	};
	FButtonStyle Style = Button->GetStyle();
	const FSlateBrush Normal = MakeBrush(NotebookTexture(NormalTexture));
	const FSlateBrush Pressed = MakeBrush(NotebookTexture(PressedTexture));
	Style.SetNormal(Normal);
	Style.SetHovered(Normal);
	Style.SetPressed(Pressed);
	Style.SetDisabled(Normal);
	Button->SetStyle(Style);
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

	const TCHAR* TabNames[] = {TEXT("SeedsTab"), TEXT("ToolsTab"), TEXT("CareTab"), TEXT("PreparationTab"), TEXT("SalesTab"), TEXT("BuildingsTab")};
	const TCHAR* TabIcons[] = {TEXT("T_Command_TabSeeds"), TEXT("T_Command_TabTools"), TEXT("T_Command_TabCare"), TEXT("T_Command_Preparation"), TEXT("T_Command_TabSales"), TEXT("T_Command_TabBuildings")};
	const TCHAR* TabLabels[] = {TEXT("Graines"), TEXT("Outils"), TEXT("Soin et entretien"), TEXT("Préparation"), TEXT("Vente"), TEXT("Bâtiments")};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		AddVisualIconButton(
			Tree, Root, TabNames[Index], TEXT("T_Command_TabBackground"),
			TabIcons[Index], FText::FromString(TabLabels[Index]),
			FVector2D(104 + Index * 280, 154), FVector2D(260, 58));
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

bool UBotanicusCommandWidgetBuilder::UpgradeCommandComputerCareTab()
{
#if WITH_EDITOR
	const TCHAR* ObjectPath =
		TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer.WBP_CommandComputer");
	const TCHAR* PackagePath =
		TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, ObjectPath);
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}
	UCanvasPanel* Root = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
	if (!Root)
	{
		return false;
	}

	Blueprint->Modify();
	Blueprint->WidgetTree->Modify();
	Root->Modify();
	UButton* CareTab = Cast<UButton>(
		Blueprint->WidgetTree->FindWidget(TEXT("CareTab")));
	if (!CareTab)
	{
		CareTab = AddVisualButton(
			Blueprint->WidgetTree, Root, TEXT("CareTab"),
			TEXT("T_Command_TabBackground"), TEXT("SOIN ET ENTRETIEN"),
			FVector2D::ZeroVector, FVector2D(260.0f, 58.0f));
	}

	const TCHAR* TabNames[] = {
		TEXT("SeedsTab"), TEXT("ToolsTab"), TEXT("CareTab"),
		TEXT("PreparationTab"), TEXT("SalesTab"), TEXT("BuildingsTab")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TabNames); ++Index)
	{
		if (UWidget* Tab = Blueprint->WidgetTree->FindWidget(TabNames[Index]))
		{
			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Tab->Slot))
			{
				Slot->SetPosition(FVector2D(104.0f + Index * 280.0f, 154.0f));
				Slot->SetSize(FVector2D(260.0f, 58.0f));
			}
		}
	}

	// The editor compiler requires a GUID for every newly constructed widget.
	// Resetting the stale map lets it rebuild the tree safely, then persist only
	// the widgets that are still reachable from the current designer root.
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget)
	{
		if (Widget)
		{
			Blueprint->WidgetVariableNameToGuidMap.Emplace(
				Widget->GetFName(),
				FGuid::NewDeterministicGuid(Widget->GetPathName()));
		}
	});
	Blueprint->MarkPackageDirty();
	const bool bSaved = UPackage::SavePackage(
		Blueprint->GetOutermost(), Blueprint,
		*FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension()),
		FSavePackageArgs());
	UE_LOG(LogTemp, Display,
		TEXT("BOTANICUS_COMMAND_CARE_TAB_UPGRADE saved=%d"), bSaved);
	return bSaved;
#else
	return false;
#endif
}

bool UBotanicusCommandWidgetBuilder::UpgradeCommandComputerTabIcons()
{
#if WITH_EDITOR
	const TCHAR* ObjectPath =
		TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer.WBP_CommandComputer");
	const TCHAR* PackagePath =
		TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, ObjectPath);
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();
	const TCHAR* TabNames[] = {
		TEXT("SeedsTab"), TEXT("ToolsTab"), TEXT("CareTab"),
		TEXT("PreparationTab"), TEXT("SalesTab"), TEXT("BuildingsTab")};
	const TCHAR* IconNames[] = {
		TEXT("T_Command_TabSeeds"), TEXT("T_Command_TabTools"),
		TEXT("T_Command_TabCare"), TEXT("T_Command_Preparation"),
		TEXT("T_Command_TabSales"), TEXT("T_Command_TabBuildings")};
	const TCHAR* Tooltips[] = {
		TEXT("Graines"), TEXT("Outils"), TEXT("Soin et entretien"),
		TEXT("Préparation"), TEXT("Vente"), TEXT("Bâtiments")};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TabNames); ++Index)
	{
		UButton* Button = Cast<UButton>(Tree->FindWidget(TabNames[Index]));
		UTexture2D* Texture = CommandTexture(IconNames[Index]);
		if (!Button || !Texture)
		{
			return false;
		}
		Button->Modify();
		Button->ClearChildren();
		const FName IconWidgetName(*FString::Printf(
			TEXT("%sIcon"), TabNames[Index]));
		UImage* Icon = Tree->ConstructWidget<UImage>(
			UImage::StaticClass(), IconWidgetName);
		Icon->SetBrushFromTexture(Texture, true);
		Icon->SetDesiredSizeOverride(FVector2D(48.0f, 48.0f));
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Icon)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		Button->SetToolTipText(FText::FromString(Tooltips[Index]));
	}

	Blueprint->WidgetVariableNameToGuidMap.Reset();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
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
	const bool bSaved = UPackage::SavePackage(
		Blueprint->GetOutermost(), Blueprint,
		*FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension()),
		FSavePackageArgs());
	UE_LOG(LogTemp, Display,
		TEXT("BOTANICUS_COMMAND_TAB_ICONS_UPGRADE saved=%d"), bSaved);
	return bSaved;
#else
	return false;
#endif
}

bool UBotanicusCommandWidgetBuilder::RebuildBotanistNotebookWidget()
{
#if WITH_EDITOR
	if (!EnsureNotebookFontAsset())
	{
		return false;
	}
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
	Backdrop->SetBrushColor(FLinearColor(0.006f, 0.012f, 0.008f, 0.88f));
	AddNotebookImage(Tree, Root, TEXT("BookBackground"),
		TEXT("T_Notebook_Background"), FVector2D(150.0f, 0.0f),
		FVector2D(1620.0f, 1080.0f), 1);
	AddNotebookButton(Tree, Root, TEXT("CloseButton"),
		TEXT("T_Notebook_CloseNormal"), TEXT("T_Notebook_ClosePressed"),
		FVector2D(1770.0f, 24.0f), FVector2D(105.0f, 105.0f));

	auto AddPage = [Tree, Root](const TCHAR* Prefix, float X)
	{
		auto Named = [Prefix](const TCHAR* Suffix)
		{
			return FName(*FString::Printf(TEXT("%s%s"), Prefix, Suffix));
		};
		UTextBlock* Name = AddNotebookText(Tree, Root,
			Named(TEXT("PlantNameLabel")), TEXT("????????????"),
			FVector2D(X + 55.0f, 91.0f), FVector2D(520.0f, 58.0f), 31);
		Name->SetJustification(ETextJustify::Center);
		AddNotebookImage(Tree, Root, Named(TEXT("TitleLeafLeft")),
			TEXT("T_Notebook_TitleLeft"), FVector2D(X + 8.0f, 73.0f),
			FVector2D(125.0f, 105.0f), 3);
		AddNotebookImage(Tree, Root, Named(TEXT("TitleLeafRight")),
			TEXT("T_Notebook_TitleRight"), FVector2D(X + 500.0f, 73.0f),
			FVector2D(125.0f, 105.0f), 3);

		AddNotebookImage(Tree, Root, Named(TEXT("PlantImage")),
			TEXT("T_Notebook_UnknownPlant"), FVector2D(X + 105.0f, 166.0f),
			FVector2D(420.0f, 360.0f), 4);
		UTextBlock* Illustration = AddNotebookText(
			Tree, Root, Named(TEXT("PlantIllustrationLabel")), TEXT("?"),
			FVector2D(X + 125.0f, 260.0f), FVector2D(380.0f, 140.0f), 35, 5);
		Illustration->SetJustification(ETextJustify::Center);

		const TCHAR* Suffixes[] = {
			TEXT("Element"), TEXT("Temperature"), TEXT("AirHumidity"),
			TEXT("Luminosity"), TEXT("Water"), TEXT("Growth")};
		const TCHAR* Defaults[] = {
			TEXT("Type : ???"), TEXT("Température : ???"), TEXT("Humidité : ???"),
			TEXT("Luminosité : ???"), TEXT("Terreau : ???"), TEXT("Croissance : ???")};
		const TCHAR* IconTextures[] = {
			TEXT("T_PlantElement_Normal"), TEXT("T_Notebook_Temperature"),
			TEXT("T_Notebook_Humidity"), TEXT("T_Notebook_Luminosity"),
			TEXT("T_Notebook_Soil"), TEXT("T_Notebook_GrowthTime")};
		for (int32 Index = 0; Index < 6; ++Index)
		{
			UImage* Icon = AddCanvasWidget<UImage>(Tree, Root,
				Named(*FString::Printf(TEXT("%sIcon"), Suffixes[Index])),
				FVector2D(X + 62.0f, 554.0f + Index * 45.0f),
				FVector2D(34.0f, 34.0f), 4);
			UTexture2D* IconTexture = Index == 0
				? LoadObject<UTexture2D>(nullptr,
					TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Normal.T_PlantElement_Normal"))
				: NotebookTexture(IconTextures[Index]);
			Icon->SetBrushFromTexture(IconTexture, true);
			AddNotebookText(Tree, Root,
				Named(*FString::Printf(TEXT("%sLabel"), Suffixes[Index])),
				Defaults[Index], FVector2D(X + 108.0f, 552.0f + Index * 45.0f),
				FVector2D(470.0f, 38.0f), 19);
		}

		UBorder* NoteFrame = AddCanvasWidget<UBorder>(
			Tree, Root, Named(TEXT("NoteFrame")), FVector2D(X + 48.0f, 842.0f),
			FVector2D(535.0f, 93.0f), 3);
		NoteFrame->SetBrushColor(FLinearColor(0.63f, 0.54f, 0.34f, 0.18f));
		UScrollBox* NoteScroll = AddCanvasWidget<UScrollBox>(
			Tree, Root, Named(TEXT("NoteScrollBox")),
			FVector2D(X + 62.0f, 850.0f), FVector2D(507.0f, 77.0f), 4);
		NoteScroll->SetOrientation(Orient_Vertical);
		NoteScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		NoteScroll->SetAnimateWheelScrolling(true);
		NoteScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		NoteScroll->SetScrollbarThickness(FVector2D(6.0f, 6.0f));
		URichTextBlock* Note = Tree->ConstructWidget<URichTextBlock>(
			URichTextBlock::StaticClass(), Named(TEXT("NoteLabel")));
		Note->SetText(FText::FromString(TEXT("Note : ???")));
		Note->SetDefaultFont(NotebookFontInfo(16));
		Note->SetDefaultColorAndOpacity(FSlateColor(
			FLinearColor(0.24f, 0.16f, 0.08f, 1.0f)));
		Note->SetDecorators({UBotanicusNotebookBoldDecorator::StaticClass()});
		Note->SetAutoWrapText(true);
		if (UScrollBoxSlot* NoteSlot = Cast<UScrollBoxSlot>(
			NoteScroll->AddChild(Note)))
		{
			NoteSlot->SetHorizontalAlignment(HAlign_Fill);
			NoteSlot->SetPadding(FMargin(8.0f, 4.0f, 8.0f, 8.0f));
		}
	};
	AddPage(TEXT("Left"), 310.0f);
	AddPage(TEXT("Right"), 980.0f);

	AddNotebookButton(Tree, Root, TEXT("PreviousPageButton"),
		TEXT("T_Notebook_PreviousNormal"), TEXT("T_Notebook_PreviousPressed"),
		FVector2D(250.0f, 905.0f), FVector2D(105.0f, 105.0f));
	AddNotebookButton(Tree, Root, TEXT("NextPageButton"),
		TEXT("T_Notebook_NextNormal"), TEXT("T_Notebook_NextPressed"),
		FVector2D(1570.0f, 905.0f), FVector2D(105.0f, 105.0f));

	for (int32 Index = 0; Index < 26; ++Index)
	{
		const TCHAR Letter = static_cast<TCHAR>('A' + Index);
		const FName ButtonName(*FString::Printf(TEXT("Letter%cButton"), Letter));
		UButton* Button = AddCanvasWidget<UButton>(
			Tree, Root, ButtonName, FVector2D(1642.0f, 79.0f + Index * 35.0f),
			FVector2D(72.0f, 34.0f), 6);
		FSlateBrush TabBrush;
		const FString TabTextureName = FString::Printf(
			TEXT("T_Notebook_Tab_%c"), Letter);
		TabBrush.SetResourceObject(NotebookTexture(*TabTextureName));
		TabBrush.ImageSize = FVector2D(72.0f, 34.0f);
		TabBrush.DrawAs = ESlateBrushDrawType::Image;
		FButtonStyle TabStyle = Button->GetStyle();
		TabStyle.SetNormal(TabBrush);
		TabStyle.SetHovered(TabBrush);
		TabStyle.SetPressed(TabBrush);
		TabStyle.SetDisabled(TabBrush);
		Button->SetStyle(TabStyle);
	}

	const TCHAR* ElementNames[] = {
		TEXT("All"), TEXT("Normal"), TEXT("Fire"), TEXT("Water"),
		TEXT("Shadow"), TEXT("Ice")};
	const TCHAR* ElementLabels[] = {
		TEXT("Tous"), TEXT("Normal"), TEXT("Feu"), TEXT("Eau"),
		TEXT("Ténèbres"), TEXT("Glace")};
	const TCHAR* ElementTextureNames[] = {
		TEXT("T_Notebook_ElementTab_Normal"),
		TEXT("T_Notebook_ElementTab_Normal"),
		TEXT("T_Notebook_ElementTab_Fire"),
		TEXT("T_Notebook_ElementTab_Water"),
		TEXT("T_Notebook_ElementTab_Shadow"),
		TEXT("T_Notebook_ElementTab_Ice")};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FName ButtonName(*FString::Printf(
			TEXT("Element%sButton"), ElementNames[Index]));
		UButton* Button = AddCanvasWidget<UButton>(
			Tree, Root, ButtonName,
			FVector2D(1040.0f + Index * 88.0f, 982.0f),
			FVector2D(64.0f, 96.0f), 7);
		FSlateBrush TabBrush;
		TabBrush.SetResourceObject(NotebookTexture(ElementTextureNames[Index]));
		TabBrush.ImageSize = FVector2D(64.0f, 96.0f);
		TabBrush.DrawAs = ESlateBrushDrawType::Image;
		FButtonStyle TabStyle = Button->GetStyle();
		TabStyle.SetNormal(TabBrush);
		TabStyle.SetHovered(TabBrush);
		TabStyle.SetPressed(TabBrush);
		Button->SetStyle(TabStyle);
		Button->SetToolTipText(FText::FromString(ElementLabels[Index]));

		if (Index == 0)
		{
			// No dedicated "Tous" artwork was supplied. Reuse the matching
			// parchment tab silhouette and cover its central emblem with a label.
			UBorder* ContentBackground = Tree->ConstructWidget<UBorder>();
			ContentBackground->SetBrushColor(
				FLinearColor(0.92f, 0.79f, 0.54f, 0.98f));
			ContentBackground->SetPadding(FMargin(5.0f, 3.0f));
			UTextBlock* AllLabel = Tree->ConstructWidget<UTextBlock>();
			AllLabel->SetText(FText::FromString(TEXT("Tous")));
			AllLabel->SetFont(NotebookFontInfo(14));
			AllLabel->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.18f, 0.11f, 0.04f, 1.0f)));
			ContentBackground->AddChild(AllLabel);
			if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(
				Button->AddChild(ContentBackground)))
			{
				ContentSlot->SetHorizontalAlignment(HAlign_Center);
				ContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
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

bool UBotanicusCommandWidgetBuilder::
	UpgradeBotanistNotebookNoteScrollBoxes()
{
#if WITH_EDITOR
	const TCHAR* ObjectPath =
		TEXT("/Game/Botanicus/UI/Botanist/WBP_BotanistNotebook.WBP_BotanistNotebook");
	const TCHAR* PackagePath =
		TEXT("/Game/Botanicus/UI/Botanist/WBP_BotanistNotebook");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, ObjectPath);
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();
	UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
	if (!Root)
	{
		return false;
	}

	bool bChanged = false;
	const TCHAR* Prefixes[] = {TEXT("Left"), TEXT("Right")};
	for (const TCHAR* Prefix : Prefixes)
	{
		const FName NoteName(*FString::Printf(TEXT("%sNoteLabel"), Prefix));
		const FName FrameName(*FString::Printf(TEXT("%sNoteFrame"), Prefix));
		const FName ScrollName(*FString::Printf(TEXT("%sNoteScrollBox"), Prefix));
		UTextBlock* Note = Cast<UTextBlock>(Tree->FindWidget(NoteName));
		UBorder* Frame = Cast<UBorder>(Tree->FindWidget(FrameName));
		UScrollBox* NoteScroll = Cast<UScrollBox>(Tree->FindWidget(ScrollName));
		UCanvasPanelSlot* FrameSlot = Frame
			? Cast<UCanvasPanelSlot>(Frame->Slot)
			: nullptr;
		if (!Note || !FrameSlot)
		{
			return false;
		}

		const FVector2D Padding(12.0f, 10.0f);
		const FVector2D ScrollPosition = FrameSlot->GetPosition() + Padding;
		const FVector2D ScrollSize(
			FMath::Max(80.0f, FrameSlot->GetSize().X - Padding.X * 2.0f),
			FMath::Max(48.0f, FrameSlot->GetSize().Y - Padding.Y * 2.0f));
		if (!NoteScroll)
		{
			NoteScroll = AddCanvasWidget<UScrollBox>(
				Tree, Root, ScrollName, ScrollPosition, ScrollSize,
				FrameSlot->GetZOrder() + 1);
			bChanged = true;
		}
		else if (UCanvasPanelSlot* ScrollCanvasSlot =
			Cast<UCanvasPanelSlot>(NoteScroll->Slot))
		{
			ScrollCanvasSlot->SetPosition(ScrollPosition);
			ScrollCanvasSlot->SetSize(ScrollSize);
		}

		NoteScroll->SetOrientation(Orient_Vertical);
		NoteScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		NoteScroll->SetAnimateWheelScrolling(true);
		NoteScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		NoteScroll->SetScrollbarThickness(FVector2D(6.0f, 6.0f));
		if (Note->GetParent() != NoteScroll)
		{
			if (UPanelWidget* Parent = Note->GetParent())
			{
				Parent->RemoveChild(Note);
			}
			if (UScrollBoxSlot* NoteSlot = Cast<UScrollBoxSlot>(
				NoteScroll->AddChild(Note)))
			{
				NoteSlot->SetHorizontalAlignment(HAlign_Fill);
				NoteSlot->SetPadding(FMargin(4.0f, 2.0f, 8.0f, 8.0f));
			}
			bChanged = true;
		}
		Note->SetAutoWrapText(true);
		Note->SetWrapTextAt(FMath::Max(60.0f, ScrollSize.X - 22.0f));
	}

	Blueprint->WidgetVariableNameToGuidMap.Reset();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
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
	return UPackage::SavePackage(
		Blueprint->GetOutermost(), Blueprint,
		*FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension()),
		FSavePackageArgs()) && bChanged;
#else
	return false;
#endif
}

bool UBotanicusCommandWidgetBuilder::UpgradeBotanistNotebookRichNotes()
{
#if WITH_EDITOR
	const TCHAR* ObjectPath =
		TEXT("/Game/Botanicus/UI/Botanist/WBP_BotanistNotebook.WBP_BotanistNotebook");
	const TCHAR* PackagePath =
		TEXT("/Game/Botanicus/UI/Botanist/WBP_BotanistNotebook");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, ObjectPath);
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return false;
	}

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();
	bool bChanged = false;
	const TCHAR* Prefixes[] = {TEXT("Left"), TEXT("Right")};
	for (const TCHAR* Prefix : Prefixes)
	{
		const FName NoteName(*FString::Printf(TEXT("%sNoteLabel"), Prefix));
		const FName ScrollName(*FString::Printf(TEXT("%sNoteScrollBox"), Prefix));
		UScrollBox* NoteScroll = Cast<UScrollBox>(Tree->FindWidget(ScrollName));
		UWidget* ExistingNote = Tree->FindWidget(NoteName);
		if (!NoteScroll || !ExistingNote)
		{
			return false;
		}

		URichTextBlock* RichNote = Cast<URichTextBlock>(ExistingNote);
		if (!RichNote)
		{
			UTextBlock* LegacyNote = Cast<UTextBlock>(ExistingNote);
			if (!LegacyNote)
			{
				return false;
			}
			const FText PreviousText = LegacyNote->GetText();
			if (UPanelWidget* Parent = LegacyNote->GetParent())
			{
				Parent->RemoveChild(LegacyNote);
			}
			const FName LegacyName = MakeUniqueObjectName(
				Tree, UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("%sLegacyNoteLabel"), Prefix)));
			LegacyNote->Rename(*LegacyName.ToString(), Tree,
				REN_DontCreateRedirectors | REN_NonTransactional);

			RichNote = Tree->ConstructWidget<URichTextBlock>(
				URichTextBlock::StaticClass(), NoteName);
			RichNote->SetText(PreviousText);
			if (UScrollBoxSlot* NoteSlot = Cast<UScrollBoxSlot>(
				NoteScroll->AddChild(RichNote)))
			{
				NoteSlot->SetHorizontalAlignment(HAlign_Fill);
				NoteSlot->SetPadding(FMargin(4.0f, 2.0f, 8.0f, 8.0f));
			}
			bChanged = true;
		}

		RichNote->SetDefaultFont(NotebookFontInfo(16));
		RichNote->SetDefaultColorAndOpacity(FSlateColor(
			FLinearColor(0.24f, 0.16f, 0.08f, 1.0f)));
		RichNote->SetDecorators(
			{UBotanicusNotebookBoldDecorator::StaticClass()});
		RichNote->SetAutoWrapText(true);
	}

	Blueprint->WidgetVariableNameToGuidMap.Reset();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
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
	return UPackage::SavePackage(
		Blueprint->GetOutermost(), Blueprint,
		*FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension()),
		FSavePackageArgs()) && bChanged;
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

