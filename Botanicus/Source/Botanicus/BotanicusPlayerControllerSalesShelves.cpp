#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalesShelfActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "UI/BotanicusInteractionTargetWidget.h"
#include "Engine/World.h"
bool ABotanicusPlayerController::HasSalePotForDeposit() const
{
    if(!GetPawn() || IsValid(LocalQuickBarItemPreview) || IsValid(LocalLargeEquipmentPlacement) || bFurnitureMoveModeActive || GetCheckoutScanner() || IsValid(LocalParcelMovePreview.Get()) || !CarriedTransplantPlantKey.IsNone())return false;
    auto* PlayerCharacter=Cast<ABotanicusCharacter>(GetPawn());
    if(!PlayerCharacter || PlayerCharacter->IsCarryingEquipment() || PlayerCharacter->GetHeldWateringCan())return false;
    if(auto* Held=Cast<ABotanicusSalePotActor>(LocalMovedPlaceableItem.Get()))return Held->IsReadyForSale();
    if(IsValid(LocalMovedPlaceableItem))return false;
    const auto Slot=PlayerCharacter->GetQuickBarComponent()->GetSelectedSlot();
    return !Slot.IsEmpty() && (Slot.ItemKey==TEXT("SalePot") || Slot.ItemKey==TEXT("SalePotSquare")) && Slot.CarriedState.bHasSalePotState && !Slot.CarriedState.SalePlantItemKey.IsNone() && !Slot.CarriedState.SaleSoilItemKey.IsNone();
}
bool ABotanicusPlayerController::FindAimedSalesDisplay(ABotanicusSalesDisplayActor*& Out) const
{
    Out=nullptr;if(!GetWorld() || !GetPawn())return false;
    FVector Eye;FRotator View;GetPlayerViewPoint(Eye,View);FCollisionQueryParams Q(SCENE_QUERY_STAT(SalesDepositAim),true,GetPawn());
    if(IsValid(LocalMovedPlaceableItem.Get()))Q.AddIgnoredActor(LocalMovedPlaceableItem.Get());
    if(IsValid(ServerMovedPlaceableItem.Get()))Q.AddIgnoredActor(ServerMovedPlaceableItem.Get());
    if(IsValid(LocalEquippedQuickBarItem.Get()))Q.AddIgnoredActor(LocalEquippedQuickBarItem.Get());
    FHitResult Hit;if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*450,ECC_Visibility,Q))return false;
    Out=Cast<ABotanicusSalesDisplayActor>(Hit.GetActor());
    if(auto* Shelf=Cast<ABotanicusSalesShelfActor>(Hit.GetActor()))Out=Shelf->FindSlotAtPoint(Hit.ImpactPoint);
    return IsValid(Out) && Out->IsDurableSaleDestination() && FVector::DistSquared(GetPawn()->GetActorLocation(),Out->GetSalePotPlacementTransform().GetLocation())<=FMath::Square(450.f);
}
bool ABotanicusPlayerController::TryRefreshSalesDepositContext()
{
    if(!HasSalePotForDeposit() || !InteractionTargetWidget)return false;
    ABotanicusSalesDisplayActor* Display=nullptr;if(!FindAimedSalesDisplay(Display))return false;
    InteractionTargetWidget->SetContextStatus(FText::FromString(Display->IsEmpty() ? TEXT("Clic gauche — Poser le pot"):TEXT("Emplacement occupé")));return true;
}
