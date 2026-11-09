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
    const FText FormattedPrice=FText::AsNumber(Price);
    const bool ContentChanged=(QualityText && !QualityText->GetText().EqualTo(Quality)) ||
        (PriceText && !PriceText->GetText().EqualTo(FormattedPrice));
	if (QualityText)
	{
		QualityText->SetText(Quality);
	}
	if (PriceText)
	{
		PriceText->SetText(FormattedPrice);
	}
	if (ElementIcon && ElementTexture)
	{
		ElementIcon->SetBrushFromTexture(ElementTexture, true);
	}
	(void)Element;
	(void)WateringCount;
	if (AgeText)
	{
        if(AgeSeconds<0){AgeText->SetText(FText::FromString(TEXT("—")));}
        else
        {
		const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(AgeSeconds));
		AgeText->SetText(FText::FromString(FString::Printf(
			TEXT("%02d:%02d"),
			TotalSeconds / 60, TotalSeconds % 60)));
        }
	}
    if(ContentChanged)ApplyPanelWidth(ContextPanelWidth);
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
    // Keep the validated component envelope/pivot unchanged. Only the card inside
    // it adapts to content, so neither the panel transform nor column spacing moves.
    ContextPanelWidth=FMath::Clamp(Width,360.f,720.f);
    if(!WidgetTree)return;
    if(auto* Root=Cast<USizeBox>(WidgetTree->RootWidget))Root->SetWidthOverride(ContextPanelWidth);
    auto Measure=[](const UTextBlock* Text)
    {
        return Text && FSlateApplication::IsInitialized()
            ? float(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text->GetText(),Text->GetFont()).X):0.f;
    };
    auto Visible=[](const UWidget* Widget)
    {return Widget && Widget->GetVisibility()!=ESlateVisibility::Collapsed && Widget->GetVisibility()!=ESlateVisibility::Hidden;};
    const bool HasName=ContextPlantName && !ContextPlantName->GetText().IsEmpty();
    const bool HasPrice=Visible(PriceText) && !PriceText->GetText().IsEmpty();
    const bool HasQuality=Visible(QualityText) && !QualityText->GetText().IsEmpty();
    const bool HasElement=Visible(ElementIcon) && ElementIcon->GetBrush().GetResourceObject();
    const bool HasTime=Visible(AgeText) && !AgeText->GetText().IsEmpty();
    auto* Credit=GetWidgetFromName(TEXT("CreditIcon"));
    const float CreditWidth=Visible(Credit) ? 50.f:0.f;
    const float CreditGap=CreditWidth>0 ? 12.f:0.f;
    const float PriceWidth=FMath::Max(32.f,Measure(PriceText));
    const float NameWidth=Measure(ContextPlantName);
    const float RequiredWidth=FMath::Max3(NameWidth+16,Measure(QualityText),FMath::Max3(CreditWidth+CreditGap+PriceWidth+16,HasElement ? 80.f:0.f,Measure(AgeText)));
    // The authored background's frame occupies [5.5%,94.5%] x [9%,91%].
    // Respect its decoration and transparent margins instead of cropping its art.
    constexpr float FrameX=.055f,FrameY=.09f,FrameW=.89f,FrameH=.82f,PadX=18,PadY=12,Gap=6;
    const float CardWidth=FMath::Clamp((RequiredWidth+2*PadX)/FrameW,256.f,ContextPanelWidth);
    const float ContentWidth=CardWidth*FrameW-2*PadX;
    const float NameHeight=NameWidth>ContentWidth-16 ? 52.f:32.f;
    if(ContextPlantName)ContextPlantName->SetWrapTextAt(ContentWidth-16);
    float ContentHeight=0;int32 Count=0;
    auto Add=[&](bool Present,float Height){if(Present){if(Count++)ContentHeight+=Gap;ContentHeight+=Height;}};
    Add(HasName,NameHeight+8);Add(HasPrice,58);Add(HasQuality,28);Add(HasElement,80);Add(HasTime,20);
    const float CardHeight=FMath::Clamp((ContentHeight+2*PadY)/FrameH,48.f,360.f);
    const FVector2D CardPosition((ContextPanelWidth-CardWidth)*.5f,(360-CardHeight)*.5f);
    const float Left=float(CardPosition.X)+CardWidth*FrameX+PadX;
    float Y=float(CardPosition.Y)+CardHeight*FrameY+PadY;bool First=true;
    auto Row=[&](bool Present,float Height)
    {if(!Present)return Y;if(!First)Y+=Gap;First=false;const float Start=Y;Y+=Height;return Start;};
    auto Place=[](UWidget* Widget,const FVector2D& Position,const FVector2D& Size)
    {
        if(Widget)if(auto* CanvasSlot=Cast<UCanvasPanelSlot>(Widget->Slot))
        {CanvasSlot->SetAutoSize(false);CanvasSlot->SetPosition(Position);CanvasSlot->SetSize(Size);}
    };
    UImage* Back=nullptr;WidgetTree->ForEachWidget([&Back](UWidget* Widget)
    {if(auto* Image=Cast<UImage>(Widget);Image && Image->GetBrush().GetResourceObject() && Image->GetBrush().GetResourceObject()->GetName()==TEXT("bg"))Back=Image;});
    Place(Back,CardPosition,FVector2D(CardWidth,CardHeight));
    const float NameY=Row(HasName,NameHeight+8);
    auto* NameBackground=GetWidgetFromName(TEXT("QualityBackground"));
    if(NameBackground)NameBackground->SetVisibility(HasName ? ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    Place(NameBackground,FVector2D(Left,NameY),FVector2D(ContentWidth,NameHeight+8));
    Place(NameScaleBox,FVector2D(Left+8,NameY+4),FVector2D(ContentWidth-16,NameHeight));
    const float PriceY=Row(HasPrice,58);
    auto* PriceBackground=GetWidgetFromName(TEXT("PriceBackground"));
    if(PriceBackground)PriceBackground->SetVisibility(HasPrice ? ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    Place(PriceBackground,FVector2D(Left,PriceY),FVector2D(ContentWidth,58));
    const float PriceStart=ContextPanelWidth*.5f-(CreditWidth+CreditGap+PriceWidth)*.5f;
    Place(Credit,FVector2D(PriceStart,PriceY+4),FVector2D(CreditWidth,50));
    Place(PriceText,FVector2D(PriceStart+CreditWidth+CreditGap,PriceY+15),FVector2D(PriceWidth,28));
    Place(QualityText,FVector2D(Left,Row(HasQuality,28)),FVector2D(ContentWidth,28));
    Place(ElementIcon,FVector2D(ContextPanelWidth*.5f-40,Row(HasElement,80)),FVector2D(80));
    Place(AgeText,FVector2D(Left,Row(HasTime,20)),FVector2D(ContentWidth,20));
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
