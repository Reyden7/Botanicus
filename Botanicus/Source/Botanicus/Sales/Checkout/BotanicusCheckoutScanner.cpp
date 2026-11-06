#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Engine/World.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

ABotanicusCheckoutScanner::ABotanicusCheckoutScanner()
{
    bReplicates=true;bAlwaysRelevant=true;SetReplicateMovement(false);PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    HitBox=CreateDefaultSubobject<UBoxComponent>(TEXT("Scanner Interaction"));SetRootComponent(HitBox);HitBox->SetBoxExtent(FVector(9,5,12));
    HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);HitBox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);HitBox->SetGenerateOverlapEvents(false);HitBox->SetCanEverAffectNavigation(false);
    Handle=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Scanner Handle"));Handle->SetupAttachment(HitBox);Handle->SetRelativeLocation(FVector(0,0,4));Handle->SetRelativeScale3D(FVector(.035f,.035f,.13f));Handle->SetRelativeRotation(FRotator(0,0,-18));Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Head=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Scanner Head"));Head->SetupAttachment(HitBox);Head->SetRelativeLocation(FVector(2,0,11));Head->SetRelativeScale3D(FVector(.11f,.065f,.055f));Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ScannerMuzzle=CreateDefaultSubobject<USceneComponent>(TEXT("ScannerMuzzle"));ScannerMuzzle->SetupAttachment(HitBox);ScannerMuzzle->SetRelativeLocation(FVector(8,0,11));ScannerMuzzle->bEditableWhenInherited=true;
}
void ABotanicusCheckoutScanner::BeginPlay()
{
    Super::BeginPlay();
    if(GetNetMode()!=NM_DedicatedServer)
    {
        auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));Handle->SetStaticMesh(Cube);Head->SetStaticMesh(Cube);
        if(auto* Future=FutureScannerMesh.LoadSynchronous()) {Handle->SetStaticMesh(Future);Handle->SetRelativeTransform(FTransform::Identity);Head->SetVisibility(false);}
        if(auto* Base=GetDefault<UBotanicusCheckoutSettings>()->BodyMaterial.LoadSynchronous())
        {
            auto* Grip=UMaterialInstanceDynamic::Create(Base,this);Grip->SetVectorParameterValue(TEXT("Color"),FLinearColor(.025f,.075f,.07f));Handle->SetMaterial(0,Grip);
            auto* Shell=UMaterialInstanceDynamic::Create(Base,this);Shell->SetVectorParameterValue(TEXT("Color"),FLinearColor(.12f,.65f,.75f));Head->SetMaterial(0,Shell);
        }
    }
    OnRep_Carry();
}
void ABotanicusCheckoutScanner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABotanicusCheckoutScanner,Register);DOREPLIFETIME(ABotanicusCheckoutScanner,Carrier);
}
void ABotanicusCheckoutScanner::InitializeDock(ABotanicusCashRegisterActor* InRegister)
{
    if(!HasAuthority()) return;Register=InRegister;SetOwner(InRegister);OnRep_Carry();ForceNetUpdate();
}
bool ABotanicusCheckoutScanner::TryTake(ABotanicusPlayerController* Controller)
{
    if(!HasAuthority() || IsValid(Carrier) || !IsValid(Register) || !Controller || !Controller->CanTakeCheckoutScanner()) return false;
    Carrier=Controller->GetPawn();OnRep_Carry();ForceNetUpdate();return true;
}
void ABotanicusCheckoutScanner::ReturnToDock()
{
    if(!HasAuthority()) return;
    if(auto* PC=Carrier ? Cast<ABotanicusPlayerController>(Carrier->GetController()) : nullptr) PC->SetCheckoutScannerReference(nullptr);
    Carrier=nullptr;OnRep_Carry();ForceNetUpdate();
}
void ABotanicusCheckoutScanner::OnRep_Carry()
{
    HitBox->SetCollisionEnabled(IsValid(Carrier) ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
    SetActorTickEnabled(IsValid(Carrier));SetActorTickInterval(GetNetMode()==NM_DedicatedServer ? .2f : 0.f);
    if(IsValid(Carrier)) DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    else if(IsValid(Register) && Register->ScannerDock) AttachToComponent(Register->ScannerDock,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    if(GetWorld()) for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It) if(auto* PC=Cast<ABotanicusPlayerController>(It->Get());PC && PC->IsLocalController())
    {if(Carrier==PC->GetPawn()) PC->SetCheckoutScannerReference(this);else if(PC->GetCheckoutScanner()==this) PC->SetCheckoutScannerReference(nullptr);}
    UpdatePose();
}
void ABotanicusCheckoutScanner::UpdatePose()
{
    if(!IsValid(Carrier)) return;
    if(Carrier->IsLocallyControlled())
    {
        if(auto* PC=Cast<APlayerController>(Carrier->GetController()))
        {
            FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);const FRotationMatrix Axes(Rot);
            SetActorLocationAndRotation(Eye+Rot.Vector()*32+Axes.GetUnitAxis(EAxis::Y)*18-Axes.GetUnitAxis(EAxis::Z)*21,Rot);
            if(auto* Bot=Cast<ABotanicusPlayerController>(PC)) Bot->SetCheckoutScannerReference(this);return;
        }
    }
    if(auto* Character=Cast<ABotanicusCharacter>(Carrier);Character && Character->GetMesh() && Character->GetMesh()->DoesSocketExist(TEXT("hand_r")))
    {
        const FTransform Hand=Character->GetMesh()->GetSocketTransform(TEXT("hand_r"));
        SetActorLocationAndRotation(Hand.TransformPosition(FVector(5,2,-4)),Hand.GetRotation()*FRotator(15,-85,15).Quaternion());
    }
    else SetActorLocationAndRotation(Carrier->GetPawnViewLocation()+Carrier->GetBaseAimRotation().Vector()*35-FVector(0,0,20),Carrier->GetBaseAimRotation());
}
void ABotanicusCheckoutScanner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(HasAuthority() && (!IsValid(Carrier) || !IsValid(Carrier->GetController()))) {ReturnToDock();return;}
    if(GetNetMode()!=NM_DedicatedServer) UpdatePose();
}
FVector ABotanicusCheckoutScanner::GetMuzzleLocation() {UpdatePose();return ScannerMuzzle->GetComponentLocation();}
void ABotanicusCheckoutScanner::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* PC=Carrier ? Cast<ABotanicusPlayerController>(Carrier->GetController()) : nullptr) if(PC->GetCheckoutScanner()==this) PC->SetCheckoutScannerReference(nullptr);
    Super::EndPlay(Reason);
}
