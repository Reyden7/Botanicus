#include "Tools/BotanicusMultiToolActor.h"
#include "Presentation/BotanicusHeldItemComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Sales/Checkout/BotanicusCheckoutSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

ABotanicusMultiToolActor::ABotanicusMultiToolActor()
{
    bUseBlueprintAppearance=true;
    ToolVisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("ToolVisualRoot"));ToolVisualRoot->SetupAttachment(Mesh);
    // Follow the existing physics proxy when dropped/thrown, while preserving the old tool sizes.
    ToolVisualRoot->SetRelativeScale3D(FVector::OneVector/Mesh->GetRelativeScale3D());
    MultiToolSkeletalMesh=SkeletalMeshVisual;MultiToolSkeletalMesh->SetupAttachment(ToolVisualRoot);
    CutterMesh_Temporary=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CutterMesh_Temporary"));CutterMesh_Temporary->SetupAttachment(ToolVisualRoot);
    ShovelMesh_Temporary=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShovelMesh_Temporary"));ShovelMesh_Temporary->SetupAttachment(ToolVisualRoot);
    InteractionPoint=CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint"));InteractionPoint->SetupAttachment(ToolVisualRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cutter(TEXT("/Game/Botanicus/Items/itemsMesh/cutter/cutter.cutter"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shovel(TEXT("/Game/Botanicus/Items/itemsMesh/petitePelle/Neon_Trowel_Import0001.Neon_Trowel_Import0001"));
    if(Cutter.Succeeded()){CutterMesh_Temporary->SetStaticMesh(Cutter.Object);Mesh->SetStaticMesh(Cutter.Object);}
    if(Shovel.Succeeded())ShovelMesh_Temporary->SetStaticMesh(Shovel.Object);
    for(auto* Part:{CutterMesh_Temporary.Get(),ShovelMesh_Temporary.Get()})
    {Part->SetRelativeScale3D(FVector(.4f));Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);Part->bEditableWhenInherited=true;}
    ToolVisualRoot->bEditableWhenInherited=true;MultiToolSkeletalMesh->bEditableWhenInherited=true;
    WhipVisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("WhipVisualRoot"));WhipVisualRoot->SetupAttachment(ToolVisualRoot);WhipVisualRoot->SetRelativeLocation(FVector(0,0,8));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    for(int32 I=0;I<5;++I)
    {
        const FName Name(*FString::Printf(TEXT("ElectricWhip_Segment%d"),I));
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(Name);Part->SetupAttachment(WhipVisualRoot);
        Part->SetStaticMesh(Cylinder.Object);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);
        Part->SetRelativeLocation(FVector(-4*I,0,7*I));Part->SetRelativeRotation(FRotator(-30,0,0));
        Part->SetRelativeScale3D(FVector(I==0?.045f:.012f,I==0?.045f:.012f,.075f));WhipSegments.Add(Part);
    }
    HeldItemGrip->GripSettings = {EBotanicusHeldItemProfile::RightHand, FVector(28, 20, 3), FRotator::ZeroRotator, true};
    const FRotator Wrist(-56.43, 148.97, -58.95);
    // Both imported tools point along -X. The whip chain rises along -X/+Z from its handle.
    CutterHeldTransform = FTransform(FRotator(0, 180, 0));
    ShovelHeldTransform = FTransform(FRotator(-12, 180, 0));
    WhipHeldTransform = FTransform(FRotator(45, 180, 0));
    RightHandGrip->SetupAttachment(CutterMesh_Temporary);
    RightHandGrip->SetRelativeLocationAndRotation(FVector(10, 0, 2),
        (CutterHeldTransform.GetRotation().Inverse() * Wrist.Quaternion()).Rotator());
    ShovelRightHandGrip = CreateDefaultSubobject<USceneComponent>(TEXT("ShovelRightHandGrip"));
    ShovelRightHandGrip->SetupAttachment(ShovelMesh_Temporary);
    ShovelRightHandGrip->SetRelativeLocationAndRotation(FVector(10, 0, 1),
        (ShovelHeldTransform.GetRotation().Inverse() * Wrist.Quaternion()).Rotator());
    WhipRightHandGrip = CreateDefaultSubobject<USceneComponent>(TEXT("WhipRightHandGrip"));
    WhipRightHandGrip->SetupAttachment(WhipVisualRoot);
    WhipRightHandGrip->SetRelativeLocationAndRotation(FVector(0, 0, 1),
        (WhipHeldTransform.GetRotation().Inverse() * Wrist.Quaternion()).Rotator());
    RightHandGrip->bEditableWhenInherited = true;
    ShovelRightHandGrip->bEditableWhenInherited = true;
    WhipRightHandGrip->bEditableWhenInherited = true;
    AnimatedRightHandGrip = CreateDefaultSubobject<USceneComponent>(TEXT("AnimatedRightHandGrip"));
    AnimatedRightHandGrip->SetupAttachment(MultiToolSkeletalMesh);
    AnimatedRightHandGrip->SetRelativeLocationAndRotation(FVector(-4, -1, 0), Wrist);
    RefreshVisuals();
}

