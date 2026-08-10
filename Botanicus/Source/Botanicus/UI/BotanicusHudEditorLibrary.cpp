// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusHudEditorLibrary.h"

#if WITH_EDITOR
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NamedSlot.h"
#include "Components/TextBlock.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"
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
