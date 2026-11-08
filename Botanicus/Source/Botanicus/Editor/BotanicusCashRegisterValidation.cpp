#include "Editor/BotanicusCashRegisterValidation.h"
#if WITH_EDITOR
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameState.h"
#include "BotanicusGameMode.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Sales/Checkout/BotanicusPaymentCoins.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Visitors/BotanicusVisitorManager.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "Visitors/BotanicusVisitorMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerState.h"
#include "IllegalTrade/BotanicusIllegalCustomerCharacter.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ImageUtils.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
namespace
{
bool bCashValidationExecuting=false;
FGuid CashTestCustomer;FName CashTestPlant;FGuid CashTestSpent;TWeakObjectPtr<ABotanicusPlayerController> CashTestVictim;
UWorld* CashValidationWorld(UObject* Context)
{
    UWorld* W=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;
    FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(Saved);
    return W && W->WorldType==EWorldType::PIE && Saved.Contains(TEXT("/isolated_user/")) && FParse::Param(FCommandLine::Get(),TEXT("BotanicusCashValidation")) ? W : nullptr;
}
ABotanicusCashRegisterActor* CashValidationRegister(UWorld* W) {for(TActorIterator<ABotanicusCashRegisterActor> It(W);It;++It) if(!It->ActorHasTag(TEXT("BotanicusPlacementPreview"))) return *It;return nullptr;}
ABotanicusVisitorCharacter* CashValidationCustomer(UWorld* W) {for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It) if(It->GetCheckoutCustomerId()==CashTestCustomer) return *It;return nullptr;}
}
#endif
bool UBotanicusCashRegisterValidation::Prepare(UObject* Context,int32 Price)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);if(!W || W->GetNetMode()==NM_Client || Price<=0) return false;
    if(!bCashValidationExecuting) {TWeakObjectPtr<UWorld> Weak=W;W->GetTimerManager().SetTimerForNextTick([Weak,Price](){if(Weak.IsValid()){TGuardValue<bool> Guard(bCashValidationExecuting,true);Prepare(Weak.Get(),Price);}});return true;}
    auto* Cash=CashValidationRegister(W);auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!Cash || !PC || !PC->GetPawn() || !Cash->IsOperational()) return false;
    auto* GS=W->GetGameState<ABotanicusGameState>();GS->SetMainShopOpen(true);GS->SetActorTickEnabled(false);
    for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It) It->Destroy();
    auto* Catalog=W->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>();
    auto Definitions=Catalog->GetAllItems();const auto* Choice=Definitions.FindByPredicate([GS](const auto& D){return D.bWholePlant && D.SalePrice>0 && GS->CountMatchingTrends(D)==0;});if(!Choice) return false;
    CashTestPlant=Choice->ItemKey;auto* Runtime=const_cast<FBotanicusItemDefinition*>(Catalog->FindItem(CashTestPlant));if(!Runtime) return false;Runtime->SalePrice=Price;
    if(Cash->DockScanner && Cash->DockScanner->GetCarrier()) Cash->DockScanner->ReturnToDock();
    Cash->Receipt=FBotanicusCheckoutReceipt();Cash->OpeningCustomer.Reset();Cash->NotifyCheckoutChanged();
    int32 I=0;const FVector Forward=Cash->GetActorForwardVector(),Right=Cash->GetActorRightVector();
    const FVector2D Offsets[]={FVector2D(-100,45),FVector2D(-135,-55),FVector2D(-180,140),FVector2D(-180,-150)};
    for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(auto* P=Cast<ABotanicusCharacter>(It->Get()->GetPawn()))
    {
        const auto Offset=Offsets[FMath::Min(I++,3)];FVector Position=Cash->GetActorLocation()+Forward*Offset.X+Right*Offset.Y;Position.Z=P->GetActorLocation().Z;P->SetActorLocation(Position);P->GetCharacterMovement()->StopMovementImmediately();P->ForceNetUpdate();
    }
    for(TActorIterator<ABotanicusSalesDisplayActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CashValidationFixture"))) It->Destroy();
    auto* Display=W->SpawnActor<ABotanicusSalesDisplayActor>();Display->Tags.Add(TEXT("CashValidationFixture"));Display->SetActorLocation(Cash->GetActorLocation()+Right*350);
    auto* Customer=W->SpawnActor<ABotanicusVisitorCharacter>();Customer->SetActorLocation(Cash->GetActorLocation()+Forward*140);Customer->SetActorRotation(FRotator(0,Cash->GetActorRotation().Yaw+180,0));
    if(!Display->TryMountSalePotState(TEXT("PottingSoil"),CashTestPlant,PC) || !Display->TryReserveForVisitor(Customer) || !Display->TakeReservedPlantForVisitor(Customer)) return false;
    Customer->TargetDisplay=Display;Customer->CarriedPlantItemKey=CashTestPlant;Customer->CarriedPotItemKey=TEXT("SalePot");Customer->bCarryingPlant=true;Customer->VisitorState=EBotanicusVisitorState::Paying;
    Customer->CheckoutStage=0;Customer->ManualCheckoutId.Invalidate();Customer->bVisitOutcomeRecorded=false;Customer->CheckoutWaitDuration=2;Customer->RefreshCarriedPlantVisuals();Customer->RefreshCheckoutScanTarget();Customer->ForceNetUpdate();
    CashTestCustomer=Customer->GetCheckoutCustomerId();return Cash->GetCheckoutCustomer()==Customer;
#else
    return false;
#endif
}
bool UBotanicusCashRegisterValidation::Aim(UObject* Context,FName Target)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);if(!W) return false;
    if(!bCashValidationExecuting) {TWeakObjectPtr<UWorld> Weak=W;W->GetTimerManager().SetTimerForNextTick([Weak,Target](){if(Weak.IsValid()){TGuardValue<bool> Guard(bCashValidationExecuting,true);Aim(Weak.Get(),Target);}});return true;}
    auto* Cash=CashValidationRegister(W);auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!Cash || !PC || !PC->GetPawn()) return false;
    FVector Point;
    if(Target==TEXT("Illegal")) {bool Found=false;for(TActorIterator<ABotanicusIllegalCustomerCharacter> It(W);It;++It) if(It->ActorHasTag(TEXT("CashIllegalRegression"))) {Point=It->GetActorLocation()+FVector(0,0,30);Found=true;break;}if(!Found) return false;}
    else if(Target==TEXT("Scanner")) {if(!Cash->GetDockScanner()) return false;Point=Cash->GetDockScanner()->GetActorLocation()+FVector(0,0,5);}
    else if(Target==TEXT("Dock")) Point=Cash->ScannerDock->GetComponentLocation();
    else if(Target==TEXT("Coins")) Point=Cash->GetPaymentCoins() ? Cash->GetPaymentCoins()->GetActorLocation() : Cash->CoinDropPoint->GetComponentLocation();
    else if(auto* Customer=CashValidationCustomer(W)) Point=Target==TEXT("Wrong") ? Customer->GetActorLocation()-FVector(0,0,30) : Customer->GetCheckoutPlantLocation();
    else return false;
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);PC->SetControlRotation((Point-Eye).Rotation());return true;
#else
    return false;
