#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "Engine/World.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
    DockSlot=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Scanner Dock Slot"));DockSlot->SetupAttachment(HitBox);
    DockSlot->SetCollisionEnabled(ECollisionEnabled::NoCollision);DockSlot->SetGenerateOverlapEvents(false);DockSlot->SetCanEverAffectNavigation(false);DockSlot->SetCastShadow(false);
    DockSlot->SetVisibility(false);DockSlot->SetHiddenInGame(true);DockSlot->SetComponentTickEnabled(false);
    ScannerMuzzle=CreateDefaultSubobject<USceneComponent>(TEXT("ScannerMuzzle"));ScannerMuzzle->SetupAttachment(HitBox);ScannerMuzzle->SetRelativeLocation(FVector(8,0,11));ScannerMuzzle->bEditableWhenInherited=true;
}
void ABotanicusCheckoutScanner::BeginPlay()
{
    Super::BeginPlay();
    ConfigureModel();
    ConfigureDockSlot();
    OnRep_Carry();
}
void ABotanicusCheckoutScanner::ConfigureModel()
{
    const auto* Settings=GetDefault<UBotanicusCheckoutSettings>();
    const float Scale=FMath::Clamp(Settings->ScannerMeshScale,.05f,2.f);
    const FQuat Rotation=Settings->ScannerMeshRotation.Quaternion();
    Handle->SetRelativeTransform(FTransform(Rotation,-Rotation.RotateVector(Settings->ScannerGripPoint*Scale),FVector(Scale)));
    ScannerMuzzle->AttachToComponent(Handle,FAttachmentTransformRules::KeepRelativeTransform);
    ScannerMuzzle->SetRelativeLocationAndRotation(Settings->ScannerReadHeadPoint,FRotator(0,180,0));
    auto* Asset=Settings->ScannerMesh.LoadSynchronous();if(!Asset)Asset=FutureScannerMesh.LoadSynchronous();
    if(Asset)
    {
        const FBox Box=Asset->GetBoundingBox().TransformBy(Handle->GetRelativeTransform());
        HitBox->SetBoxExtent(Box.Min.GetAbs().ComponentMax(Box.Max.GetAbs())+FVector(1));
    }
    if(GetNetMode()==NM_DedicatedServer)return;
    Handle->SetStaticMesh(Asset);Handle->EmptyOverrideMaterials();Head->SetStaticMesh(nullptr);Head->SetVisibility(false);
}
void ABotanicusCheckoutScanner::ConfigureDockSlot()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    const auto* S=GetDefault<UBotanicusCheckoutSettings>();
    auto* Material=S->ScanHologramMaterial.LoadSynchronous();if(!Material)return;
    DockSlot->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    auto* Tint=UMaterialInstanceDynamic::Create(Material,this);Tint->SetVectorParameterValue(TEXT("BodyTint"),S->ScannerDockSlotColor);
    Tint->SetVectorParameterValue(TEXT("RimTint"),S->ScannerDockSlotColor);Tint->SetScalarParameterValue(TEXT("Opacity"),.55f);
    Tint->SetScalarParameterValue(TEXT("Emission"),.7f);Tint->SetScalarParameterValue(TEXT("FresnelIntensity"),.65f);DockSlot->SetMaterial(0,Tint);
    const FVector2D Size(FMath::Clamp(S->ScannerDockSlotSize.X,6.f,30.f),FMath::Clamp(S->ScannerDockSlotSize.Y,6.f,30.f));
    constexpr float Width=.6f,Height=.3f;
    for(float Sign:{-1.f,1.f})
    {
        DockSlot->AddInstance(FTransform(FQuat::Identity,FVector(0,Sign*(Size.Y-Width)*.5f,0),FVector(Size.X,Width,Height)/100.f));
        DockSlot->AddInstance(FTransform(FQuat::Identity,FVector(Sign*(Size.X-Width)*.5f,0,0),FVector(Width,Size.Y-2*Width,Height)/100.f));
    }
}
void ABotanicusCheckoutScanner::RefreshDockSlot()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    const bool bValidDock=IsValid(Register) && IsValid(Register->ScannerDock);
    if(bValidDock && DockSlot->GetAttachParent()!=Register->ScannerDock)
    {
        DockSlot->AttachToComponent(Register->ScannerDock,FAttachmentTransformRules::KeepRelativeTransform);
        DockSlot->SetRelativeTransform(FTransform(GetDefault<UBotanicusCheckoutSettings>()->ScannerDockSlotOffset));
    }
    const bool bShow=bValidDock && IsValid(Carrier) && !Register->IsHidden() && !IsHidden() &&
        GetDefault<UBotanicusCheckoutSettings>()->bShowScannerDockSlot && DockSlot->GetStaticMesh() && DockSlot->GetNumInstances()==4;
    if(DockSlot->IsVisible()!=bShow){DockSlot->SetVisibility(bShow);DockSlot->SetHiddenInGame(!bShow);}
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
    else if(IsValid(Register) && Register->ScannerDock)
    {AttachToComponent(Register->ScannerDock,FAttachmentTransformRules::SnapToTargetNotIncludingScale);const auto* S=GetDefault<UBotanicusCheckoutSettings>();SetActorRelativeLocation(S->ScannerDockPoseOffset);SetActorRelativeRotation(S->ScannerDockPoseRotation);}
    if(GetWorld()) for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It) if(auto* PC=Cast<ABotanicusPlayerController>(It->Get());PC && PC->IsLocalController())
    {if(Carrier==PC->GetPawn()) PC->SetCheckoutScannerReference(this);else if(PC->GetCheckoutScanner()==this) PC->SetCheckoutScannerReference(nullptr);}
    UpdatePose();
    RefreshDockSlot();
}
void ABotanicusCheckoutScanner::UpdatePose()
{
    if(!IsValid(Carrier)) return;
    if(Carrier->IsLocallyControlled())
    {
        if(auto* PC=Cast<APlayerController>(Carrier->GetController()))
        {
            FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);const FRotationMatrix Axes(Rot);
            const auto* S=GetDefault<UBotanicusCheckoutSettings>();
            SetActorLocationAndRotation(Eye+Axes.TransformVector(S->ScannerFirstPersonOffset),(Rot.Quaternion()*S->ScannerFirstPersonRotation.Quaternion()).Rotator());
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
    if(GetNetMode()!=NM_DedicatedServer){UpdatePose();RefreshDockSlot();}
}
FVector ABotanicusCheckoutScanner::GetMuzzleLocation() {UpdatePose();return ScannerMuzzle->GetComponentLocation();}
void ABotanicusCheckoutScanner::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* PC=Carrier ? Cast<ABotanicusPlayerController>(Carrier->GetController()) : nullptr) if(PC->GetCheckoutScanner()==this) PC->SetCheckoutScannerReference(nullptr);
    if(DockSlot)DockSlot->DestroyComponent();
    Super::EndPlay(Reason);
}
