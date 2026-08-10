// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusHudEditorLibrary.h"

#if WITH_EDITOR
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/TextBlock.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Styling/CoreStyle.h"
#include "UI/BotanicusHudStyle.h"
#include "WidgetBlueprint.h"
#endif

#if WITH_EDITOR
namespace
{
UWidgetTree* ResetSourceTree(UWidgetBlueprint* Blueprint)
{
	UWidgetTree* Tree = Blueprint->WidgetTree;
	if (!Tree)
	{
		Tree = NewObject<UWidgetTree>(
			Blueprint, TEXT("WidgetTree"), RF_Transactional);
		Blueprint->WidgetTree = Tree;
	}
	Tree->Modify();
	Tree->RootWidget = nullptr;
	return Tree;
}

UCanvasPanelSlot* AddToCanvas(
	UCanvasPanel* Parent,
	UWidget* Child,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 ZOrder = 0)
{
	UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(ZOrder);
	return Slot;
}

UTextBlock* AddText(
	UWidgetTree* Tree,
	UCanvasPanel* Parent,
	const TCHAR* Name,
	const TCHAR* PreviewText,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 FontSize,
	bool bBold = false,
	ETextJustify::Type Justification = ETextJustify::Left)
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(Name));
	Text->SetText(FText::FromString(PreviewText));
	const UObject* PersistentFont = LoadObject<UObject>(
		nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	Text->SetFont(FSlateFontInfo(
		PersistentFont, FontSize,
		bBold ? FName(TEXT("Bold")) : NAME_None));
	Text->SetColorAndOpacity(FLinearColor(0.96f, 0.93f, 0.82f, 1.0f));
	Text->SetJustification(Justification);
	AddToCanvas(Parent, Text, Position, Size, 2);
	return Text;
}

UImage* AddImage(
	UWidgetTree* Tree,
	UCanvasPanel* Parent,
	const TCHAR* Name,
	const TCHAR* TextureName,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 ZOrder = 0)
{
	UImage* Image = Tree->ConstructWidget<UImage>(
		UImage::StaticClass(), FName(Name));
	Image->SetBrushFromTexture(
		BotanicusHudStyle::LoadTexture(TextureName), true);
	AddToCanvas(Parent, Image, Position, Size, ZOrder);
	return Image;
}
}
#endif

bool UBotanicusHudEditorLibrary::BuildEditableHudLayout(
	UObject* WidgetBlueprintAsset)
{
#if WITH_EDITOR
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
	if (!Blueprint)
	{
		return false;
	}

	UWidgetTree* Tree = Blueprint->WidgetTree;
	if (!Tree)
	{
		Tree = NewObject<UWidgetTree>(
			Blueprint, TEXT("WidgetTree"), RF_Transactional);
		Blueprint->WidgetTree = Tree;
	}
	Tree->Modify();

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("HUD_Canvas_Modifiable"));
	Tree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	auto AddHudSlot = [Tree, Root](
		const TCHAR* Name,
		const FAnchors& Anchors,
		const FVector2D& Alignment,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder)
	{
		UNamedSlot* NamedSlot = Tree->ConstructWidget<UNamedSlot>(
			UNamedSlot::StaticClass(), FName(Name));
		UTextBlock* DesignerGuide = Tree->ConstructWidget<UTextBlock>();
		DesignerGuide->SetText(FText::FromString(FString::Printf(
			TEXT("DEPLACER : %s"), Name)));
		DesignerGuide->SetJustification(ETextJustify::Center);
		DesignerGuide->SetColorAndOpacity(FLinearColor(
			0.58f, 0.86f, 0.46f, 0.7f));
		NamedSlot->SetContent(DesignerGuide);
		UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(NamedSlot);
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetZOrder(ZOrder);
	};

	AddHudSlot(TEXT("ClockSlot"), FAnchors(0.0f, 0.0f),
		FVector2D::ZeroVector, FVector2D(18.0f, 16.0f),
		FVector2D(420.0f, 175.0f), 10);
	AddHudSlot(TEXT("CreditsSlot"), FAnchors(1.0f, 0.0f),
		FVector2D(1.0f, 0.0f), FVector2D(-274.0f, 20.0f),
		FVector2D(232.0f, 72.0f), 10);
	AddHudSlot(TEXT("ReputationSlot"), FAnchors(1.0f, 0.0f),
		FVector2D(1.0f, 0.0f), FVector2D(-24.0f, 28.0f),
		FVector2D(232.0f, 54.0f), 10);
	AddHudSlot(TEXT("ObjectivesSlot"), FAnchors(1.0f, 0.0f),
		FVector2D(1.0f, 0.0f), FVector2D(-24.0f, 126.0f),
		FVector2D(370.0f, 370.0f), 9);
	AddHudSlot(TEXT("QuickBarSlot"), FAnchors(0.5f, 1.0f),
		FVector2D(0.5f, 1.0f), FVector2D(0.0f, -24.0f),
		FVector2D(920.0f, 210.0f), 20);
	AddHudSlot(TEXT("InteractionSlot"), FAnchors(0.5f, 1.0f),
		FVector2D(0.5f, 1.0f), FVector2D(0.0f, -112.0f),
		FVector2D(310.0f, 108.0f), 30);
	AddHudSlot(TEXT("MessageSlot"), FAnchors(0.5f, 1.0f),
		FVector2D(0.5f, 1.0f), FVector2D(0.0f, -226.0f),
		FVector2D(460.0f, 108.0f), 31);

	Blueprint->Modify();
	Blueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	UWidgetTree* CompiledTree =
		GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	const TCHAR* RequiredNames[] = {
		TEXT("ClockSlot"), TEXT("CreditsSlot"),
		TEXT("ReputationSlot"), TEXT("ObjectivesSlot"),
		TEXT("QuickBarSlot"), TEXT("InteractionSlot"),
		TEXT("MessageSlot")};
	if (!CompiledTree)
	{
		UE_LOG(LogTemp, Error, TEXT("HUD Blueprint has no compiled widget tree."));
		return false;
	}
	for (const TCHAR* RequiredName : RequiredNames)
	{
		if (!CompiledTree->FindWidget(FName(RequiredName)))
		{
			UE_LOG(LogTemp, Error,
				TEXT("HUD Blueprint lost required widget %s during compilation."),
				RequiredName);
			return false;
		}
	}
	return Blueprint->Status != BS_Error;
