#include "Sales/BotanicusCashRegisterActor.h"
#include "Engine/World.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Sales/Checkout/BotanicusPaymentCoins.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "BotanicusPlayerController.h"
#include "BotanicusGameState.h"
#include "BotanicusGameMode.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Scalability.h"

void ABotanicusCashRegisterActor::InitializeCheckoutComponents()
{
    auto Point=[this](const TCHAR* Name,const FVector& Location,USceneComponent* Parent=nullptr)
    {auto* C=CreateDefaultSubobject<USceneComponent>(Name);C->SetupAttachment(Parent ? Parent : SceneRoot.Get());C->SetRelativeLocation(Location);C->bEditableWhenInherited=true;return C;};
    ScannerDock=Point(TEXT("ScannerDock"),FVector(-27,22,78));
    CoinDropPoint=Point(TEXT("CoinDropPoint"),FVector(-18,-12,77));
    DrawerClosedTarget=Point(TEXT("DrawerClosedTarget"),FVector(-23,0,69));
    DrawerOpenTarget=Point(TEXT("DrawerOpenTarget"),FVector(-48,0,69));
    DrawerVisualRoot=Point(TEXT("Checkout Drawer"),FVector(-23,0,69));
    DrawerTarget=Point(TEXT("DrawerTarget"),FVector(0,0,3),DrawerVisualRoot);
    ScannerDockHit=CreateDefaultSubobject<UBoxComponent>(TEXT("Scanner Dock Interaction"));ScannerDockHit->SetupAttachment(ScannerDock);ScannerDockHit->SetBoxExtent(FVector(7,8,3));
    ScannerDockHit->SetCollisionEnabled(ECollisionEnabled::QueryOnly);ScannerDockHit->SetCollisionResponseToAllChannels(ECR_Ignore);ScannerDockHit->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);ScannerDockHit->SetGenerateOverlapEvents(false);ScannerDockHit->SetCanEverAffectNavigation(false);
    DrawerMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Drawer Tray"));DrawerMesh->SetupAttachment(DrawerVisualRoot);DrawerMesh->SetRelativeScale3D(FVector(.30f,.25f,.025f));DrawerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DrawerLip=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Drawer Front"));DrawerLip->SetupAttachment(DrawerVisualRoot);DrawerLip->SetRelativeLocation(FVector(-15,0,2));DrawerLip->SetRelativeScale3D(FVector(.025f,.26f,.065f));DrawerLip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ScannerDockMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Scanner Dock Stand"));ScannerDockMesh->SetupAttachment(ScannerDock);ScannerDockMesh->SetRelativeScale3D(FVector(.12f,.12f,.025f));ScannerDockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ABotanicusCashRegisterActor::BeginCheckout()
{
    if(ActorHasTag(TEXT("BotanicusPlacementPreview")) || ActorHasTag(TEXT("BotanicusEquippedPreview")) || !GetIsReplicated()) return;
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();
    ConfigureRegisterRepresentation();
    if(HasAuthority())
    {
        if(!RegisterId.IsValid()) RegisterId=FGuid::NewGuid();
        FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(!DockScanner)
        {UClass* Class=S->ScannerClass.LoadSynchronous();DockScanner=GetWorld()->SpawnActor<ABotanicusCheckoutScanner>(Class ? Class : ABotanicusCheckoutScanner::StaticClass(),Params);if(DockScanner) DockScanner->InitializeDock(this);}
        if(!PaymentCoins)
        {UClass* Class=S->PaymentCoinsClass.LoadSynchronous();PaymentCoins=GetWorld()->SpawnActor<ABotanicusPaymentCoins>(Class ? Class : ABotanicusPaymentCoins::StaticClass(),Params);if(PaymentCoins) PaymentCoins->InitializeRegister(this);}
        ForceNetUpdate();
    }
    OnRep_Checkout();
}
double ABotanicusCashRegisterActor::CheckoutServerTime() const
{
    if(const auto* GS=GetWorld()->GetGameState()) return GS->GetServerWorldTimeSeconds();return GetWorld()->GetTimeSeconds();
}
FVector ABotanicusCashRegisterActor::GetDrawerDepositLocation() const {return DrawerTarget->GetComponentLocation();}
void ABotanicusCashRegisterActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABotanicusCashRegisterActor,Receipt);DOREPLIFETIME(ABotanicusCashRegisterActor,RegisterId);
    DOREPLIFETIME(ABotanicusCashRegisterActor,DockScanner);DOREPLIFETIME(ABotanicusCashRegisterActor,PaymentCoins);
}
void ABotanicusCashRegisterActor::OnRep_Checkout()
{
    if(PaymentCoins) PaymentCoins->RefreshFromReceipt();
    if(Receipt.Phase==EBotanicusCheckoutPhase::Opening && LastBeamTransaction!=Receipt.TransactionId && GetNetMode()!=NM_DedicatedServer)
    {
        LastBeamTransaction=Receipt.TransactionId;
        if(auto* J=GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>()) J->StartCheckoutScan(DockScanner,Receipt.ScanOrigin,Receipt.ScanTarget,static_cast<float>(CheckoutServerTime()-Receipt.PhaseStartedAt));
    }
}
void ABotanicusCashRegisterActor::NotifyCheckoutChanged()
{
    FlushNetDormancy();ForceNetUpdate();OnRep_Checkout();
    if(auto* GM=GetWorld()->GetAuthGameMode<ABotanicusGameMode>()) GM->ScheduleInventoryAutosave();
}
bool ABotanicusCashRegisterActor::TryScanCustomer(ABotanicusPlayerController* PC,ABotanicusVisitorCharacter* Customer)
{
    if(!HasAuthority() || !PC || !Customer || Receipt.Phase!=EBotanicusCheckoutPhase::Idle || !IsOperational() ||
        !DockScanner || DockScanner->GetCarrier()!=PC->GetPawn() || GetCheckoutCustomer()!=Customer) return false;
    FBotanicusCheckoutReceipt Candidate;Candidate.TransactionId=FGuid::NewGuid();
    if(!Customer->CommitManualCheckout(Candidate)) return false;
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();Candidate.Phase=EBotanicusCheckoutPhase::Opening;Candidate.PhaseStartedAt=CheckoutServerTime();
    Candidate.OpenSeconds=FMath::Clamp(S->DrawerOpenSeconds,.1f,1.f);Candidate.CloseSeconds=FMath::Clamp(S->DrawerCloseSeconds,.1f,1.f);Candidate.DepositSeconds=FMath::Clamp(S->CoinDepositSeconds,.1f,.6f);Candidate.ClosePauseSeconds=FMath::Clamp(S->AfterLastCoinPause,0.f,1.f);
    Candidate.CoinFlightSeconds=FMath::Clamp(S->CoinFlight,.1f,.5f);Candidate.VisibleGoldCoins=FMath::Min(Candidate.Amount/10,FMath::Clamp(S->MaximumVisibleGoldCoins,10,256));
    Candidate.VoucherCredits=Candidate.Amount-Candidate.VisibleGoldCoins*10;
    Candidate.CoinIntervalSeconds=FMath::Min(FMath::Clamp(S->CoinInterval,.03f,.2f),FMath::Max(.001f,(FMath::Clamp(S->MaximumCollectionSeconds,1.f,5.f)-Candidate.CoinFlightSeconds)/FMath::Max(1,Candidate.VisualCount()-1)));
    Candidate.CollectionSeconds=Candidate.CoinFlightSeconds+FMath::Max(0,Candidate.VisualCount()-1)*Candidate.CoinIntervalSeconds;
    Candidate.ScanOrigin=DockScanner->GetMuzzleLocation();Candidate.ScanTarget=Customer->GetCheckoutPlantLocation();Candidate.CustomerHand=Customer->GetCheckoutPlantLocation();
    Receipt=Candidate;UpdatePaymentAnchor(Customer);OpeningCustomer=Customer;Customer->RecordManualCheckoutOutcome();++AcceptedScans;NotifyCheckoutChanged();
    UBotanicusAudioWorldSubsystem::Emit(this,TEXT("ScannerBeep"),Receipt.ScanOrigin);UBotanicusAudioWorldSubsystem::Emit(this,TEXT("RegisterOpen"),GetDrawerDepositLocation());return true;
}
bool ABotanicusCashRegisterActor::TryCollectPayment(ABotanicusPlayerController* PC,const FGuid& ExpectedId)
{
    auto* GS=GetWorld()->GetGameState<ABotanicusGameState>();
    if(!HasAuthority() || !PC || !PC->GetPawn() || !GS || ExpectedId!=Receipt.TransactionId || !ExpectedId.IsValid() ||
        Receipt.Phase!=EBotanicusCheckoutPhase::AwaitingCollection || Receipt.bCredited || Receipt.Amount<=0 ||
        static_cast<int64>(GS->GetSharedFunds())+Receipt.Amount>MAX_int32 || static_cast<int64>(GS->GetDailyRevenue())+Receipt.Amount>MAX_int32) return false;
    // Consume before any credit callback. Neither animation completion nor later RPCs can enter again.
    Receipt.bCredited=true;Receipt.Phase=EBotanicusCheckoutPhase::Collecting;Receipt.PhaseStartedAt=CheckoutServerTime();Receipt.CollectedAt=Receipt.PhaseStartedAt;
    PC->CreditPlantSale(Receipt.PlantItemKey,Receipt.Amount,false);++AcceptedCollections;NotifyCheckoutChanged();return true;
}
void ABotanicusCashRegisterActor::TickCheckout(float DeltaSeconds)
{
    if(ActorHasTag(TEXT("BotanicusPlacementPreview"))) return;const double Now=CheckoutServerTime();
    if(HasAuthority())
    {
        if(Receipt.Phase==EBotanicusCheckoutPhase::Opening && Now-Receipt.PhaseStartedAt>=Receipt.OpenSeconds)
        {
            Receipt.Phase=EBotanicusCheckoutPhase::AwaitingCollection;Receipt.PhaseStartedAt=Now;Receipt.OpenedAt=Now;
            UpdatePaymentAnchor(OpeningCustomer.Get());if(auto* Customer=OpeningCustomer.Get()) Customer->BeginDeparture(true);OpeningCustomer.Reset();NotifyCheckoutChanged();
        }
        else if(Receipt.Phase==EBotanicusCheckoutPhase::Collecting && Now-Receipt.CollectedAt>=Receipt.CollectionSeconds+Receipt.ClosePauseSeconds)
        {
            Receipt.Phase=EBotanicusCheckoutPhase::Closing;Receipt.PhaseStartedAt=Now;NotifyCheckoutChanged();UBotanicusAudioWorldSubsystem::Emit(this,TEXT("RegisterClose"),GetDrawerDepositLocation());
        }
        else if(Receipt.Phase==EBotanicusCheckoutPhase::Closing && Now-Receipt.PhaseStartedAt>=Receipt.CloseSeconds)
        {Receipt.Phase=EBotanicusCheckoutPhase::Idle;Receipt.PhaseStartedAt=Now;NotifyCheckoutChanged();}
    }
    if(GetNetMode()==NM_DedicatedServer) return;
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();const bool bAnimate=S->bAnimationsEnabled && (!S->bDisableMotionOnLowQuality || Scalability::GetQualityLevels().EffectsQuality>0);
    float Open=Receipt.Phase==EBotanicusCheckoutPhase::Idle ? 0 : 1;
    if(bAnimate && Receipt.Phase==EBotanicusCheckoutPhase::Opening) Open=FMath::Clamp(static_cast<float>((Now-Receipt.PhaseStartedAt)/Receipt.OpenSeconds),0.f,1.f);
    if(bAnimate && Receipt.Phase==EBotanicusCheckoutPhase::Closing) Open=1-FMath::Clamp(static_cast<float>((Now-Receipt.PhaseStartedAt)/Receipt.CloseSeconds),0.f,1.f);
    Open=Open*Open*(3-2*Open);DrawerVisualRoot->SetRelativeLocation(FMath::Lerp(DrawerClosedTarget->GetRelativeLocation(),DrawerOpenTarget->GetRelativeLocation(),Open));
}
FBotanicusSavedCashRegister ABotanicusCashRegisterActor::CaptureCheckout() const
{
    FBotanicusSavedCashRegister Out;Out.ActorName=GetFName();Out.ActorClass=FSoftClassPath(GetClass());Out.Transform=GetActorTransform();Out.RegisterId=RegisterId;Out.Receipt=Receipt;return Out;
}
void ABotanicusCashRegisterActor::RestoreCheckout(const FBotanicusSavedCashRegister& Saved)
{
    if(!HasAuthority()) return;RegisterId=Saved.RegisterId.IsValid() ? Saved.RegisterId : FGuid::NewGuid();Receipt=Saved.Receipt;OpeningCustomer.Reset();
    if(!Receipt.TransactionId.IsValid() || Receipt.Amount<=0 || Receipt.PlantItemKey.IsNone()) Receipt=FBotanicusCheckoutReceipt();
    else
    {
        const auto* S=GetDefault<UBotanicusCheckoutSettings>();Receipt.VisibleGoldCoins=FMath::Min(Receipt.Amount/10,FMath::Clamp(S->MaximumVisibleGoldCoins,10,256));Receipt.VoucherCredits=Receipt.Amount-Receipt.VisibleGoldCoins*10;
        Receipt.DepositSeconds=FMath::Clamp(Receipt.DepositSeconds,.1f,.6f);Receipt.ClosePauseSeconds=FMath::Clamp(Receipt.ClosePauseSeconds,0.f,1.f);Receipt.OpenSeconds=FMath::Clamp(Receipt.OpenSeconds,.1f,1.f);Receipt.CloseSeconds=FMath::Clamp(Receipt.CloseSeconds,.1f,1.f);
        Receipt.CoinFlightSeconds=FMath::Clamp(Receipt.CoinFlightSeconds,.1f,.5f);Receipt.CoinIntervalSeconds=FMath::Clamp(Receipt.CoinIntervalSeconds,.001f,.2f);
        Receipt.CollectionSeconds=Receipt.CoinFlightSeconds+FMath::Max(0,Receipt.VisualCount()-1)*Receipt.CoinIntervalSeconds;
    }
    if(Receipt.bCredited || Receipt.Phase==EBotanicusCheckoutPhase::Collecting || Receipt.Phase==EBotanicusCheckoutPhase::Closing) {Receipt.bCredited=true;Receipt.Phase=EBotanicusCheckoutPhase::Idle;}
    else if(Receipt.TransactionId.IsValid() && Receipt.Amount>0 && Receipt.Phase!=EBotanicusCheckoutPhase::Idle) {Receipt.Phase=EBotanicusCheckoutPhase::AwaitingCollection;Receipt.OpenedAt=CheckoutServerTime()-Receipt.DepositSeconds-.05;}
    else Receipt=FBotanicusCheckoutReceipt();
    if(Receipt.TransactionId.IsValid() && !Receipt.bHasPaymentAnchor) UpdatePaymentAnchor(nullptr);
    LastBeamTransaction=Receipt.TransactionId;ForceNetUpdate();OnRep_Checkout();
}
bool ABotanicusCashRegisterActor::IsScannerDockHit(const UPrimitiveComponent* Component) const {return Component==ScannerDockHit;}
void ABotanicusCashRegisterActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if(HasAuthority()) {if(DockScanner) DockScanner->Destroy();if(PaymentCoins) PaymentCoins->Destroy();}Super::EndPlay(Reason);
}

