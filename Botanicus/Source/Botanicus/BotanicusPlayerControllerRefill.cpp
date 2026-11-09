#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UI/BotanicusInteractionTargetWidget.h"
#include "InputKeyEventArgs.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"

bool ABotanicusPlayerController::TraceRefillStationAim(FHitResult& Hit) const
{
    if(!GetWorld() || !GetPawn())return false;
    FVector Eye;FRotator View;GetPlayerViewPoint(Eye,View);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RefillFaucetAim),true,GetPawn());
    if(IsValid(LocalEquippedQuickBarItem))Q.AddIgnoredActor(LocalEquippedQuickBarItem.Get());
    const float Range=MaximumWorldInteractionDistance+200;
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*Range,ECC_Visibility,Q);
    float Closest=bHit ? Hit.Distance:Range;
    // The faucet's aim target is query-only in this function. It must never become
    // a surface hit by the existing ground-placement trace directly underneath it.
    for(TActorIterator<ABotanicusWaterReserveActor> It(GetWorld());It;++It)
    {
        if(!It->IsRealStation())continue;
        const FVector Outlet=It->FaucetOutlet->GetComponentLocation(),Delta=Outlet-Eye;
        const float Along=FVector::DotProduct(Delta,View.Vector());
        const float Radius=It->FaucetInteraction->GetScaledBoxExtent().GetMin();
        if(Along<0 || Along>Closest || FVector::DistSquared(Outlet,Eye+View.Vector()*Along)>FMath::Square(Radius))continue;
        Hit=FHitResult(*It,It->FaucetInteraction,Outlet,-View.Vector());Hit.bBlockingHit=true;Hit.Distance=Along;Closest=Along;
    }
    return Hit.GetActor() && FVector::DistSquared2D(GetPawn()->GetActorLocation(),Hit.ImpactPoint)<=FMath::Square(MaximumWorldInteractionDistance);
}
bool ABotanicusPlayerController::TryWaterReserveInput(const FInputKeyEventArgs& Params)
{
    if(!IsLocalController() || bCheckoutContextBlocked || !GetPawn() || LocalQuickBarItemPreview || LocalMovedPlaceableItem || LocalLargeEquipmentPlacement || LocalParcelMovePreview)return false;
    FHitResult Hit;if(!TraceRefillStationAim(Hit))return false;
    auto* Station=Cast<ABotanicusWaterReserveActor>(Hit.GetActor());if(!Station || !Station->IsRealStation())return false;
    // Left click cannot start the retired held-can refill (or inadvertently spray at the reserve).
    if(Params.Key==EKeys::LeftMouseButton && Params.Event==IE_Pressed)return true;
    if(Params.Key!=EKeys::E || Params.Event!=IE_Pressed)return false;
    ServerSetRefillFaucet(Station,Station->GetDockedCan(),!Station->IsOpen());return true;
}
void ABotanicusPlayerController::ServerSetRefillFaucet_Implementation(ABotanicusWaterReserveActor* Station,ABotanicusWateringCanActor* ExpectedCan,bool bOpen)
{
    FHitResult Hit;
    if(!IsValid(Station) || !Station->IsRealStation() || !TraceRefillStationAim(Hit) || Hit.GetActor()!=Station)return;
    Station->RequestFaucet(ExpectedCan,bOpen);
}
bool ABotanicusPlayerController::TryRefreshRefillStationContext()
{
    if(bCheckoutContextBlocked || !InteractionTargetWidget)return false;
    FHitResult Hit;if(!TraceRefillStationAim(Hit))return false;
    auto* Station=Cast<ABotanicusWaterReserveActor>(Hit.GetActor());if(!Station || !Station->IsRealStation())return false;
    auto* Can=Station->GetDockedCan();
    if(Station->IsOpen() && Can)
    {
        InteractionTargetWidget->SetKeyboardPrompt(FText::FromString(TEXT("Fermer le robinet")),
            FText::FromString(FString::Printf(TEXT("Remplissage : %d %%"),FMath::RoundToInt(Can->GetWaterLevel()*100))));
    }
    else if(!Can)InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Placez un arrosoir dans la zone verte")));
    else if(Can->IsFull())InteractionTargetWidget->SetContextStatus(FText::FromString(TEXT("Arrosoir plein")));
    else InteractionTargetWidget->SetKeyboardPrompt(FText::FromString(TEXT("Ouvrir le robinet")),FText::FromString(TEXT("Remplissage de l'arrosoir")));
    return true;
}
bool ABotanicusPlayerController::WantsRefillZone(const ABotanicusWaterReserveActor* Station) const
{
    if(!Station || !GetPawn() || !IsLocalController() || FVector::DistSquared(GetPawn()->GetActorLocation(),Station->GetActorLocation())>FMath::Square(Station->ZoneDisplayDistance))return false;
    const bool Placing=Cast<ABotanicusWateringCanActor>(LocalQuickBarItemPreview.Get())!=nullptr;
    FHitResult Aim;const bool Interacting=TraceRefillStationAim(Aim) && Aim.GetActor()==Station;
    if(Interacting)return true; // The accepted aim already establishes an unobstructed view of this reserve.
    if(!Placing && !Station->IsOpen())return false;
    FVector Eye;FRotator View;GetPlayerViewPoint(Eye,View);FCollisionQueryParams Q(SCENE_QUERY_STAT(RefillZoneSight),true,GetPawn());
    if(LocalQuickBarItemPreview)Q.AddIgnoredActor(LocalQuickBarItemPreview);
    if(IsValid(LocalEquippedQuickBarItem))Q.AddIgnoredActor(LocalEquippedQuickBarItem.Get());
    FHitResult Sight;const bool Blocked=GetWorld()->LineTraceSingleByChannel(Sight,Eye,Station->FaucetOutlet->GetComponentLocation(),ECC_Visibility,Q);
    return !Blocked || Sight.GetActor()==Station;
}
