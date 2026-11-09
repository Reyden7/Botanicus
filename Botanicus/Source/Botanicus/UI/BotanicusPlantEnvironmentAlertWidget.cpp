// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusPlantEnvironmentAlertWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

namespace
{
	UImage* AddAlertImage(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const TCHAR* Name,
		float HorizontalPosition)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(
			UImage::StaticClass(), FName(Name));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
		Slot->SetPosition(FVector2D(HorizontalPosition, 0.0f));
		Slot->SetSize(FVector2D(72.0f, 72.0f));
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return Image;
	}
}

void UBotanicusPlantEnvironmentAlertWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(240.0f);
		Root->SetHeightOverride(72.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetContent(Canvas);
		TemperatureImage = AddAlertImage(
			WidgetTree, Canvas, TEXT("TemperatureAlert"), 4.0f);
		AirHumidityImage = AddAlertImage(
			WidgetTree, Canvas, TEXT("AirHumidityAlert"), 84.0f);
		LuminosityImage = AddAlertImage(
			WidgetTree, Canvas, TEXT("LuminosityAlert"), 164.0f);
		WidgetTree->RootWidget = Root;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusPlantEnvironmentAlertWidget::SetEnvironmentState(
	const FBotanicusPlantEnvironmentState& EnvironmentState)
{
	if (!EnvironmentState.bEnvironmentAvailable)
	{
		bHasAnyAlert = false;
		if (TemperatureImage)
		{
			TemperatureImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (AirHumidityImage)
		{
			AirHumidityImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (LuminosityImage)
		{
			LuminosityImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const bool bTemperatureAlert = ApplyConditionIcon(
		TemperatureImage,
		EnvironmentState.TemperatureCondition,
		TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_TemperatureLow.T_PlantEnvironment_TemperatureLow"),
		TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_TemperatureHigh.T_PlantEnvironment_TemperatureHigh"));
	const bool bHumidityAlert = ApplyConditionIcon(
		AirHumidityImage,
		EnvironmentState.AirHumidityCondition,
		TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_HumidityLow.T_PlantEnvironment_HumidityLow"),
		TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_HumidityHigh.T_PlantEnvironment_HumidityHigh"));
	const bool bLuminosityAlert = ApplyConditionIcon(
		LuminosityImage,
		EnvironmentState.LuminosityCondition,
		TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_LuminosityLow.T_PlantEnvironment_LuminosityLow"),
		TEXT("/Game/Botanicus/UI/Plant/Environment/T_PlantEnvironment_LuminosityHigh.T_PlantEnvironment_LuminosityHigh"));
	bHasAnyAlert = bTemperatureAlert || bHumidityAlert || bLuminosityAlert;
}

bool UBotanicusPlantEnvironmentAlertWidget::ApplyConditionIcon(
	UImage* Image,
	EBotanicusPlantEnvironmentCondition Condition,
	const TCHAR* LowTexturePath,
	const TCHAR* HighTexturePath)
{
	if (!Image ||
		(Condition != EBotanicusPlantEnvironmentCondition::TooLow &&
		 Condition != EBotanicusPlantEnvironmentCondition::TooHigh))
	{
		if (Image)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
		}
		return false;
	}

	// The arrow communicates the correction the player must make, rather than
	// merely describing the current value: increase a low value and decrease
	// a high value.
	const TCHAR* TexturePath =
		Condition == EBotanicusPlantEnvironmentCondition::TooLow
			? HighTexturePath
			: LowTexturePath;
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}
	Image->SetBrushFromTexture(Texture, true);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UBotanicusPlantEnvironmentAlertWidget::SetContextAlert(UTexture2D* Texture,const FLinearColor& Tint)
{
    if(auto* Root=Cast<USizeBox>(WidgetTree ? WidgetTree->RootWidget:nullptr)){Root->SetWidthOverride(240);Root->SetHeightOverride(72);}
    bHasAnyAlert=Texture!=nullptr;
    if(AirHumidityImage) AirHumidityImage->SetVisibility(ESlateVisibility::Collapsed);
    if(LuminosityImage) LuminosityImage->SetVisibility(ESlateVisibility::Collapsed);
    if(!TemperatureImage) return;
    if(auto* CanvasSlot=Cast<UCanvasPanelSlot>(TemperatureImage->Slot)) CanvasSlot->SetPosition(FVector2D(84,0));
    if(TemperatureImage->GetBrush().GetResourceObject()!=Texture) TemperatureImage->SetBrushFromTexture(Texture,true);
    TemperatureImage->SetColorAndOpacity(Tint);
    TemperatureImage->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UBotanicusPlantEnvironmentAlertWidget::SetColumnState(const FBotanicusPlantEnvironmentState& State)
{
    SetEnvironmentState(State);
    if(!WidgetTree)return;
    if(auto* Root=Cast<USizeBox>(WidgetTree->RootWidget)){Root->SetWidthOverride(72);Root->SetHeightOverride(240);}
    WidgetTree->ForEachWidget([this](UWidget* W)
    {
        if(auto* I=Cast<UImage>(W);I && I!=TemperatureImage && I!=AirHumidityImage && I!=LuminosityImage)I->SetVisibility(ESlateVisibility::Collapsed);
        if(auto* Border=Cast<UBorder>(W))Border->SetBrushColor(FLinearColor::Transparent);
    });
    int32 Index=0;for(UImage* I:{TemperatureImage.Get(),AirHumidityImage.Get(),LuminosityImage.Get()})
    {
        const int32 Row=Index++;if(!I)continue;
        I->SetColorAndOpacity(FLinearColor::White);
        if(auto* ImageSlot=Cast<UCanvasPanelSlot>(I->Slot)){ImageSlot->SetAnchors(FAnchors(0,0));ImageSlot->SetAlignment(FVector2D::ZeroVector);ImageSlot->SetPosition(FVector2D(0,Row*80));ImageSlot->SetSize(FVector2D(72));}
    }
}