#endif
}
bool UBotanicusCashRegisterValidation::Step(UObject* Context,FName Action)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);if(!W) return false;
    if(!bCashValidationExecuting) {TWeakObjectPtr<UWorld> Weak=W;W->GetTimerManager().SetTimerForNextTick([Weak,Action](){if(Weak.IsValid()){TGuardValue<bool> Guard(bCashValidationExecuting,true);Step(Weak.Get(),Action);}});return true;}
    auto* Cash=CashValidationRegister(W);auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!Cash || !PC) return false;
    auto* Scanner=Cash->GetDockScanner();auto* Customer=CashValidationCustomer(W);
    if(Action==TEXT("DeskSupport"))
    {
        if(W->GetNetMode()==NM_Client) return false;
        UClass* Class=LoadClass<AActor>(nullptr,TEXT("/Game/Botanicus/blueprints/BP_Item_WorkSurfaceMedium.BP_Item_WorkSurfaceMedium_C"));
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Desk=W->SpawnActor<AActor>(Class,Cash->GetActorLocation(),FRotator::ZeroRotator,Params);if(!Desk) return false;
        Desk->Tags.Add(TEXT("CashCounterSupportTest"));float Top=-FLT_MAX;
        TInlineComponentArray<UStaticMeshComponent*> Parts;Desk->GetComponents(Parts);
        for(auto* Part:Parts) if(Part->IsVisible() && Part->GetStaticMesh()) Top=FMath::Max(Top,static_cast<float>(Part->Bounds.Origin.Z+Part->Bounds.BoxExtent.Z));
        if(Top==-FLT_MAX) return false;
        FVector Position=Cash->GetActorLocation();Position.Z=Top+Cash->GetPlacementPivotToBottomOffset();Cash->SetActorLocation(Position);Cash->ForceNetUpdate();return true;
    }
    if(Action==TEXT("PrepareIllegal"))
    {
        if(W->GetNetMode()==NM_Client || !PC->GetPawn()) return false;auto* GS=W->GetGameState<ABotanicusGameState>();const auto* Settings=GetDefault<UBotanicusIllegalTradeSettings>();if(!GS || Settings->Plants.IsEmpty()) return false;
        auto* Pawn=Cast<ABotanicusCharacter>(PC->GetPawn());auto* Bar=Pawn ? Pawn->GetQuickBarComponent() : nullptr;if(!Bar) return false;for(int32 I=0;I<10;++I) Bar->ClearSlot(I);
        const auto& Plant=Settings->Plants[0];int32 Slot;const int32 Quantity=FMath::Max(1,Settings->CustomerOrderQuantity);if(!Bar->AddItem(Plant.ProductItemKey,Quantity,Slot)) return false;
        GS->SetDayTimeMinutesForDevelopment(22*60);FVector Point=Pawn->GetActorLocation()-Cash->GetActorRightVector()*140;FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* C=W->SpawnActor<ABotanicusIllegalCustomerCharacter>(ABotanicusIllegalCustomerCharacter::StaticClass(),Point,FRotator::ZeroRotator,Params);if(!C) return false;C->Tags.Add(TEXT("CashIllegalRegression"));C->InitializeIllegalOrder(Plant.ProductItemKey,Plant.DisplayName,Quantity,Quantity*FMath::Max(1,Plant.UnitSaleValue),Settings->CustomerWaitSeconds,Point,Point-Cash->GetActorForwardVector()*300);return true;
    }
    if(Action==TEXT("IllegalSell")) {for(TActorIterator<ABotanicusIllegalCustomerCharacter> It(W);It;++It) if(It->ActorHasTag(TEXT("CashIllegalRegression"))) {PC->ServerSellIllegalOrder(*It);return true;}return false;}
    if(Action==TEXT("Position"))
    {if(W->GetNetMode()==NM_Client || !PC->GetPawn()) return false;const FVector Original=PC->GetPawn()->GetActorLocation();FVector Point=Cash->GetActorLocation()-Cash->GetActorForwardVector()*100+Cash->GetActorRightVector()*45;Point.Z=Original.Z;PC->GetPawn()->SetActorLocation(Point);return true;}
    if(Action==TEXT("DisconnectCarrier"))
    {if(W->GetNetMode()==NM_Client || !Scanner || !Scanner->GetCarrier()) return false;auto* OwnerPC=Cast<APlayerController>(Scanner->GetCarrier()->GetController());if(OwnerPC && OwnerPC->GetNetConnection()) {OwnerPC->GetNetConnection()->Close();return true;}return false;}
    if(Action==TEXT("DisconnectRemote"))
    {if(W->GetNetMode()==NM_Client) return false;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It) if(!It->Get()->IsLocalController() && It->Get()->GetNetConnection()) {It->Get()->GetNetConnection()->Close();return true;}return false;}
    if(Action==TEXT("Auto"))
    {if(W->GetNetMode()==NM_Client || !Customer) return false;auto* Auto=W->SpawnActor<ABotanicusSelfCheckoutActor>();Auto->Tags.Add(TEXT("CashValidationFixture"));Auto->SetActorTransform(Cash->GetSelfCheckoutSlotTransform(2));if(!Customer->AssignSelfCheckout(Auto)) return false;Customer->SetActorLocation(Auto->GetCustomerStandLocation());return true;}
    if(Action==TEXT("KeyE") || Action==TEXT("Click")) {FInputKeyEventArgs Args;Args.Key=Action==TEXT("KeyE") ? EKeys::E : EKeys::LeftMouseButton;Args.Event=IE_Pressed;return PC->InputKey(Args);}
    if(Action==TEXT("Take")) {PC->ServerTakeCheckoutScanner(Scanner);return true;}
    if(Action==TEXT("Return")) {PC->ServerReturnCheckoutScanner(Scanner);return true;}
    if(Action==TEXT("Scan") || Action==TEXT("WrongPlant"))
    {if(!Customer) return false;PC->ServerScanCheckoutCustomer(Scanner,Customer,Customer->GetCheckoutCustomerId(),Action==TEXT("WrongPlant") ? FName(TEXT("NotTheCustomersPlant")) : Customer->GetCarriedCheckoutPlantKey());return true;}
    if(Action==TEXT("Collect") || Action==TEXT("Replay")) {PC->ServerCollectCheckoutCoins(Cash->GetPaymentCoins(),Action==TEXT("Replay") ? CashTestSpent : Cash->GetReceipt().TransactionId);return true;}
    if(Action==TEXT("Remember")) {CashTestSpent=Cash->GetReceipt().TransactionId;return true;}
    if(Action==TEXT("Off") || Action==TEXT("On")) {GetMutableDefault<UBotanicusCheckoutSettings>()->bAnimationsEnabled=Action==TEXT("On");return true;}
    if(Action==TEXT("DestroyCarrier"))
    {if(W->GetNetMode()==NM_Client || !Scanner || !Scanner->GetCarrier()) return false;CashTestVictim=Cast<ABotanicusPlayerController>(Scanner->GetCarrier()->GetController());Scanner->GetCarrier()->Destroy();return true;}
    if(Action==TEXT("Respawn")) {if(W->GetNetMode()==NM_Client || !CashTestVictim.IsValid()) return false;W->GetAuthGameMode()->RestartPlayer(CashTestVictim.Get());return true;}
    if(Action==TEXT("DestroyCustomer")) {if(W->GetNetMode()==NM_Client || !Customer) return false;Customer->Destroy();return true;}
    if(Action==TEXT("Save")) return W->GetNetMode()!=NM_Client && W->GetAuthGameMode<ABotanicusGameMode>()->BotanicusSaveNow();
    return false;