void ABotanicusCashRegisterActor::UpdatePaymentAnchor(ABotanicusVisitorCharacter* Customer)
{
    if(!HasAuthority()) return;
    FVector Floor=Receipt.CustomerHand;
    if(IsValid(Customer)) Floor=Customer->GetActorLocation()-FVector(0,0,Customer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    else if(Receipt.bHasPaymentAnchor) return;
    else
    {
        // Legacy receipts persisted the carried plant point, but no customer root.
        FHitResult Ground;FCollisionQueryParams Query(SCENE_QUERY_STAT(CheckoutLegacyFloor),false,this);
        if(GetWorld()->LineTraceSingleByObjectType(Ground,Floor+FVector(0,0,100),Floor-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic),Query)) Floor.Z=Ground.ImpactPoint.Z;
        else Floor.Z-=90.f;
    }
    Floor.Z+=FMath::Clamp(GetDefault<UBotanicusCheckoutSettings>()->CoinFloatHeight,30.f,160.f);
    Receipt.PaymentAnchor=FTransform(GetActorQuat(),Floor);Receipt.bHasPaymentAnchor=true;
}

void ABotanicusCashRegisterActor::ConfigureRegisterRepresentation()
{
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();
    const FTransform Pose(S->RegisterVisualRotation,S->RegisterVisualOffset,FVector(FMath::Clamp(S->RegisterVisualScale,.01f,2.f)));
    Mesh->SetStaticMesh(S->RegisterBodyMesh.LoadSynchronous());
    Mesh->SetRelativeTransform(Pose);
    Mesh->EmptyOverrideMaterials();
    // Keep the inherited main mesh as the correctly sized placement/collision body.
    // Retain obsolete component names for Blueprint compatibility, but no geometry or ghost collision.
    for(UStaticMeshComponent* Obsolete : {CounterVisual.Get(),RegisterVisual.Get(),ScreenVisual.Get(),DrawerLip.Get(),ScannerDockMesh.Get()})
    {
        if(!Obsolete) continue;
        Obsolete->SetStaticMesh(nullptr);Obsolete->EmptyOverrideMaterials();
        Obsolete->SetRelativeTransform(FTransform::Identity);
        Obsolete->SetVisibility(false);Obsolete->SetHiddenInGame(true);
        Obsolete->SetCollisionEnabled(ECollisionEnabled::NoCollision);Obsolete->SetCanEverAffectNavigation(false);
    }
    DrawerMesh->SetStaticMesh(S->RegisterDrawerMesh.LoadSynchronous());
    DrawerMesh->SetRelativeTransform(FTransform::Identity);DrawerMesh->EmptyOverrideMaterials();
    DrawerVisualRoot->SetRelativeTransform(Pose);
    DrawerClosedTarget->SetRelativeLocation(Pose.GetLocation());
    DrawerOpenTarget->SetRelativeLocation(Pose.TransformPosition(S->DrawerOpenOffset));
    DrawerTarget->SetRelativeLocation(S->DrawerInteriorTarget);
    ScannerDock->SetRelativeLocationAndRotation(S->ScannerDockOffset,S->ScannerDockRotation);
    CoinDropPoint->SetRelativeLocation(FVector(-18,-12,5));
}
