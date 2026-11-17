#include "Gardening/BotanicusRobotBatteryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
void UBotanicusRobotBatteryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if(WidgetTree && !WidgetTree->RootWidget)
    {
        auto* Background=WidgetTree->ConstructWidget<UBorder>();
        Background->SetBrushColor(FLinearColor(.008f,.018f,.015f,1));Background->SetPadding(FMargin(8));
        auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();Background->SetContent(Overlay);
        Bar=WidgetTree->ConstructWidget<UProgressBar>();
        auto Style=Bar->GetWidgetStyle();
        Style.BackgroundImage.TintColor=FSlateColor(FLinearColor(.025f,.045f,.035f,1));
        Style.FillImage.TintColor=FSlateColor(FLinearColor::White);Bar->SetWidgetStyle(Style);
        Bar->SetFillColorAndOpacity(FLinearColor(.05f,.75f,.18f,1));Bar->SetPercent(0);
        auto* BarSlot=Overlay->AddChildToOverlay(Bar);BarSlot->SetHorizontalAlignment(HAlign_Fill);BarSlot->SetVerticalAlignment(VAlign_Fill);
        Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold",28));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));Label->SetShadowColorAndOpacity(FLinearColor(0,0,0,.9f));Label->SetShadowOffset(FVector2D(1,1));
        auto* TextSlot=Overlay->AddChildToOverlay(Label);TextSlot->SetHorizontalAlignment(HAlign_Center);TextSlot->SetVerticalAlignment(VAlign_Center);
        auto* Design=WidgetTree->ConstructWidget<USizeBox>();Design->SetWidthOverride(256);Design->SetHeightOverride(64);Design->SetContent(Background);
        auto* ResolutionScale=WidgetTree->ConstructWidget<UScaleBox>();ResolutionScale->SetStretch(EStretch::ScaleToFit);ResolutionScale->SetContent(Design);
        WidgetTree->RootWidget=ResolutionScale;
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UBotanicusRobotBatteryWidget::SetBattery(float Fraction,bool bAvailable)
{
    const float Value=bAvailable?FMath::Clamp(Fraction,0.f,1.f):0;
    if(Bar && !FMath::IsNearlyEqual(DisplayedFraction,Value,1.e-5f)){Bar->SetPercent(Value);DisplayedFraction=Value;}
    const int32 Percent=bAvailable?FMath::RoundToInt(Value*100):-1;
    if(Label && LastPercent!=Percent){Label->SetText(bAvailable?FText::Format(NSLOCTEXT("Robot","BatteryPercent","{0}%"),Percent):NSLOCTEXT("Robot","BatteryUnavailable","—"));LastPercent=Percent;}
}
