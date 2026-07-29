// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BotanicusBuildingMenuWidget.generated.h"

class UButton;
class UWidget;

/**
 * Native extension for the EBS building menu.
 *
 * The marketplace Widget Blueprint is duplicated and reparented to this class,
 * preserving its existing layout while adding a functional Botanicus view button.
 */
UCLASS(Abstract, Blueprintable)
class BOTANICUS_API UBotanicusBuildingMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleTopDownViewClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> TopDownViewButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ObservedBuildingMenu;
};