void ABotanicusMultiToolActor::EnableHeldVisualTransforms()
{
    const auto* Defaults = GetClass()->GetDefaultObject<ABotanicusMultiToolActor>();
    CutterMesh_Temporary->SetRelativeTransform(CutterHeldTransform * Defaults->CutterMesh_Temporary->GetRelativeTransform());
    ShovelMesh_Temporary->SetRelativeTransform(ShovelHeldTransform * Defaults->ShovelMesh_Temporary->GetRelativeTransform());
    WhipVisualRoot->SetRelativeTransform(WhipHeldTransform * Defaults->WhipVisualRoot->GetRelativeTransform());
}

void ABotanicusMultiToolActor::EquipForRobot(ABotanicusCharacter* Carrier, EBotanicusHeldItemSource Source,
    const FBotanicusHeldItemSettings& Catalogue)
{
    UPrimitiveComponent* Geometry = CutterMesh_Temporary;
    USceneComponent* Grip = RightHandGrip;
    if (UsesSkeletalMeshAppearance() && MultiToolSkeletalMesh->GetSkeletalMeshAsset())
    {
        Geometry = MultiToolSkeletalMesh;
        Grip = AnimatedRightHandGrip;
    }
    else if (Mode == EBotanicusMultiToolMode::Shovel)
    {
        Geometry = ShovelMesh_Temporary;
        Grip = ShovelRightHandGrip;
    }
    else if (Mode == EBotanicusMultiToolMode::ElectricWhip && !WhipSegments.IsEmpty())
    {
        Geometry = WhipSegments[0];
        Grip = WhipRightHandGrip;
    }
    FBotanicusHeldItemSettings Settings = HeldItemGrip->ResolveSettings(Catalogue);
    if (Grip == RightHandGrip)
        Settings.CarryRelativeLocation += CutterHeldTransform.GetLocation();
    else if (Grip == ShovelRightHandGrip)
        Settings.CarryRelativeLocation += ShovelHeldTransform.GetLocation();
    else if (Grip == WhipRightHandGrip)
        Settings.CarryRelativeLocation += WhipHeldTransform.GetLocation();
    HeldItemGrip->Bind(Carrier, *SceneRoot, *Geometry, LeftHandGrip, Grip, Source, Settings);
}

