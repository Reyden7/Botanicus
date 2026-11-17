#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusRobotBatteryWidget.generated.h"
class UProgressBar;
class UTextBlock;
/** Local station readout; driven by the existing station timer, no bindings or widget tick. */
UCLASS(meta=(DisableNativeTick))
class BOTANICUS_API UBotanicusRobotBatteryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetBattery(float Fraction,bool bAvailable);
    float GetDisplayedFraction() const {return DisplayedFraction;}
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
    float DisplayedFraction=-1;
    int32 LastPercent=-2;
};