#else
    return false;
#endif
}
FString UBotanicusCashRegisterValidation::Snapshot(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);if(!W) return TEXT("{}");auto Root=MakeShared<FJsonObject>();
    auto* Cash=CashValidationRegister(W);auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());if(!Cash || !PC) return TEXT("{}");
    auto* GS=W->GetGameState<ABotanicusGameState>();if(GS) {Root->SetNumberField(TEXT("funds"),GS->GetSharedFunds());Root->SetNumberField(TEXT("sales"),GS->GetTotalPlantsSold());Root->SetNumberField(TEXT("revenue"),GS->GetDailyRevenue());Root->SetNumberField(TEXT("reviews"),GS->GetTotalVisitorReviews());Root->SetNumberField(TEXT("suspicion"),GS->GetSuspicion());}
    for(TActorIterator<ABotanicusSelfCheckoutActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CashValidationFixture"))) {Root->SetBoolField(TEXT("autoOperational"),It->IsOperational());Root->SetBoolField(TEXT("autoAssigned"),It->GetAssignedVisitor()!=nullptr);}
    for(TActorIterator<ABotanicusIllegalCustomerCharacter> It(W);It;++It) if(It->ActorHasTag(TEXT("CashIllegalRegression"))) {Root->SetNumberField(TEXT("illegalState"),static_cast<int32>(It->GetCustomerState()));Root->SetNumberField(TEXT("illegalReward"),It->GetRewardCredits());}
    if(auto* Pawn=Cast<ABotanicusCharacter>(PC->GetPawn())) {const auto* S=GetDefault<UBotanicusIllegalTradeSettings>();if(!S->Plants.IsEmpty()) Root->SetNumberField(TEXT("illegalQuantity"),Pawn->GetQuickBarComponent()->GetTotalQuantity(S->Plants[0].ProductItemKey));}
    const auto& R=Cash->GetReceipt();Root->SetNumberField(TEXT("phase"),static_cast<int32>(R.Phase));Root->SetStringField(TEXT("transaction"),R.TransactionId.ToString());Root->SetStringField(TEXT("plantId"),R.PlantInstanceId.ToString());Root->SetStringField(TEXT("customerId"),R.CustomerId.ToString());
    Root->SetNumberField(TEXT("amount"),R.Amount);Root->SetBoolField(TEXT("credited"),R.bCredited);Root->SetNumberField(TEXT("gold"),R.VisibleGoldCoins);Root->SetNumberField(TEXT("voucher"),R.VoucherCredits);Root->SetNumberField(TEXT("duration"),R.CollectionSeconds);
    Root->SetNumberField(TEXT("scansAccepted"),Cash->GetAcceptedScans());Root->SetNumberField(TEXT("collectionsAccepted"),Cash->GetAcceptedCollections());Root->SetBoolField(TEXT("operational"),Cash->IsOperational());Root->SetStringField(TEXT("drawer"),Cash->DrawerVisualRoot->GetRelativeLocation().ToString());
    if(auto* Coins=Cash->GetPaymentCoins()) {Root->SetNumberField(TEXT("renderedGold"),Coins->GetRenderedCoinCount());Root->SetNumberField(TEXT("arrivals"),Coins->GetArrivalCount());
        TInlineComponentArray<UInstancedStaticMeshComponent*> Instances;Coins->GetComponents(Instances);if(Instances.Num() && Instances[0]->GetInstanceCount()) {FTransform First,Last;Instances[0]->GetInstanceTransform(0,First,true);Instances[0]->GetInstanceTransform(Instances[0]->GetInstanceCount()-1,Last,true);Root->SetStringField(TEXT("firstCoin"),First.GetLocation().ToString());Root->SetStringField(TEXT("lastCoin"),Last.GetLocation().ToString());Root->SetNumberField(TEXT("firstScale"),First.GetScale3D().X);Root->SetNumberField(TEXT("lastScale"),Last.GetScale3D().X);}}
    if(auto* Tool=Cash->GetDockScanner()) {Root->SetStringField(TEXT("scanner"),Tool->GetName());Root->SetStringField(TEXT("carrier"),Tool->GetCarrier() ? Tool->GetCarrier()->GetName() : TEXT(""));Root->SetBoolField(TEXT("owned"),Tool->GetCarrier()==PC->GetPawn());Root->SetNumberField(TEXT("carrierPlayerId"),Tool->GetCarrier() && Tool->GetCarrier()->GetPlayerState() ? Tool->GetCarrier()->GetPlayerState()->GetPlayerId() : -1);}
    if(auto* C=CashValidationCustomer(W)) {Root->SetNumberField(TEXT("customerPhase"),static_cast<int32>(C->VisitorState));Root->SetBoolField(TEXT("payable"),C->CanUsePlayerCheckout());Root->SetNumberField(TEXT("nativePrice"),C->GetCheckoutPrice());Root->SetNumberField(TEXT("routePoints"),C->RoutePoints.Num());Root->SetStringField(TEXT("customerLocation"),C->GetActorLocation().ToString());}
    if(auto* Item=PC->LocalEquippedQuickBarItem.Get()) {Root->SetStringField(TEXT("equipped"),Item->GetName());Root->SetBoolField(TEXT("equippedHidden"),Item->IsHidden());}
    FHitResult Hit;if(PC->TraceCheckoutAim(Hit)) {Root->SetStringField(TEXT("hit"),Hit.GetActor()->GetName());Root->SetStringField(TEXT("component"),Hit.GetComponent()->GetName());}
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);Root->SetStringField(TEXT("view"),Eye.ToString()+View.ToString());
    if(auto* J=W->GetSubsystem<UBotanicusJuiceWorldSubsystem>()) {Root->SetNumberField(TEXT("beams"),J->GetCheckoutBeamCount());Root->SetNumberField(TEXT("scanVisuals"),J->GetCheckoutScanCount());}
    if(auto* A=W->GetSubsystem<UBotanicusAudioWorldSubsystem>()) for(const TCHAR* Id:{TEXT("ScannerBeep"),TEXT("RegisterOpen"),TEXT("RegisterClose"),TEXT("CoinPickup"),TEXT("Sale")}) Root->SetNumberField(Id,A->GetEventPlayCount(Id));
    Root->SetNumberField(TEXT("time"),Cash->CheckoutServerTime());FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Root,Writer);return Out;
