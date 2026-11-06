#include "Sales/Checkout/BotanicusPaymentCoins.h"
#include "Engine/World.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Scalability.h"

ABotanicusPaymentCoins::ABotanicusPaymentCoins()
{
    bReplicates=true;bAlwaysRelevant=true;SetReplicateMovement(false);PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    HitBox=CreateDefaultSubobject<UBoxComponent>(TEXT("Payment Interaction"));SetRootComponent(HitBox);HitBox->SetBoxExtent(FVector(15,17,5));
    HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);HitBox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);HitBox->SetGenerateOverlapEvents(false);HitBox->SetCanEverAffectNavigation(false);
    GoldCoins=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ten Credit Coins"));GoldCoins->SetupAttachment(HitBox);GoldCoins->SetCollisionEnabled(ECollisionEnabled::NoCollision);GoldCoins->SetCanEverAffectNavigation(false);
    CreditVoucher=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Exact Credit Voucher"));CreditVoucher->SetupAttachment(HitBox);CreditVoucher->SetCollisionEnabled(ECollisionEnabled::NoCollision);CreditVoucher->SetVisibility(false);
    VoucherLabel=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Voucher Denomination"));VoucherLabel->SetupAttachment(CreditVoucher);VoucherLabel->SetHorizontalAlignment(EHTA_Center);VoucherLabel->SetWorldSize(2);VoucherLabel->SetRelativeLocation(FVector(-1,0,1));VoucherLabel->SetRelativeRotation(FRotator(0,180,0));VoucherLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);VoucherLabel->SetVisibility(false);
}
void ABotanicusPaymentCoins::BeginPlay()
{
    Super::BeginPlay();
    if(GetNetMode()!=NM_DedicatedServer)
    {
        GoldCoins->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        CreditVoucher->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        if(auto* Base=GetDefault<UBotanicusCheckoutSettings>()->BodyMaterial.LoadSynchronous())
        {
            auto* Gold=UMaterialInstanceDynamic::Create(Base,this);Gold->SetVectorParameterValue(TEXT("Color"),FLinearColor(.85f,.48f,.08f));GoldCoins->SetMaterial(0,Gold);
            auto* Paper=UMaterialInstanceDynamic::Create(Base,this);Paper->SetVectorParameterValue(TEXT("Color"),FLinearColor(.09f,.7f,.55f));CreditVoucher->SetMaterial(0,Paper);
        }
    }
    OnRep_Register();
}
void ABotanicusPaymentCoins::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABotanicusPaymentCoins,Register);}
void ABotanicusPaymentCoins::InitializeRegister(ABotanicusCashRegisterActor* InRegister) {if(HasAuthority()) {Register=InRegister;SetOwner(InRegister);OnRep_Register();ForceNetUpdate();}}
void ABotanicusPaymentCoins::OnRep_Register() {if(Register && Register->CoinDropPoint) AttachToComponent(Register->CoinDropPoint,FAttachmentTransformRules::SnapToTargetNotIncludingScale);RefreshFromReceipt();}
int32 ABotanicusPaymentCoins::GetRenderedCoinCount() const {return GoldCoins->GetInstanceCount();}
FVector ABotanicusPaymentCoins::CounterPosition(int32 Index) const
{
    const float Size=FMath::Clamp(GetDefault<UBotanicusCheckoutSettings>()->CoinVisualScale,.25f,4.f);
    const FVector Local=Register && Index>=Register->GetReceipt().VisibleGoldCoins ? FVector(0,12,.6f) : FVector((Index%6-2.5f)*3.5f,((Index/6)%4-1.5f)*3.5f,(Index/24)*.5f+.4f);
    return GetActorTransform().TransformPosition(Local*Size);
}
bool ABotanicusPaymentCoins::HasFloatingMotion() const
{
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();
    return S->bAnimationsEnabled && (!S->bDisableMotionOnLowQuality || Scalability::GetQualityLevels().EffectsQuality>0) && (S->CoinFloatAmplitude>0 || S->CoinFloatRotationSpeed>0);
}
FVector ABotanicusPaymentCoins::FloatingPosition(int32 Index,double Time) const
{
    FVector Position=CounterPosition(Index);const auto* S=GetDefault<UBotanicusCheckoutSettings>();
    if(HasFloatingMotion()) Position.Z+=FMath::Clamp(S->CoinFloatAmplitude,0.f,5.f)*FMath::Sin(2*PI*(Time-Register->GetReceipt().OpenedAt)/FMath::Max(.5f,S->CoinFloatPeriod)+Index*.35f);
    return Position;
}
FVector ABotanicusPaymentCoins::DepositPosition(int32 Index,double Time) const
{
    const auto& R=Register->GetReceipt();const float U=FMath::Clamp(static_cast<float>((Time-R.OpenedAt)/R.DepositSeconds),0.f,1.f);
    const FVector End=FloatingPosition(Index,Time);return FMath::Lerp(R.CustomerHand,End,U*U*(3-2*U))+FVector(0,0,FMath::Sin(PI*U)*FMath::Clamp(GetDefault<UBotanicusCheckoutSettings>()->CoinDepositArcHeight,0.f,15.f));
}
void ABotanicusPaymentCoins::RefreshFromReceipt()
{
    if(!Register) return;const auto& R=Register->GetReceipt();
    if(R.bHasPaymentAnchor) {DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);SetActorTransform(R.PaymentAnchor);}
    const float Size=FMath::Clamp(GetDefault<UBotanicusCheckoutSettings>()->CoinVisualScale,.25f,4.f);
    HitBox->SetBoxExtent(FVector(12*Size+1,14*Size+1,((R.VisibleGoldCoins/24)*.5f+1)*Size+FMath::Clamp(GetDefault<UBotanicusCheckoutSettings>()->CoinFloatAmplitude,0.f,5.f)+2),false);
    HitBox->SetCollisionEnabled(R.Phase==EBotanicusCheckoutPhase::AwaitingCollection && !R.bCredited ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    const bool bNew=R.TransactionId!=LastTransaction;
    if(bNew || LastPhase!=R.Phase)
    {
        bCollectionAudio=!bNew && R.Phase==EBotanicusCheckoutPhase::Collecting;
        LastTransaction=R.TransactionId;LastPhase=R.Phase;
        if(GetNetMode()!=NM_DedicatedServer)
        {
            GoldCoins->ClearInstances();ArrivalPlayed.Init(false,R.VisualCount());
            const bool bShow=R.Phase==EBotanicusCheckoutPhase::AwaitingCollection || R.Phase==EBotanicusCheckoutPhase::Collecting;
            if(bShow) for(int32 I=0;I<R.VisibleGoldCoins;++I) GoldCoins->AddInstance(FTransform(FRotator::ZeroRotator,CounterPosition(I),FVector(.03f,.03f,.004f)*Size),true);
            CreditVoucher->SetVisibility(bShow && R.VoucherCredits>0);VoucherLabel->SetVisibility(bShow && R.VoucherCredits>0);
            VoucherLabel->SetText(FText::FromString(FString::Printf(TEXT("%d cr"),R.VoucherCredits)));
        }
    }
    const bool bAnimate=R.Phase==EBotanicusCheckoutPhase::Collecting || (R.Phase==EBotanicusCheckoutPhase::AwaitingCollection && (Register->CheckoutServerTime()<R.OpenedAt+R.DepositSeconds || HasFloatingMotion()));
    SetActorTickInterval(0);SetActorTickEnabled(bAnimate && GetNetMode()!=NM_DedicatedServer);UpdatePresentation();
}
void ABotanicusPaymentCoins::UpdatePresentation()
{
    if(!Register || GetNetMode()==NM_DedicatedServer) return;const auto& R=Register->GetReceipt();const double Now=Register->CheckoutServerTime();
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();const float VisualScale=FMath::Clamp(S->CoinVisualScale,.25f,4.f);const bool bMotion=S->bAnimationsEnabled && (!S->bDisableMotionOnLowQuality || Scalability::GetQualityLevels().EffectsQuality>0);
    if(R.Phase!=EBotanicusCheckoutPhase::AwaitingCollection && R.Phase!=EBotanicusCheckoutPhase::Collecting) return;
    for(int32 I=0;I<R.VisualCount();++I)
    {
        FVector Position=bMotion ? DepositPosition(I,Now) : CounterPosition(I);float Scale=1;
        const double PoseTime=R.Phase==EBotanicusCheckoutPhase::Collecting ? R.CollectedAt : Now;
        FRotator Rotation(0,I*47.f+(HasFloatingMotion() ? FMath::Fmod((PoseTime-R.OpenedAt)*FMath::Clamp(S->CoinFloatRotationSpeed,0.f,30.f),360.) : 0),0);
        if(R.Phase==EBotanicusCheckoutPhase::Collecting)
        {
            const double Departure=R.CollectedAt+I*R.CoinIntervalSeconds;
            const float U=FMath::Clamp(static_cast<float>((Now-Departure)/R.CoinFlightSeconds),0.f,1.f);
            const FVector Start=DepositPosition(I,R.CollectedAt);
            Position=FMath::Lerp(Start,Register->GetDrawerDepositLocation(),U*U*(3-2*U))+FVector(0,0,FMath::Sin(PI*U)*S->CoinArcHeight);
            Rotation.Roll=U*FMath::Clamp(S->CoinRotationDegrees,0.f,90.f);Scale=bMotion && Now<Departure+R.CoinFlightSeconds ? 1.f : 0.f;
            if(U>=1 && ArrivalPlayed.IsValidIndex(I) && !ArrivalPlayed[I])
            {
                ArrivalPlayed[I]=true;++Arrivals;
                if(bCollectionAudio) if(auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>()) Audio->PlayCheckoutContact(TEXT("CoinPickup"),Position,FMath::FRandRange(.97f,1.03f));
            }
        }
        if(I<R.VisibleGoldCoins) GoldCoins->UpdateInstanceTransform(I,FTransform(Rotation,Position,FVector(.03f,.03f,.004f)*Scale*VisualScale),true,false,true);
        else {CreditVoucher->SetWorldTransform(FTransform(Rotation,Position,FVector(.055f,.035f,.006f)*Scale));VoucherLabel->SetWorldLocation(Position+FVector(0,0,3));VoucherLabel->SetWorldRotation(GetActorRotation()+FRotator(0,180,0));VoucherLabel->SetWorldScale3D(FVector(.7f*Scale));}
    }
    GoldCoins->MarkRenderStateDirty();
    if(R.Phase==EBotanicusCheckoutPhase::AwaitingCollection && Now>=R.OpenedAt+R.DepositSeconds) {SetActorTickInterval(1.f/30.f);if(!HasFloatingMotion()) SetActorTickEnabled(false);}
    if(R.Phase==EBotanicusCheckoutPhase::Collecting && Now>=R.CollectedAt+R.CollectionSeconds) SetActorTickEnabled(false);
}
void ABotanicusPaymentCoins::Tick(float DeltaSeconds) {Super::Tick(DeltaSeconds);UpdatePresentation();}