#else
	return false;
#endif
}

bool UBotanicusHudEditorLibrary::BuildEditableHudElement(
	UObject* WidgetBlueprintAsset,
	FName ElementType)
{
#if WITH_EDITOR
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
	if (!Blueprint)
	{
		return false;
	}
	UWidgetTree* Tree = ResetSourceTree(Blueprint);
	const FString Type = ElementType.ToString();

	// The quickbar contents remain dynamic, but all its font and padding
	// properties are editable in this Blueprint's Class Defaults.
	if (Type != TEXT("QuickBar"))
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("ElementCanvas"));
		Tree->RootWidget = Root;

		if (Type == TEXT("Clock"))
		{
			AddImage(Tree, Root, TEXT("CalendarBackground"),
				TEXT("T_HUD_Calendar"), FVector2D::ZeroVector,
				FVector2D(420.0f, 143.0f));
			AddText(Tree, Root, TEXT("ClockLabel"), TEXT("JOUR 25"),
				FVector2D(98.0f, 21.0f), FVector2D(128.0f, 42.0f),
				20, true, ETextJustify::Center);
			AddText(Tree, Root, TEXT("TimeLabel"), TEXT("20:45"),
				FVector2D(269.0f, 20.0f), FVector2D(116.0f, 43.0f),
				21, true, ETextJustify::Center);
			AddText(Tree, Root, TEXT("ScheduleLabel"),
				TEXT("BOUTIQUE OUVERTE  08:00 - 19:00"),
				FVector2D(95.0f, 90.0f), FVector2D(290.0f, 30.0f),
				11, true, ETextJustify::Center);
			UTextBlock* Furniture = AddText(Tree, Root,
				TEXT("FurnitureModeLabel"), TEXT("MODE MEUBLES [B] : ACTIF"),
				FVector2D(116.0f, 151.0f), FVector2D(260.0f, 24.0f),
				12, true);
			Furniture->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (Type == TEXT("Credits"))
		{
			AddImage(Tree, Root, TEXT("CreditsBackground"),
				TEXT("T_HUD_CreditsBackground"), FVector2D::ZeroVector,
				FVector2D(232.0f, 72.0f));
			AddImage(Tree, Root, TEXT("CreditsIcon"), TEXT("T_HUD_Credit"),
				FVector2D(9.0f, 15.0f), FVector2D(42.0f, 42.0f), 1);
			AddText(Tree, Root, TEXT("FundsLabel"), TEXT("440"),
				FVector2D(48.0f, 19.0f), FVector2D(173.0f, 34.0f),
				19, true, ETextJustify::Center);
		}
		else if (Type == TEXT("Reputation"))
		{
			AddImage(Tree, Root, TEXT("ReputationBackground"),
				TEXT("T_HUD_ReputationBackground"), FVector2D::ZeroVector,
				FVector2D(232.0f, 54.0f));
			AddImage(Tree, Root, TEXT("ReputationIcon"), TEXT("T_HUD_Star"),
				FVector2D(9.0f, 8.0f), FVector2D(38.0f, 38.0f), 1);
			AddText(Tree, Root, TEXT("ReputationLabel"), TEXT("3.34/5"),
				FVector2D(48.0f, 10.0f), FVector2D(173.0f, 34.0f),
				19, true, ETextJustify::Center);
		}
		else if (Type == TEXT("Interaction"))
		{
			AddImage(Tree, Root, TEXT("ActionBackground"),
				TEXT("T_HUD_ActionBackground"), FVector2D::ZeroVector,
				FVector2D(310.0f, 108.0f));
			AddText(Tree, Root, TEXT("KeyLabel"), TEXT("E"),
				FVector2D(22.0f, 32.0f), FVector2D(48.0f, 43.0f),
				19, true, ETextJustify::Center);
			AddText(Tree, Root, TEXT("ActionLabel"), TEXT("INTERAGIR"),
				FVector2D(84.0f, 28.0f), FVector2D(190.0f, 28.0f),
				14, true);
			UTextBlock* Target = AddText(Tree, Root,
				TEXT("TargetNameText"), TEXT("Nom de l'objet"),
				FVector2D(84.0f, 54.0f), FVector2D(190.0f, 38.0f), 12);
			Target->SetAutoWrapText(true);
		}
		else if (Type == TEXT("Message"))
		{
			AddImage(Tree, Root, TEXT("MessageBackground"),
				TEXT("T_HUD_ActionBackground"), FVector2D::ZeroVector,
				FVector2D(460.0f, 108.0f));
			AddText(Tree, Root, TEXT("NoticeIcon"), TEXT("!"),
				FVector2D(31.0f, 31.0f), FVector2D(66.0f, 45.0f),
				20, true, ETextJustify::Center);
			UTextBlock* Message = AddText(Tree, Root,
				TEXT("MessageLabel"), TEXT("MESSAGE DU JEU"),
				FVector2D(112.0f, 23.0f), FVector2D(310.0f, 64.0f),
				15, true, ETextJustify::Center);
			Message->SetAutoWrapText(true);
		}
		else if (Type == TEXT("Objectives"))
		{
			UCanvasPanel* Panel = Tree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), TEXT("ObjectivesPanel"));
			Panel->SetClipping(EWidgetClipping::ClipToBounds);
			AddToCanvas(Root, Panel, FVector2D::ZeroVector,
				FVector2D(370.0f, 370.0f));
			AddImage(Tree, Panel, TEXT("ObjectivesBackground"),
				TEXT("T_HUD_ObjectivesBackground"), FVector2D::ZeroVector,
				FVector2D(370.0f, 370.0f));
			AddText(Tree, Panel, TEXT("TitleLabel"),
				TEXT("OBJECTIFS NIVEAU 2"), FVector2D(68.0f, 27.0f),
				FVector2D(226.0f, 43.0f), 16, true, ETextJustify::Center);
			UButton* Toggle = Tree->ConstructWidget<UButton>(
				UButton::StaticClass(), TEXT("ToggleButton"));
			Toggle->SetBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.01f));
			AddToCanvas(Panel, Toggle, FVector2D(302.0f, 24.0f),
				FVector2D(49.0f, 49.0f), 3);

			UCanvasPanel* Body = Tree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), TEXT("ObjectivesBody"));
			AddToCanvas(Panel, Body, FVector2D::ZeroVector,
				FVector2D(370.0f, 370.0f), 2);
			const TCHAR* Prefixes[] = {
				TEXT("PlantSales"), TEXT("CatalogOrders"),
				TEXT("ReputationGoal"), TEXT("FundsGoal"),
				TEXT("DailyRevenue")};
			const TCHAR* Previews[] = {
				TEXT("Vendre des plantes"), TEXT("Passer des commandes"),
				TEXT("Reputation"), TEXT("Reunir les credits"),
				TEXT("Chiffre d'affaires")};
			for (int32 Index = 0; Index < 5; ++Index)
			{
				const float Y = 91.0f + Index * 47.0f;
				AddImage(Tree, Body,
					*FString::Printf(TEXT("%sIcon"), Prefixes[Index]),
					TEXT("T_HUD_ObjectiveIncomplete"), FVector2D(33.0f, Y),
					FVector2D(30.0f, 30.0f));
				AddText(Tree, Body,
					*FString::Printf(TEXT("%sLabel"), Prefixes[Index]),
					Previews[Index], FVector2D(70.0f, Y - 2.0f),
					FVector2D(190.0f, 34.0f), 14);
				AddText(Tree, Body,
					*FString::Printf(TEXT("%sProgressLabel"), Prefixes[Index]),
					TEXT("0/0"), FVector2D(258.0f, Y - 2.0f),
					FVector2D(79.0f, 34.0f), 14, false,
					ETextJustify::Right);
			}
		}
		else
		{
			return false;
		}
	}

	Blueprint->Modify();
	Blueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Blueprint->Status != BS_Error;
#else
	return false;
#endif
}