#else
    return TEXT("{}");
#endif
}

bool UBotanicusCashRegisterValidation::Capture(UObject* Context,const FString& Path)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);FString Full=FPaths::ConvertRelativePathToFull(Path);FPaths::NormalizeFilename(Full);
    FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("captures"));FPaths::NormalizeDirectoryName(Allowed);
    if(!W || !Full.StartsWith(Allowed+TEXT("/")) || !W->GetGameViewport() || !W->GetGameViewport()->Viewport) return false;
    FViewport* Viewport=W->GetGameViewport()->Viewport;TArray<FColor> Pixels;const FIntPoint Size=Viewport->GetSizeXY();
    if(!Viewport->ReadPixels(Pixels) || Pixels.Num()!=Size.X*Size.Y) return false;
    TArray64<uint8> Bytes;Bytes.SetNumUninitialized(8+Pixels.Num()*4);FMemory::Memcpy(Bytes.GetData(),&Size.X,4);FMemory::Memcpy(Bytes.GetData()+4,&Size.Y,4);FMemory::Memcpy(Bytes.GetData()+8,Pixels.GetData(),Pixels.Num()*4);
    return FFileHelper::SaveArrayToFile(Bytes,*Full);
#else
    return false;
#endif
}

bool UBotanicusCashRegisterValidation::PrepareVisitorVisit(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);if(!W || W->GetNetMode()==NM_Client) return false;
    if(!bCashValidationExecuting) {TWeakObjectPtr<UWorld> Weak=W;W->GetTimerManager().SetTimerForNextTick([Weak](){if(Weak.IsValid()){TGuardValue<bool> Guard(bCashValidationExecuting,true);PrepareVisitorVisit(Weak.Get());}});return true;}
    ABotanicusVisitorManager* Manager=nullptr;for(TActorIterator<ABotanicusVisitorManager> It(W);It;++It) {Manager=*It;break;}
    auto* GS=W->GetGameState<ABotanicusGameState>();auto* PC=Cast<ABotanicusPlayerController>(W->GetFirstPlayerController());
    if(!Manager || !GS || !PC) return false;
    GS->SetMainShopOpen(true);GS->SetActorTickEnabled(false);
    Manager->SpawnRemaining=3600;Manager->WaitingVisitors.Reset();Manager->AdmissionRemaining=0;
    for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It) It->Destroy();
    TArray<FVector> Circuit,Arrival,Direct;int32 Checkout=INDEX_NONE;
    if(!Manager->BuildVisitorCircuit(Circuit,Checkout,Arrival,Direct)) return false;
    ABotanicusVisitorZoneActor* Sales=nullptr;for(TActorIterator<ABotanicusVisitorZoneActor> It(W);It;++It) if(It->GetZoneType()==EBotanicusVisitorZoneType::SalesArea) Sales=*It;
    ABotanicusSalesDisplayActor* Display=nullptr;
    for(TActorIterator<ABotanicusSalesDisplayActor> It(W);It;++It) if(Sales && Sales->ContainsPoint2D(It->GetActorLocation()) && !It->ActorHasTag(TEXT("BotanicusPlacementPreview"))) {Display=*It;break;}
    if(!Display) return false; // Require the existing shop layout, including a real display.
    if(Display->IsEmpty())
    {
        auto* Catalog=W->GetGameInstance()->GetSubsystem<UBotanicusItemCatalogSubsystem>();const auto Definitions=Catalog->GetAllItems();
        const auto* Choice=Definitions.FindByPredicate([](const auto& D){return D.bWholePlant && D.SalePrice>0;});
        if(!Choice || !Display->TryMountSalePotState(TEXT("PottingSoil"),Choice->ItemKey,PC)) return false;
    }
    Manager->SpawnQueuedVisitor(Circuit,Checkout,Arrival,Direct);
    if(Manager->WaitingVisitors.IsEmpty() || !Manager->WaitingVisitors.Last().IsValid()) return false;
    auto* Visitor=Manager->WaitingVisitors.Last().Get();CashTestCustomer=Visitor->GetCheckoutCustomerId();CashTestPlant=Display->GetDisplayedPlantItemKey();
    // Isolated fixture only: make this single shopper deterministic, without changing native prices or transactions.
    Visitor->ShoppingBudget=10000;Visitor->MinimumAcceptableScore=-1000;Visitor->DesiredInspectionCount=1;
    return true;
