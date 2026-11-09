// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BotanicusPlantInspectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
UImage* AddInspectionImage(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name,
	const TCHAR* Path, const FVector2D Position, const FVector2D Size,
	const int32 ZOrder = 0)
{
	UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, Path), true);
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(ZOrder);
	return Image;
}

UTextBlock* AddInspectionText(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name,
	const TCHAR* Preview, const FVector2D Position, const FVector2D Size,
	const int32 FontSize, const FLinearColor Color = FLinearColor(0.10f, 0.16f, 0.08f))
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), Name);
	Text->SetText(FText::FromString(Preview));
	Text->SetColorAndOpacity(Color);
	Text->SetJustification(ETextJustify::Center);
	Text->SetFont(FSlateFontInfo(
		LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
		FontSize, TEXT("Bold")));
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Text);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(2);
	return Text;
}
}

void UBotanicusPlantInspectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
		Root->SetWidthOverride(420.0f);
		Root->SetHeightOverride(260.0f);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Root->SetContent(Canvas);
		AddInspectionImage(WidgetTree, Canvas, TEXT("QualityBackground"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantInspect_QualityBackground.T_PlantInspect_QualityBackground"),
			FVector2D(10.0f, 4.0f), FVector2D(400.0f, 76.0f));
		AddInspectionImage(WidgetTree, Canvas, TEXT("PriceBackground"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantInspect_PriceBackground.T_PlantInspect_PriceBackground"),
			FVector2D(10.0f, 66.0f), FVector2D(400.0f, 76.0f));
		QualityText = AddInspectionText(WidgetTree, Canvas, TEXT("QualityText"),
			TEXT("BELLE"), FVector2D(54.0f, 27.0f),
			FVector2D(312.0f, 32.0f), 20);
		AddInspectionImage(WidgetTree, Canvas, TEXT("CreditIcon"),
			TEXT("/Game/Botanicus/UI/HUD/Textures/T_HUD_Credit.T_HUD_Credit"),
			FVector2D(104.0f, 80.0f), FVector2D(50.0f, 50.0f), 2);
		PriceText = AddInspectionText(WidgetTree, Canvas, TEXT("PriceText"),
			TEXT("120"), FVector2D(154.0f, 89.0f),
			FVector2D(162.0f, 32.0f), 20);
		ElementIcon = AddInspectionImage(WidgetTree, Canvas, TEXT("ElementIcon"),
			TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/T_PlantElement_Normal.T_PlantElement_Normal"),
			FVector2D(170.0f, 139.0f), FVector2D(80.0f, 80.0f), 2);
		AgeText = AddInspectionText(WidgetTree, Canvas, TEXT("AgeText"),
			TEXT("02:15"), FVector2D(30.0f, 224.0f),
			FVector2D(360.0f, 28.0f), 14, FLinearColor::White);
		WidgetTree->RootWidget = Root;
	}
	ConfigureContextLayout();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBotanicusPlantInspectionWidget::SetInspectionData(
	const FText& Quality, const int32 Price, const FText& Element,
	UTexture2D* ElementTexture, const int32 WateringCount,
	const float AgeSeconds)
{
	if (QualityText)
	{
		QualityText->SetText(Quality);
	}
	if (PriceText)
	{
		PriceText->SetText(FText::AsNumber(Price));
	}
	if (ElementIcon && ElementTexture)
	{
		ElementIcon->SetBrushFromTexture(ElementTexture, true);
	}
	(void)Element;
	(void)WateringCount;
	if (AgeText)
	{
        if(AgeSeconds<0){AgeText->SetText(FText::FromString(TEXT("—")));return;}
		const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(AgeSeconds));
		AgeText->SetText(FText::FromString(FString::Printf(
			TEXT("%02d:%02d"),
			TotalSeconds / 60, TotalSeconds % 60)));
	}
}

