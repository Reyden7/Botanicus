#include "BotanicusPlayerController.h"
#include "Tools/BotanicusMultiToolComponent.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "BotanicusCharacter.h"

bool ABotanicusPlayerController::IsMultiToolContextBlocked() const
{
    const auto* Tool=GetPawn()?GetPawn()->FindComponentByClass<UBotanicusMultiToolComponent>():nullptr;
    const auto Mode=Tool?Tool->GetMode():EBotanicusMultiToolMode::Neutral;
    return IsQuickBarInputBlocked() || bCheckoutContextBlocked || GetCheckoutScanner() || HasCarriedTransplant() ||
        bFurnitureMoveModeActive || bServerFurnitureMoveModeActive || bWateringCanSprayHeld || bWaterRefillActionHeld ||
        bTrowelTransplantActionHeld || bEquipmentCarryKeyHeld || bEquipmentCarryHoldActivated ||
        IsValid(LocalLargeEquipmentPlacement) || IsValid(LocalHeldHeavyEquipment) ||
        (bPlantPotActionHeld && Mode!=EBotanicusMultiToolMode::Shovel) ||
        (bParcelCutActionHeld && Mode!=EBotanicusMultiToolMode::Cutter);
}

bool ABotanicusPlayerController::IsMultiToolCutInProgress() const
{
    const auto* C=Cast<ABotanicusCharacter>(GetPawn());
    return C && C->GetQuickBarComponent()->GetSelectedSlot().ItemKey==UBotanicusMultiToolComponent::ItemKey() &&
        bParcelCutActionHeld && IsValid(LocalActiveParcelCut) && !LocalActiveParcelCut->IsOpened();
}

void ABotanicusPlayerController::PlayMultiToolWhipVisual()
{
    if(auto* Visual=Cast<ABotanicusMultiToolActor>(LocalEquippedQuickBarItem))Visual->PlayWhipSwing();
}
