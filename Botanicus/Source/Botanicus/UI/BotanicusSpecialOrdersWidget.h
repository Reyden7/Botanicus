#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusSpecialOrdersWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS()
class BOTANICUS_API UBotanicusSpecialOrdersWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void RefreshOrders();
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OrdersText;
	FTimerHandle RefreshTimer;
	FString LastText;
};