#else
    return false;
#endif
}

FString UBotanicusCashRegisterValidation::VisitorSnapshot(UObject* Context)
{
#if WITH_EDITOR
    UWorld* W=CashValidationWorld(Context);if(!W) return TEXT("{}");auto Root=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Zones;for(TActorIterator<ABotanicusVisitorZoneActor> It(W);It;++It) {auto Z=MakeShared<FJsonObject>();Z->SetNumberField(TEXT("type"),static_cast<int32>(It->GetZoneType()));Z->SetStringField(TEXT("location"),It->GetActorLocation().ToString());Zones.Add(MakeShared<FJsonValueObject>(Z));}Root->SetArrayField(TEXT("zones"),Zones);
    for(TActorIterator<ABotanicusVisitorManager> It(W);It;++It) {TArray<FVector> Circuit,Arrival,Direct;int32 Checkout;Root->SetBoolField(TEXT("realCircuit"),It->BuildVisitorCircuit(Circuit,Checkout,Arrival,Direct));Root->SetNumberField(TEXT("checkoutIndex"),Checkout);TArray<TSharedPtr<FJsonValue>> Points;for(const FVector& P:Circuit) Points.Add(MakeShared<FJsonValueString>(P.ToString()));Root->SetArrayField(TEXT("circuit"),Points);Root->SetNumberField(TEXT("arrivalPoints"),Arrival.Num());break;}
    TArray<TSharedPtr<FJsonValue>> Visitors;
    for(TActorIterator<ABotanicusVisitorCharacter> It(W);It;++It)
    {
        auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("id"),It->GetCheckoutCustomerId().ToString());C->SetNumberField(TEXT("state"),static_cast<int32>(It->VisitorState));C->SetNumberField(TEXT("stage"),It->CheckoutStage);
        C->SetStringField(TEXT("position"),It->GetActorLocation().ToString());C->SetNumberField(TEXT("speed"),It->GetVelocity().Size2D());C->SetNumberField(TEXT("routeIndex"),It->RouteWaypointIndex);C->SetNumberField(TEXT("routeSize"),It->RoutePoints.Num());C->SetNumberField(TEXT("stuck"),It->StuckDuration);
        if(It->RoutePoints.Num()) C->SetNumberField(TEXT("distanceToExit"),FVector::Dist2D(It->GetActorLocation(),It->RoutePoints.Last()));
        C->SetStringField(TEXT("focus"),FVector(It->PresentationFocus).ToString());C->SetBoolField(TEXT("carrying"),It->bCarryingPlant);
        if(auto* Mesh=Cast<UBotanicusVisitorMeshComponent>(It->GetMesh())) {C->SetStringField(TEXT("gaze"),Mesh->GetAppliedGaze().ToString());C->SetStringField(TEXT("spinePose"),Mesh->GetSocketTransform(TEXT("spine_03"),RTS_Component).ToString());C->SetNumberField(TEXT("scanReactions"),Mesh->GetScanReactionCount());C->SetNumberField(TEXT("paymentReactions"),Mesh->GetPaymentReactionCount());C->SetStringField(TEXT("animation"),Mesh->GetAnimInstance() ? Mesh->GetAnimInstance()->GetClass()->GetName() : TEXT(""));}
        Visitors.Add(MakeShared<FJsonValueObject>(C));
    }
    Root->SetArrayField(TEXT("visitors"),Visitors);FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Root,Writer);return Out;
#else
    return TEXT("{}");
#endif
}
