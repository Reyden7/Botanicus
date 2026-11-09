#include "Growing/BotanicusWaterReserveActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "BotanicusPlayerController.h"
#include "BotanicusGameMode.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Visuals/BotanicusJuiceSettings.h"
#include "Visuals/BotanicusPlacementJuiceSettings.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusWaterReserveActor::ABotanicusWaterReserveActor()
{
    ReserveText=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Reserve Label"));ReserveText->SetupAttachment(SceneRoot);
    ReserveText->SetVisibility(false);ReserveText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FillZone=CreateDefaultSubobject<UBoxComponent>(TEXT("Refill Zone"));FillZone->SetupAttachment(SceneRoot);
    FillZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);FillZone->SetCanEverAffectNavigation(false);
    FaucetOutlet=CreateDefaultSubobject<USceneComponent>(TEXT("FaucetOutlet"));FaucetOutlet->SetupAttachment(SceneRoot);
    FaucetOutlet->SetRelativeLocation(FVector(95,0,130));FaucetOutlet->bEditableWhenInherited=true;
    FaucetInteraction=CreateDefaultSubobject<UBoxComponent>(TEXT("Faucet Interaction"));FaucetInteraction->SetupAttachment(FaucetOutlet);
    FaucetInteraction->SetBoxExtent(FVector(10));FaucetInteraction->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FaucetInteraction->SetCollisionResponseToAllChannels(ECR_Ignore);FaucetInteraction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    FaucetInteraction->SetGenerateOverlapEvents(false);FaucetInteraction->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Visual=[&](FName Name,UStaticMesh* Asset)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(FillZone);C->SetStaticMesh(Asset);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetCastShadow(false);C->SetVisibility(false);return C;
    };
    ZoneFloor=Visual(TEXT("Refill Area"),Plane.Object);
    for(int32 I=0;I<4;++I)ZoneBorder.Add(Visual(*FString::Printf(TEXT("Refill Border %d"),I),Cube.Object));
    Stream=CreateDefaultSubobject<UNiagaraComponent>(TEXT("Refill Stream"));Stream->SetupAttachment(FaucetOutlet);
    Splash=CreateDefaultSubobject<UNiagaraComponent>(TEXT("Refill Splash"));Splash->SetupAttachment(SceneRoot);
    for(auto* FX:{Stream.Get(),Splash.Get()}){FX->bAutoActivate=false;FX->SetAutoDestroy(false);FX->SetCastShadow(false);FX->SetCanEverAffectNavigation(false);}
}
void ABotanicusWaterReserveActor::BuildZone()
{
    ZoneHalfSize=ZoneHalfSize.ComponentMax(FVector(10));
    float Bottom=0;
    if(Mesh && Mesh->GetStaticMesh())Bottom=Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetRelativeTransform()).Min.Z;
    FVector Offset=ZoneOffset;
    FRotator ZoneRotation=FRotator::ZeroRotator;
    if(Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName()==TEXT("/Game/Botanicus/Items/itemsMesh/ReservoirEau/cuve_d_eau.cuve_d_eau"))
    {
        // Calibrated on the actual spout, not ActorForwardVector (the old blockout used +X).
        // Transform through the visual mesh so authored mesh rotation/translation is respected.
        Offset=Mesh->GetRelativeTransform().TransformPosition(FVector(FillZoneLocalOffset.X,FillZoneLocalOffset.Y,0));
        Offset.Z=FillZoneLocalOffset.Z;
        ZoneRotation.Yaw=Mesh->GetRelativeRotation().Yaw;
        FVector Outlet=FaucetOutlet->GetRelativeLocation();
        Outlet.X=Offset.X;Outlet.Y=Offset.Y; // Keep the existing outlet height and flow behaviour.
        FaucetOutlet->SetRelativeLocation(Outlet);
    }
    FillZone->SetRelativeLocation(FVector(Offset.X,Offset.Y,Bottom+Offset.Z+ZoneHalfSize.Z));
    FillZone->SetRelativeRotation(ZoneRotation);FillZone->SetBoxExtent(ZoneHalfSize);
    ZoneFloor->SetRelativeLocation(FVector(0,0,-ZoneHalfSize.Z+1));ZoneFloor->SetRelativeScale3D(FVector(ZoneHalfSize.X/50,ZoneHalfSize.Y/50,1));
    for(int32 I=0;I<4;++I)
    {
        const bool X=I<2;const float Side=I%2 ? 1.f:-1.f;
        ZoneBorder[I]->SetRelativeLocation(FVector(X ? Side*ZoneHalfSize.X:0,X ? 0:Side*ZoneHalfSize.Y,-ZoneHalfSize.Z+1.2f));
        ZoneBorder[I]->SetRelativeScale3D(FVector(X ? .012f:ZoneHalfSize.X/50,X ? ZoneHalfSize.Y/50:.012f,.006f));
    }
    if(ReserveText)ReserveText->SetVisibility(false);
}
void ABotanicusWaterReserveActor::ApplyItemDefinition()
{
    Super::ApplyItemDefinition();
    if(FillZone)BuildZone();
}
void ABotanicusWaterReserveActor::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);BuildZone();}
bool ABotanicusWaterReserveActor::IsRealStation() const
{
    return GetIsReplicated() && !ActorHasTag(TEXT("BotanicusPlacementPreview")) && !ActorHasTag(TEXT("BotanicusEquippedPreview"));
}
void ABotanicusWaterReserveActor::BeginPlay()
{
    Super::BeginPlay();BuildZone();if(!IsRealStation())return;
    if(GetNetMode()!=NM_DedicatedServer)
    {
        const auto* S=GetDefault<UBotanicusJuiceSettings>();Stream->SetAsset(S->WaterSystem.LoadSynchronous());Splash->SetAsset(S->BurstSystem.LoadSynchronous());
        if(auto* M=GetDefault<UBotanicusPlacementJuiceSettings>()->HologramMaterial.LoadSynchronous())
        {
            ZoneMaterial=UMaterialInstanceDynamic::Create(M,this);BorderMaterial=UMaterialInstanceDynamic::Create(M,this);
            for(auto* MID:{ZoneMaterial.Get(),BorderMaterial.Get()})
            {
                MID->SetVectorParameterValue(TEXT("BodyTint"),FLinearColor(.07f,.7f,.2f));MID->SetVectorParameterValue(TEXT("RimTint"),FLinearColor(.2f,.9f,.35f));
                MID->SetScalarParameterValue(TEXT("FresnelIntensity"),.15f);MID->SetScalarParameterValue(TEXT("Emission"),.65f);
            }
            ZoneMaterial->SetScalarParameterValue(TEXT("Opacity"),.16f);BorderMaterial->SetScalarParameterValue(TEXT("Opacity"),.55f);
            ZoneFloor->SetMaterial(0,ZoneMaterial);for(UStaticMeshComponent* C:ZoneBorder)C->SetMaterial(0,BorderMaterial);
        }
    }
    LastTransferTime=GetWorld()->GetTimeSeconds();GetWorldTimerManager().SetTimer(StationTimer,this,&ThisClass::UpdateStation,.1f,true);
    OnRep_Station();
}
FVector ABotanicusWaterReserveActor::GetDockGroundLocation() const
{
    return FillZone->GetComponentTransform().TransformPosition(FVector(0,0,-ZoneHalfSize.Z));
}
bool ABotanicusWaterReserveActor::ContainsCan(const ABotanicusWateringCanActor* Can) const
{
    if(!IsValid(Can) || !Can->GetIsReplicated() || Can->GetCarrier() || Can->GetAttachParentActor() || !Can->GetActorEnableCollision())return false;
    if(Can->ActorHasTag(TEXT("BotanicusPlacementPreview")) || Can->ActorHasTag(TEXT("BotanicusEquippedPreview")))return false;
    const FVector P=FillZone->GetComponentTransform().InverseTransformPosition(Can->GetActorLocation());
    return FMath::Abs(P.X)<=ZoneHalfSize.X && FMath::Abs(P.Y)<=ZoneHalfSize.Y && FMath::Abs(P.Z)<=ZoneHalfSize.Z;
}
bool ABotanicusWaterReserveActor::AcceptPlacement(ABotanicusWateringCanActor* Can)
{
    if(!IsValid(Can) || !Can->HasAuthority())return false;
    ABotanicusWaterReserveActor* Best=nullptr;double Distance=MAX_dbl;
    for(TActorIterator<ABotanicusWaterReserveActor> It(Can->GetWorld());It;++It)
    {
        if(!It->IsRealStation() || It->State.Can || !It->ContainsCan(Can))continue;
        const double D=FVector::DistSquared(It->GetDockGroundLocation(),Can->GetActorLocation());if(D<Distance){Best=*It;Distance=D;}
    }
    return Best && Best->TryDock(Can,true);
}
bool ABotanicusWaterReserveActor::TryDock(ABotanicusWateringCanActor* Can,bool bSnap)
{
    if(!HasAuthority() || !IsRealStation() || State.Can || !ContainsCan(Can) || Can->GetRefillStation())return false;
    if(bSnap)
    {
        // Preserve the existing placement pivot and world floor height. Only centre an already validated placement inside this slot.
        FVector P=GetDockGroundLocation();P.Z=Can->GetActorLocation().Z;
        // Centring under the faucet must preserve the yaw chosen in the hologram.
        const FRotator Rotation(0,Can->GetActorRotation().Yaw,0);
        bool Clear=false;
        if(Can->Mesh && Can->Mesh->GetStaticMesh())
        {
            const FTransform Pose(Rotation,P,Can->GetActorScale3D());
            const FBox Bounds=Can->Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Can->Mesh->GetRelativeTransform()*Pose);
            FCollisionQueryParams Q(SCENE_QUERY_STAT(RefillSafeAlignment),false,Can);
            // Do not carry a validated edge placement onto a different floor height.
            FHitResult Floor;
            const FVector Foot=FVector(P.X,P.Y,Bounds.Min.Z);
            const bool SameFloor=GetWorld()->LineTraceSingleByChannel(Floor,Foot+FVector(0,0,20),Foot-FVector(0,0,20),ECC_Visibility,Q) &&
                Floor.ImpactNormal.Z>=.7f && FMath::Abs(Foot.Z-Floor.ImpactPoint.Z-3.f)<=4.f;
            Clear=SameFloor && !GetWorld()->OverlapBlockingTestByChannel(Bounds.GetCenter(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeBox((Bounds.GetExtent()-FVector(.5f,.5f,2.f)).ComponentMax(FVector(1))),Q);
        }
        // An occupied canonical pose never overrides the validated placement; the stream follows the actual fill target instead.
        if(Clear){Can->SetActorLocationAndRotation(P,Rotation,false,nullptr,ETeleportType::TeleportPhysics);Can->ForceNetUpdate();}
    }
    State.Can=Can;Can->SetRefillStation(this);
    CanTransformHandle=Can->GetRootComponent()->TransformUpdated.AddUObject(this,&ThisClass::CanTransformUpdated);
    ForceNetUpdate();OnRep_Station();return true;
}
void ABotanicusWaterReserveActor::CanTransformUpdated(USceneComponent*,EUpdateTransformFlags,ETeleportType)
{
    if(HasAuthority() && State.Can && !ContainsCan(State.Can))ReleaseCan(State.Can);
}
void ABotanicusWaterReserveActor::ReleaseCan(ABotanicusWateringCanActor* Can)
{
    if(!HasAuthority() || State.Can!=Can)return;
    SetOpen(false);
    if(IsValid(Can))
    {
        Can->GetRootComponent()->TransformUpdated.Remove(CanTransformHandle);Can->SetRefillStation(nullptr);
    }
    CanTransformHandle.Reset();State.Can=nullptr;ForceNetUpdate();OnRep_Station();
}
bool ABotanicusWaterReserveActor::RequestFaucet(ABotanicusWateringCanActor* ExpectedCan,bool bOpen)
{
    if(!HasAuthority() || !IsRealStation() || bEnding)return false;
    if(bOpen==State.bOpen)return true;
    if(bOpen && (!State.Can || State.Can!=ExpectedCan || !ContainsCan(State.Can) || State.Can->IsFull()))return false;
    SetOpen(bOpen);return true;
}
void ABotanicusWaterReserveActor::SetOpen(bool bOpen)
{
    if(State.bOpen==bOpen)return;
    State.bOpen=bOpen;LastTransferTime=GetWorld()->GetTimeSeconds();
    if(bOpen){++State.OpenCount;UBotanicusAudioWorldSubsystem::Emit(this,TEXT("FaucetOpen"),FaucetOutlet->GetComponentLocation());}
    else if(bWaterChanged)
    {
        bWaterChanged=false;if(auto* GM=GetWorld()->GetAuthGameMode<ABotanicusGameMode>())GM->ScheduleInventoryAutosave();
    }
    ForceNetUpdate();OnRep_Station();
}
void ABotanicusWaterReserveActor::UpdateStation()
{
    if(!IsRealStation() || bEnding)return;
    if(HasAuthority())
    {
        const double Now=GetWorld()->GetTimeSeconds();const float Delta=FMath::Clamp(float(Now-LastTransferTime),0.f,.15f);LastTransferTime=Now;
        if(State.Can && !ContainsCan(State.Can))ReleaseCan(State.Can);
        if(!State.Can)
        {
            TArray<FOverlapResult> Hits;FCollisionQueryParams Q(SCENE_QUERY_STAT(RefillSlot),false,this);
            GetWorld()->OverlapMultiByObjectType(Hits,FillZone->GetComponentLocation(),FillZone->GetComponentQuat(),FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
                FCollisionShape::MakeBox(FillZone->GetScaledBoxExtent()),Q);
            for(const auto& Hit:Hits)if(auto* Can=Cast<ABotanicusWateringCanActor>(Hit.GetActor());Can && TryDock(Can,false))break;
        }
        if(State.bOpen)
        {
            if(!State.Can || State.Can->IsFull())SetOpen(false);
            else {bWaterChanged|=State.Can->AddWater(.4f*Delta);if(State.Can->IsFull())SetOpen(false);}
        }
    }
    UpdateCosmetics();
}
void ABotanicusWaterReserveActor::OnRep_Station(){UpdateCosmetics();}
void ABotanicusWaterReserveActor::UpdateCosmetics()
{
    if(GetNetMode()==NM_DedicatedServer || !IsRealStation() || bEnding)return;
    auto* PC=Cast<ABotanicusPlayerController>(GetWorld()->GetFirstPlayerController());
    const bool Area=PC && PC->WantsRefillZone(this);
    ZoneFloor->SetVisibility(Area);ZoneFloor->SetOverlayMaterial(nullptr);ZoneFloor->SetRenderCustomDepth(false);
    for(UStaticMeshComponent* C:ZoneBorder){C->SetVisibility(Area);C->SetOverlayMaterial(nullptr);C->SetRenderCustomDepth(false);}
    const bool Active=State.bOpen && State.Can && ContainsCan(State.Can) && PC && PC->GetPawn() &&
        FVector::DistSquared(PC->GetPawn()->GetActorLocation(),GetActorLocation())<FMath::Square(1800.f);
    if(!Active){StopCosmetics();return;}
    const FVector Start=FaucetOutlet->GetComponentLocation(),Target=State.Can->GetFillTargetLocation();
    const FVector Gravity(0,0,-580);const float Flight=FMath::Clamp(FVector::Dist(Start,Target)/350.f,.12f,.5f);
    Stream->SetWorldLocation(Start);Stream->SetVariableVec3(TEXT("User.Velocity"),(Target-Start-.5f*Gravity*Flight*Flight)/Flight);
    Stream->SetVariableVec3(TEXT("User.Gravity"),Gravity);Stream->SetVariableFloat(TEXT("User.Lifetime"),Flight);
    Stream->SetVariableFloat(TEXT("User.Size"),.9f);Stream->SetVariableFloat(TEXT("User.SpawnRate"),90);
    Stream->SetVariableLinearColor(TEXT("User.Tint"),FLinearColor(.16f,.62f,.92f,.7f));
    if(!Stream->IsActive())Stream->Activate(true);
    const double Now=GetWorld()->GetTimeSeconds();
    if(Now-LastSplashTime>.25)
    {
        LastSplashTime=Now;Splash->DeactivateImmediate();Splash->SetWorldLocation(Target);
        Splash->SetVariableInt(TEXT("User.BurstCount"),2);Splash->SetVariableFloat(TEXT("User.Lifetime"),.18f);Splash->SetVariableFloat(TEXT("User.Size"),.6f);
        Splash->SetVariableLinearColor(TEXT("User.Tint"),FLinearColor(.16f,.62f,.92f,.65f));Splash->SetVariableVec3(TEXT("User.Velocity"),FVector(0,0,25));Splash->SetVariableVec3(TEXT("User.Gravity"),FVector(0,0,-180));Splash->Activate(true);
    }
    if(!FlowVoice.IsValid())if(auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>())FlowVoice=Audio->StartRefillLoop(this,Start);
    if(FlowVoice.IsValid())FlowVoice->SetWorldLocation(Start);
}
void ABotanicusWaterReserveActor::StopCosmetics()
{
    Stream->DeactivateImmediate();Splash->DeactivateImmediate();
    if(FlowVoice.IsValid()){FlowVoice->Stop();FlowVoice.Reset();}
}
void ABotanicusWaterReserveActor::ConfigureAsLocalPreview(bool bIsValid)
{
    GetWorldTimerManager().ClearTimer(StationTimer);
    Super::ConfigureAsLocalPreview(bIsValid);ReserveText->SetVisibility(false);ZoneFloor->SetVisibility(false);for(UStaticMeshComponent* C:ZoneBorder)C->SetVisibility(false);
    FaucetInteraction->SetCollisionEnabled(ECollisionEnabled::NoCollision);StopCosmetics();
}
void ABotanicusWaterReserveActor::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;GetWorldTimerManager().ClearTimer(StationTimer);
    if(HasAuthority() && State.Can)ReleaseCan(State.Can);
    StopCosmetics();Super::EndPlay(Reason);
}
void ABotanicusWaterReserveActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ABotanicusWaterReserveActor,State);
}

ABotanicusWateringCanActor* ABotanicusWaterReserveActor::GetDockedCan() const {return State.Can;}