void ABotanicusMultiToolActor::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);RefreshVisuals();}
void ABotanicusMultiToolActor::ApplyItemDefinition(){Super::ApplyItemDefinition();RefreshVisuals();}
void ABotanicusMultiToolActor::SetMultiToolMode(EBotanicusMultiToolMode NewMode)
{
    if(Mode==NewMode)return;
    const auto Previous=Mode;Mode=NewMode;RefreshVisuals();
    if(Previous!=Mode)OnMultiToolModeChanged(Previous,Mode);
}
void ABotanicusMultiToolActor::RefreshVisuals()
{
    if(!CutterMesh_Temporary || !ShovelMesh_Temporary || !MultiToolSkeletalMesh)return;
    // Retain the existing item collision proxy; it is not a third visible tool.
    Mesh->SetVisibility(false);Mesh->SetHiddenInGame(true);
    const bool bAnimated=bUseSkeletalMeshAppearance && MultiToolSkeletalMesh->GetSkeletalMeshAsset();
    MultiToolSkeletalMesh->SetVisibility(bAnimated);MultiToolSkeletalMesh->SetHiddenInGame(!bAnimated);
    // Neutral uses the existing cutter as the resting silhouette, including world pickup/drop.
    const bool bWhip=!bAnimated && Mode==EBotanicusMultiToolMode::ElectricWhip;
    if(WhipVisualRoot)
    {
        WhipVisualRoot->SetVisibility(bWhip,true);WhipVisualRoot->SetHiddenInGame(!bWhip,true);
        // Existing lightweight cyan hologram; no new mesh, material, or skeleton asset.
        if(GetWorld() && GetWorld()->IsGameWorld() && bWhip && !WhipSegments.IsEmpty() && !Cast<UMaterialInstanceDynamic>(WhipSegments[0]->GetMaterial(0)))
            if(auto* Mat=GetDefault<UBotanicusCheckoutSettings>()->ScanHologramMaterial.LoadSynchronous())
            {
                auto* MID=UMaterialInstanceDynamic::Create(Mat,this);
                MID->SetVectorParameterValue(TEXT("BodyTint"),FLinearColor(0,.75f,1));MID->SetVectorParameterValue(TEXT("RimTint"),FLinearColor(0,1,1));
                MID->SetScalarParameterValue(TEXT("Opacity"),.8f);MID->SetScalarParameterValue(TEXT("Emission"),1);
                for(auto& Part:WhipSegments)Part->SetMaterial(0,MID);
            }
    }
    const bool bShovel=!bAnimated && Mode==EBotanicusMultiToolMode::Shovel;
    ShovelMesh_Temporary->SetVisibility(bShovel);ShovelMesh_Temporary->SetHiddenInGame(!bShovel);
    CutterMesh_Temporary->SetVisibility(!bAnimated && !bShovel && !bWhip);CutterMesh_Temporary->SetHiddenInGame(bAnimated || bShovel || bWhip);
}

float ABotanicusMultiToolActor::GetHeldVisualRadius() const
{
    if(Mode==EBotanicusMultiToolMode::ElectricWhip)return 28;
    if(bUseSkeletalMeshAppearance && MultiToolSkeletalMesh->GetSkeletalMeshAsset())
        return (MultiToolSkeletalMesh->GetSkeletalMeshAsset()->GetBounds().BoxExtent*MultiToolSkeletalMesh->GetComponentScale().GetAbs()).GetMax();
    const auto* Part=Mode==EBotanicusMultiToolMode::Shovel?ShovelMesh_Temporary.Get():CutterMesh_Temporary.Get();
    return Part && Part->GetStaticMesh()?(Part->GetStaticMesh()->GetBounds().BoxExtent*Part->GetComponentScale().GetAbs()).GetMax():20;
}

void ABotanicusMultiToolActor::PlayWhipSwing()
{
    if(!GetWorld() || Mode!=EBotanicusMultiToolMode::ElectricWhip)return;
    SwingStarted=GetWorld()->GetTimeSeconds()-.11f; // Contact pose, followed by cosmetic recoil.
    GetWorld()->GetTimerManager().SetTimer(SwingTimer,this,&ThisClass::UpdateWhipSwing,.016f,true);
    UpdateWhipSwing();
}
void ABotanicusMultiToolActor::UpdateWhipSwing()
{
    const float A=FMath::Clamp(float((GetWorld()->GetTimeSeconds()-SwingStarted)/.22f),0.f,1.f);
    const bool bEnd=A>=1 || Mode!=EBotanicusMultiToolMode::ElectricWhip;
    for(int32 I=0;I<WhipSegments.Num();++I)
    {
        const float Bend=bEnd?0:22*FMath::Sin(A*2*PI-I*.35f)*FMath::Sin(A*PI);
        WhipSegments[I]->SetRelativeLocation(FVector(-4*I,Bend*.12f*I,7*I));
        WhipSegments[I]->SetRelativeRotation(FRotator(-30,Bend,0));
    }
    if(bEnd){GetWorld()->GetTimerManager().ClearTimer(SwingTimer);SwingStarted=-1;}
}
void ABotanicusMultiToolActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(SwingTimer);
    Super::EndPlay(Reason);
}