void UBotanicusPlantInspectionWidget::ConfigureContextLayout()
{
    if(!WidgetTree)return;
    UCanvasPanel* Canvas=nullptr;
    WidgetTree->ForEachWidget([&Canvas](UWidget* W){if(!Canvas)Canvas=Cast<UCanvasPanel>(W);});
    if(!Canvas)return;
    if(auto* Root=Cast<USizeBox>(WidgetTree->RootWidget)){Root->SetWidthOverride(420);Root->SetHeightOverride(360);}
    UImage* Back=nullptr;
    WidgetTree->ForEachWidget([&Back,this](UWidget* W)
    {
        if(auto* I=Cast<UImage>(W))
        {
            const FString Name=I->GetName();
            if(Name==TEXT("CreditIcon") || I==ElementIcon || Name==TEXT("QualityBackground") || Name==TEXT("PriceBackground"))return;
            if(!Back && I->GetBrush().GetResourceObject() && I->GetBrush().GetResourceObject()->GetName()==TEXT("bg"))Back=I;
            else I->SetVisibility(ESlateVisibility::Collapsed);
        }
    });
    if(!Back)Back=AddInspectionImage(WidgetTree,Canvas,TEXT("ContextPanelBackground"),TEXT("/Game/Botanicus/UI/Plant/Inspection/Textures/bg.bg"),FVector2D::ZeroVector,FVector2D(420,360),-5);
    auto Place=[](UWidget* W,const FVector2D& P,const FVector2D& S,int32 Z)
    {if(W)if(auto* Slot=Cast<UCanvasPanelSlot>(W->Slot)){Slot->SetAutoSize(false);W->SetRenderTransform(FWidgetTransform());Slot->SetAnchors(FAnchors(0,0));Slot->SetAlignment(FVector2D::ZeroVector);Slot->SetPosition(P);Slot->SetSize(S);Slot->SetZOrder(Z);}};
    Place(Back,FVector2D::ZeroVector,FVector2D(420,360),-5);
    // The existing cream textures include large transparent margins; sample the
    // authored label area without changing the texture or its colours.
    for(const TCHAR* Label:{TEXT("QualityBackground"),TEXT("PriceBackground")})if(auto* Image=Cast<UImage>(GetWidgetFromName(Label)))
    {FSlateBrush Brush=Image->GetBrush();Brush.SetUVRegion(FBox2D(FVector2D(.02,.29),FVector2D(.98,.69)));Image->SetBrush(Brush);}
    Place(GetWidgetFromName(TEXT("QualityBackground")),FVector2D(10,18),FVector2D(400,76),0);
    Place(GetWidgetFromName(TEXT("PriceBackground")),FVector2D(10,98),FVector2D(400,76),0);
    if(!ContextPlantName)ContextPlantName=AddInspectionText(WidgetTree,Canvas,TEXT("ContextPlantName"),TEXT(""),FVector2D(36,36),FVector2D(348,44),24);
    ContextPlantName->SetAutoWrapText(false);
    if(!NameScaleBox)
    {
        ContextPlantName->RemoveFromParent();NameScaleBox=WidgetTree->ConstructWidget<UScaleBox>();
        NameScaleBox->SetStretch(EStretch::ScaleToFit);NameScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
        auto* NameContent=Cast<UScaleBoxSlot>(NameScaleBox->AddChild(ContextPlantName));
        NameContent->SetHorizontalAlignment(HAlign_Center);NameContent->SetVerticalAlignment(VAlign_Center);
        Canvas->AddChildToCanvas(NameScaleBox);
    }
    Place(QualityText,FVector2D(34,181),FVector2D(352,32),2);
    if(QualityText)QualityText->SetColorAndOpacity(FLinearColor::White);
    Place(GetWidgetFromName(TEXT("CreditIcon")),FVector2D(104,112),FVector2D(50),2);
    Place(PriceText,FVector2D(154,121),FVector2D(162,32),2);
    Place(ElementIcon,FVector2D(170,219),FVector2D(80),2);
    Place(AgeText,FVector2D(30,306),FVector2D(360,30),2);
    ApplyPanelWidth(ContextPanelWidth);
}
void UBotanicusPlantInspectionWidget::ApplyPanelWidth(float Width)
{
    ContextPanelWidth=FMath::Clamp(Width,360.f,720.f);
    if(ContextPlantName)ContextPlantName->SetWrapTextAt(ContextPanelWidth-72);
    if(auto* Root=Cast<USizeBox>(WidgetTree->RootWidget))Root->SetWidthOverride(ContextPanelWidth);
    auto Place=[](UWidget* Widget,const FVector2D& Position,const FVector2D& Size)
    {
        if(Widget)if(auto* CanvasSlot=Cast<UCanvasPanelSlot>(Widget->Slot))
        {CanvasSlot->SetAutoSize(false);CanvasSlot->SetPosition(Position);CanvasSlot->SetSize(Size);}
    };
    UImage* Back=nullptr;WidgetTree->ForEachWidget([&Back](UWidget* Widget)
    {if(auto* Image=Cast<UImage>(Widget);Image && Image->GetBrush().GetResourceObject() && Image->GetBrush().GetResourceObject()->GetName()==TEXT("bg"))Back=Image;});
    Place(Back,FVector2D::ZeroVector,FVector2D(ContextPanelWidth,360));
    Place(GetWidgetFromName(TEXT("QualityBackground")),FVector2D(10,18),FVector2D(ContextPanelWidth-20,76));
    Place(GetWidgetFromName(TEXT("PriceBackground")),FVector2D(10,98),FVector2D(ContextPanelWidth-20,76));
    Place(NameScaleBox,FVector2D(36,26),FVector2D(ContextPanelWidth-72,60));
    Place(QualityText,FVector2D(34,181),FVector2D(ContextPanelWidth-68,32));
    Place(GetWidgetFromName(TEXT("CreditIcon")),FVector2D(ContextPanelWidth*.5f-106,112),FVector2D(50));
    Place(PriceText,FVector2D(ContextPanelWidth*.5f-56,121),FVector2D(162,32));
    Place(ElementIcon,FVector2D(ContextPanelWidth*.5f-40,219),FVector2D(80));
    Place(AgeText,FVector2D(30,306),FVector2D(ContextPanelWidth-60,30));
    InvalidateLayoutAndVolatility();
}
void UBotanicusPlantInspectionWidget::SetPlantName(const FText& Name)
{
    if(!ContextPlantName)return;
    const FText Label=FText::FromString(Name.ToString().ToUpper());if(ContextPlantName->GetText().EqualTo(Label))return;
    ContextPlantName->SetText(Label);
    const float TextWidth=FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label,ContextPlantName->GetFont()).X:288.f;
    ApplyPanelWidth(TextWidth+72.f);
}
