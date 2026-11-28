#include "BotanicusPlayerController.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "InputKeyEventArgs.h"

#include "BotanicusCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "UI/BotanicusDevelopmentPanelWidget.h"
#include "UI/BotanicusOrderCatalogWidget.h"
#include "UI/BotanicusRadialEmoteMenu.h"
#include "UI/BotanicusWorkbenchUpgradeWidget.h"

bool ABotanicusPlayerController::CanUseRobotEmotes(bool bReport)
{
	const auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn());
	if (!PlayerCharacter)
		return false;

	if (PlayerCharacter->GetCarriedParcelForAnimation() || PlayerCharacter->IsCarryingEquipment() ||
		HasCarriedTransplant() || IsValid(LocalMovedPlaceableItem) || IsValid(ServerMovedPlaceableItem))
	{
		if (bReport)
			ClientMessage(NSLOCTEXT("BotanicusEmotes", "PutObjectDown", "Posez votre objet pour jouer une emote.").ToString());
		return false;
	}

	const bool bBusy = bServerEmotePlacementActive || IsQuickBarInputBlocked(false) || bLocalBookSessionActive ||
		ServerBookSession.IsValid() || PlayerCharacter->IsUsingOrderComputer() || ActiveComputerView.IsValid() ||
		bParcelCutActionHeld || bWateringCanSprayHeld || bWaterRefillActionHeld || IsValid(ServerActiveWaterReserve) ||
		bPlantPotActionHeld || bQuickBarReorganizationMode || bHudCursorModeActive || IsFurnitureMoveModeActiveForGameplay() ||
		IsValid(LocalStorageCollectionItem) || bCheckoutContextBlocked ||
		(DevelopmentPanelWidget && DevelopmentPanelWidget->IsVisible()) ||
		(OrderCatalogWidget && OrderCatalogWidget->IsVisible()) ||
		(WorkbenchUpgradeWidget && WorkbenchUpgradeWidget->IsVisible());
	bool bUsingTool = false;
	if (!bBusy)
		for (const auto Action : {EBotanicusRobotAction::CutTape, EBotanicusRobotAction::Water,
			EBotanicusRobotAction::PourSoil, EBotanicusRobotAction::Scan})
			if (PlayerCharacter->GetRobotActionState(Action).Action != EBotanicusRobotAction::None)
			{
				bUsingTool = true;
				break;
			}
	if ((bBusy || bUsingTool) && bReport)
		ClientMessage(NSLOCTEXT("BotanicusEmotes", "FinishInteraction", "Terminez l’interaction en cours pour jouer une emote.").ToString());
	return !bBusy && !bUsingTool;
}

void ABotanicusPlayerController::BindRobotEmoteInput(UEnhancedInputComponent& Input)
{
	if (!EmoteMenuInputAction || !EmoteMenuMappingContext)
		return;
	if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		Subsystem->AddMappingContext(EmoteMenuMappingContext, 10);

	Input.BindAction(EmoteMenuInputAction, ETriggerEvent::Started, this, &ABotanicusPlayerController::ToggleRadialEmoteMenu);
}

bool ABotanicusPlayerController::AreRobotEmoteControlsLocked() const
{
	const auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn());
	return PlayerCharacter && PlayerCharacter->IsRobotEmotePlaying();
}

void ABotanicusPlayerController::AddYawInput(float Val)
{
	auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn());
	if (IsLookInputIgnored() || !PlayerCharacter || !PlayerCharacter->AddEmoteCameraOrbit(Val, 0.f))
		Super::AddYawInput(Val);
}

void ABotanicusPlayerController::AddPitchInput(float Val)
{
	auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn());
	if (IsLookInputIgnored() || !PlayerCharacter || !PlayerCharacter->AddEmoteCameraOrbit(0.f, Val))
		Super::AddPitchInput(Val);
}

