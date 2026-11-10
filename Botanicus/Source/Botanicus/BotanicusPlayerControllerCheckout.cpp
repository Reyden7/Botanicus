#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Sales/Checkout/BotanicusPaymentCoins.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "UI/BotanicusInteractionTargetWidget.h"
#include "InputKeyEventArgs.h"
#include "Engine/World.h"

ABotanicusCheckoutScanner* ABotanicusPlayerController::GetCheckoutScanner() const {auto* Tool=LocalCheckoutScanner.Get();return Tool && GetPawn() && Tool->GetCarrier()==GetPawn() ? Tool : nullptr;}
void ABotanicusPlayerController::SetCheckoutScannerReference(ABotanicusCheckoutScanner* Scanner)
{
    if(LocalCheckoutScanner.Get()==Scanner) return;
    if(auto* Old=CheckoutSuppressedEquipment.Get()) Old->SetActorHiddenInGame(bCheckoutEquipmentWasHidden);
    CheckoutSuppressedEquipment.Reset();LocalCheckoutScanner=Scanner;
}
bool ABotanicusPlayerController::CanTakeCheckoutScanner() const
{
    const auto* PlayerCharacter=Cast<ABotanicusCharacter>(GetPawn());
    return PlayerCharacter && !GetCheckoutScanner() && !PlayerCharacter->IsCarryingEquipment() && !PlayerCharacter->GetHeldWateringCan() &&
        CarriedTransplantPlantKey.IsNone() && !IsValid(LocalMovedPlaceableItem) && !IsValid(LocalQuickBarItemPreview) &&
        !IsValid(LocalLargeEquipmentPlacement) && !bWateringCanSprayHeld && !bPlantPotActionHeld && !bWaterRefillActionHeld;
}
void ABotanicusPlayerController::UpdateCheckoutEquipmentPresentation()
{
    const bool bHeld=LocalCheckoutScanner.IsValid() && LocalCheckoutScanner->GetCarrier()==GetPawn();
    auto* Item=bHeld ? LocalEquippedQuickBarItem.Get() : nullptr;
    if(CheckoutSuppressedEquipment.Get()!=Item)
    {
        if(auto* Previous=CheckoutSuppressedEquipment.Get()) Previous->SetActorHiddenInGame(bCheckoutEquipmentWasHidden);
        CheckoutSuppressedEquipment=Item;if(Item) bCheckoutEquipmentWasHidden=Item->IsHidden();
    }
    if(Item) Item->SetActorHiddenInGame(true); // This representation is already local and non-replicated (3B.1).
}
bool ABotanicusPlayerController::TraceCheckoutAim(FHitResult& Hit) const
{
    if(!GetWorld() || !GetPawn()) return false;FVector Eye;FRotator Rotation;GetPlayerViewPoint(Eye,Rotation);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BotanicusCheckoutAim),true,GetPawn());
    if(auto* Tool=GetCheckoutScanner()) Q.AddIgnoredActor(Tool);
    return GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Rotation.Vector()*(MaximumWorldInteractionDistance+200),ECC_Visibility,Q) &&
        FVector::DistSquared2D(GetPawn()->GetActorLocation(),Hit.ImpactPoint)<=FMath::Square(MaximumWorldInteractionDistance);
}
bool ABotanicusPlayerController::TryCheckoutInput(const FInputKeyEventArgs& Params)
{
    if(bCheckoutContextBlocked || !IsLocalController() || !GetPawn() || bFurnitureMoveModeActive || IsValid(LocalMovedPlaceableItem) || IsValid(LocalQuickBarItemPreview) || IsValid(LocalLargeEquipmentPlacement) || IsValid(LocalParcelMovePreview)) return false;
    auto* Held=GetCheckoutScanner();
    if(Held && Params.Key==EKeys::A) return true;
    if(Params.Event!=IE_Pressed || (Params.Key!=EKeys::E && Params.Key!=EKeys::LeftMouseButton)) return false;
    FHitResult Hit;const bool bHit=TraceCheckoutAim(Hit);
    if(Params.Key==EKeys::E && bHit)
    {
        if(auto* Scanner=Cast<ABotanicusCheckoutScanner>(Hit.GetActor());Scanner && !Scanner->GetCarrier()) {if(CanTakeCheckoutScanner()) ServerTakeCheckoutScanner(Scanner);return true;}
        if(auto* Register=Cast<ABotanicusCashRegisterActor>(Hit.GetActor());Register && Register->IsScannerDockHit(Hit.GetComponent()))
        {
            if(Held && Held->GetRegister()==Register){ServerReturnCheckoutScanner(Held);return true;}
            auto* Scanner=Register->GetDockScanner();
            if(!Held && Scanner && !Scanner->GetCarrier()){if(CanTakeCheckoutScanner())ServerTakeCheckoutScanner(Scanner);return true;}
        }
    }
    if(Params.Key==EKeys::LeftMouseButton)
    {
        if(bHit) if(auto* Coins=Cast<ABotanicusPaymentCoins>(Hit.GetActor());Coins && Coins->GetRegister())
        {ServerCollectCheckoutCoins(Coins,Coins->GetRegister()->GetReceipt().TransactionId);return true;}
        if(Held)
        {
            if(Held->GetRegister() && Held->GetRegister()->IsCheckoutScanning())return true;
            if(bHit) if(auto* Customer=Cast<ABotanicusVisitorCharacter>(Hit.GetActor())) ServerScanCheckoutCustomer(Held,Customer,Customer->GetCheckoutCustomerId(),Customer->GetCarriedCheckoutPlantKey());
            return true;
        }
    }
    return false;
}
void ABotanicusPlayerController::ServerTakeCheckoutScanner_Implementation(ABotanicusCheckoutScanner* Scanner)
{
    FHitResult Hit;if(!IsValid(Scanner) || !CanTakeCheckoutScanner() || !TraceCheckoutAim(Hit)) return;
    auto* Register=Scanner->GetRegister();
    const bool bDock=Register && Register->GetDockScanner()==Scanner && Hit.GetActor()==Register && Register->IsScannerDockHit(Hit.GetComponent());
    if(Hit.GetActor()!=Scanner && !bDock)return;
    if(Scanner->TryTake(this)) SetCheckoutScannerReference(Scanner);
}
void ABotanicusPlayerController::ServerReturnCheckoutScanner_Implementation(ABotanicusCheckoutScanner* Scanner)
{
    FHitResult Hit;auto* Register=IsValid(Scanner) ? Scanner->GetRegister() : nullptr;
    if(!Register || Scanner->GetCarrier()!=GetPawn() || !TraceCheckoutAim(Hit) || Hit.GetActor()!=Register || !Register->IsScannerDockHit(Hit.GetComponent())) return;
    Scanner->ReturnToDock();SetCheckoutScannerReference(nullptr);
}
bool ABotanicusPlayerController::CanScanCheckoutCustomer(ABotanicusCheckoutScanner* Scanner,ABotanicusVisitorCharacter* Customer,FGuid ExpectedCustomer,FName ExpectedPlant,bool bRequireAim) const
{
    FHitResult Hit;
    if(!IsValid(GetPawn()) || !IsValid(Scanner) || !IsValid(Customer) || Scanner->GetCarrier()!=GetPawn() || Customer->GetCheckoutCustomerId()!=ExpectedCustomer ||
        Customer->GetCarriedCheckoutPlantKey()!=ExpectedPlant || !Customer->CanUsePlayerCheckout())return false;
    if(bRequireAim)return TraceCheckoutAim(Hit) && Hit.GetActor()==Customer && Customer->IsCheckoutPlantHit(Hit.GetComponent());
    // The accepted target stays locked during the sweep. Recheck range and occlusion,
    // without imposing a new requirement to hold the reticle on a moving leaf.
    FBox Bounds;if(!Customer->GetCheckoutPlantVisualBounds(Bounds) || FVector::DistSquared2D(GetPawn()->GetActorLocation(),Bounds.GetCenter())>FMath::Square(MaximumWorldInteractionDistance))return false;
    FVector Eye;FRotator View;GetPlayerViewPoint(Eye,View);FCollisionQueryParams Query(SCENE_QUERY_STAT(CheckoutScanCompletion),true,GetPawn());Query.AddIgnoredActor(Scanner);
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Bounds.GetCenter(),ECC_Visibility,Query) || (Hit.GetActor()==Customer && Customer->IsCheckoutPlantHit(Hit.GetComponent()));
}
void ABotanicusPlayerController::ServerScanCheckoutCustomer_Implementation(ABotanicusCheckoutScanner* Scanner,ABotanicusVisitorCharacter* Customer,FGuid ExpectedCustomer,FName ExpectedPlant)
{
    if(!CanScanCheckoutCustomer(Scanner,Customer,ExpectedCustomer,ExpectedPlant))return;
    if(auto* Register=Scanner->GetRegister()) Register->TryScanCustomer(this,Customer);
}
void ABotanicusPlayerController::ServerCollectCheckoutCoins_Implementation(ABotanicusPaymentCoins* Coins,FGuid ExpectedTransaction)
{
    FHitResult Hit;if(!IsValid(Coins) || !Coins->GetRegister() || !TraceCheckoutAim(Hit) || Hit.GetActor()!=Coins) return;
    Coins->GetRegister()->TryCollectPayment(this,ExpectedTransaction);
}
bool ABotanicusPlayerController::TryRefreshCheckoutContext()
{
    if(bCheckoutContextBlocked || !InteractionTargetWidget || !GetPawn() || bFurnitureMoveModeActive || IsValid(LocalMovedPlaceableItem) || IsValid(LocalQuickBarItemPreview) || IsValid(LocalLargeEquipmentPlacement) || IsValid(LocalParcelMovePreview)) return false;
    FHitResult Hit;if(!TraceCheckoutAim(Hit)) return false;
    auto* Held=GetCheckoutScanner();ABotanicusCashRegisterActor* Register=nullptr;
    if(auto* Scanner=Cast<ABotanicusCheckoutScanner>(Hit.GetActor()))
    {
        if(!Scanner->GetCarrier()) InteractionTargetWidget->SetKeyboardPrompt(FText::FromString(TEXT("Prendre la scanette")),FText::FromString(TEXT("Scanette")));
        else InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Scanette utilisée")));
        return true;
    }
    if(auto* Coins=Cast<ABotanicusPaymentCoins>(Hit.GetActor())) Register=Coins->GetRegister();
    if(auto* Cash=Cast<ABotanicusCashRegisterActor>(Hit.GetActor()))
    {
        Register=Cash;
        if(Cash->IsScannerDockHit(Hit.GetComponent()) && Held && Held->GetRegister()==Cash)
        {InteractionTargetWidget->SetKeyboardPrompt(FText::FromString(TEXT("Reposer la scanette")),FText::FromString(TEXT("Support")));return true;}
        if(Cash->IsScannerDockHit(Hit.GetComponent()) && !Held && Cash->GetDockScanner() && !Cash->GetDockScanner()->GetCarrier())
        {InteractionTargetWidget->SetKeyboardPrompt(FText::FromString(TEXT("Prendre la scanette")),FText::FromString(TEXT("Support")));return true;}
    }
    if(auto* Customer=Cast<ABotanicusVisitorCharacter>(Hit.GetActor());Customer && Customer->CanUsePlayerCheckout())
    {
        if(!Held) {InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Prenez la scanette à côté de la caisse")));return true;}
        Register=Held->GetRegister();
        if(!Register || !Register->IsOperational())
        {InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Caisse hors zone de paiement")));return true;}
        if(Register->GetCheckoutCustomer()!=Customer)
        {InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Ce client n'attend pas cette caisse")));return true;}
    }
    if(!Register) return false;
    if(Register->IsCheckoutScanning()){InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Analyse de la plante...")));return true;}
    const auto& R=Register->GetReceipt();
    if(R.Phase==EBotanicusCheckoutPhase::AwaitingCollection)
    {
        const FString Value=FString::Printf(TEXT("%d crédits (%d pièces de 10%s)"),R.Amount,R.VisibleGoldCoins,R.VoucherCredits>0 ? *FString::Printf(TEXT(" + bon exact de %d"),R.VoucherCredits) : TEXT(""));
        if(Cast<ABotanicusPaymentCoins>(Hit.GetActor())) InteractionTargetWidget->SetLeftMousePrompt(FText::FromString(TEXT("Encaisser les pièces")),FText::FromString(Value),false);
        else InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Récupérez les pièces sur le comptoir")));
    }
    else if(R.Phase==EBotanicusCheckoutPhase::Opening) InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Paiement en cours...")));
    else if(R.Phase==EBotanicusCheckoutPhase::Collecting || R.Phase==EBotanicusCheckoutPhase::Closing) InteractionTargetWidget->SetContextStatus(FText::FromString(FString::Printf(TEXT("+%d crédits"),R.Amount)));
    else if(!Register->IsOperational()) InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Caisse hors zone de paiement")));
    else if(Held && Register->GetCheckoutCustomer()) InteractionTargetWidget->SetLeftMousePrompt(FText::FromString(TEXT("Scanner la plante")),FText::FromString(TEXT("Visez la plante du client")),false);
    else InteractionTargetWidget->SetContextStatus(FText::FromString(Register->GetCheckoutCustomer() ? TEXT("Prenez la scanette à côté de la caisse") : TEXT("Caisse - en attente d'un client")));
    return true;
}