bool ABotanicusPlayerController::HandleRobotEmoteInput(const FInputKeyEventArgs& Params)
{
	if (!IsLocalPlayerController())
		return false;

	if (RadialEmoteMenu && RadialEmoteMenu->IsMenuOpen() && Params.Key != EKeys::G)
	{
		if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
			CloseRadialEmoteMenu();
		return true;
	}

	if (auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn());
		PlayerCharacter && PlayerCharacter->IsRobotEmotePlaying())
	{
		if (Params.Key == EKeys::Z || Params.Key == EKeys::S)
		{
			if (Params.Event == IE_Pressed || Params.Event == IE_Repeat)
			{
				bAzertyLeftPressed = false;
				bAzertyRightPressed = false;
				PlayerCharacter->InterruptLocalRobotEmote();
			}
			return false;
		}
		if (Params.Key == EKeys::SpaceBar || Params.Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			if (Params.Event == IE_Pressed)
				PlayerCharacter->Jump();
			return false;
		}
		if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
		{
			PlayerCharacter->InterruptLocalRobotEmote();
			return true;
		}
		if (Params.Event == IE_Released)
		{
			if (Params.Key == EKeys::Q)
				bAzertyLeftPressed = false;
			if (Params.Key == EKeys::D)
				bAzertyRightPressed = false;
		}
		return Params.Key != EKeys::MouseX && Params.Key != EKeys::MouseY && Params.Key != EKeys::Mouse2D &&
			Params.Key != EKeys::Gamepad_RightX && Params.Key != EKeys::Gamepad_RightY &&
			Params.Key != EKeys::Gamepad_Right2D && Params.Key != EKeys::G;
	}
	return false;
}

void ABotanicusPlayerController::ToggleRadialEmoteMenu()
{
	if (RadialEmoteMenu && RadialEmoteMenu->IsMenuOpen())
	{
		CloseRadialEmoteMenu();
		return;
	}
	const auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn());
	const auto* Catalog = PlayerCharacter ? PlayerCharacter->GetRobotEmoteCatalog() : nullptr;
	if (!Catalog || !CanUseRobotEmotes(true))
		return;

	if (!RadialEmoteMenu)
	{
		const TSubclassOf<UBotanicusRadialEmoteMenu> WidgetClass = EmoteMenuWidgetClass
			? EmoteMenuWidgetClass.Get() : UBotanicusRadialEmoteMenu::StaticClass();
		RadialEmoteMenu = CreateWidget<UBotanicusRadialEmoteMenu>(this, WidgetClass);
		if (!RadialEmoteMenu || !RadialEmoteMenu->AddToPlayerScreen(75))
		{
			RadialEmoteMenu = nullptr;
			return;
		}
	}

	if (!RadialEmoteMenu->Open(*Catalog, PlayerCharacter->GetRobotEmoteSkeleton()))
	{
		ClientMessage(NSLOCTEXT("BotanicusEmotes", "NoEmotes", "Aucune emote disponible.").ToString());
		return;
	}

	bAzertyForwardPressed = false;
	bAzertyBackwardPressed = false;
	bAzertyLeftPressed = false;
	bAzertyRightPressed = false;
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(RadialEmoteMenu->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	int32 Width = 0;
	int32 Height = 0;
	GetViewportSize(Width, Height);
	if (Width > 0 && Height > 0)
		SetMouseLocation(Width / 2, Height / 2);
}

void ABotanicusPlayerController::CloseRadialEmoteMenu()
{
	if (!RadialEmoteMenu || !RadialEmoteMenu->IsMenuOpen())
		return;
	RadialEmoteMenu->Close();
	SetIgnoreLookInput(false);
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ABotanicusPlayerController::SelectRadialRobotEmote(FName EmoteId)
{
	if (!RadialEmoteMenu || !RadialEmoteMenu->IsMenuOpen())
		return;
	CloseRadialEmoteMenu();
	if (CanUseRobotEmotes(true))
		if (auto* PlayerCharacter = Cast<ABotanicusCharacter>(GetPawn()))
			PlayerCharacter->RequestRobotEmote(EmoteId);
}

void ABotanicusPlayerController::UpdateRadialEmoteContext()
{
	const bool bPlacement = IsValid(LocalQuickBarItemPreview);
	if (bLastReportedEmotePlacement != bPlacement)
	{
		bLastReportedEmotePlacement = bPlacement;
		ServerSetEmotePlacementActive(bPlacement);
	}
	if (RadialEmoteMenu && RadialEmoteMenu->IsMenuOpen() && !CanUseRobotEmotes(false))
		CloseRadialEmoteMenu();
}

void ABotanicusPlayerController::ServerSetEmotePlacementActive_Implementation(bool bActive)
{
	bServerEmotePlacementActive = bActive;
}
