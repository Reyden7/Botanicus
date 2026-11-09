// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusWateringCanActor.h"
#include "Growing/BotanicusWaterReserveActor.h"

#include "BotanicusCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "ItemDataAsset.h"
#include "ItemDataSubsystem.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "NiagaraComponent.h"
#include "Visuals/BotanicusJuiceSettings.h"
#include "Net/UnrealNetwork.h"

ABotanicusWateringCanActor::ABotanicusWateringCanActor()
{
	PrimaryActorTick.bCanEverTick = false;

	WaterNozzle = CreateDefaultSubobject<USceneComponent>(TEXT("Water Nozzle"));
	WaterNozzle->SetupAttachment(SceneRoot);
	WaterNozzle->bEditableWhenInherited = true;
    WateringCanFillTarget=CreateDefaultSubobject<USceneComponent>(TEXT("WateringCanFillTarget"));
    WateringCanFillTarget->SetupAttachment(Mesh);WateringCanFillTarget->SetRelativeLocation(FVector(8,0,16));WateringCanFillTarget->bEditableWhenInherited=true;
}

namespace
{
// Measured from the exported render geometry: the imported Solar can is already
// upright; its lowest support vertices are on the base, not the spout/handle.
constexpr float SolarCanBaseZ=-32.23605f;
bool IsCalibratedSolarCan(const UStaticMesh* Asset)
{
    static const FName Name(TEXT("Solar_Garden_Watering_Can_Import0002"));
    static const FName Package(TEXT("/Game/Botanicus/Items/itemsMesh/arosoire/Solar_Garden_Watering_Can_Import0002"));
    return Asset && Asset->GetFName()==Name && Asset->GetOutermost()->GetFName()==Package;
}
}
bool ABotanicusWateringCanActor::ResolveGroundGeometry(const UObject* Context,const FBotanicusItemDefinition& Definition,const FVector& ActorScale,FVector& Extent,float& BaseOffset)
{
    UClass* ItemClass=Definition.WorldActorClass.LoadSynchronous();
    if(!ItemClass || !ItemClass->IsChildOf(StaticClass()))return false;
    const auto* Defaults=ItemClass->GetDefaultObject<ABotanicusWateringCanActor>();
    const UStaticMesh* Asset=Defaults->Mesh ? Defaults->Mesh->GetStaticMesh():nullptr;
    FVector Scale=Defaults->Mesh ? Defaults->Mesh->GetRelativeScale3D():FVector::OneVector;
    if(!Defaults->UsesBlueprintAppearance())
    {
        if(const auto* Data=UItemDataSubsystem::Get(Context).GetItemDataAsset(TEXT("WateringCan")))
            if(auto* DataMesh=Data->GetItemStaticMesh().LoadSynchronous()){Asset=DataMesh;Scale=FVector::OneVector;}
    }
    if(!IsCalibratedSolarCan(Asset))return false;
    Scale=(Scale*ActorScale).GetAbs();
    Extent=Asset->GetBounds().BoxExtent*Scale;
    BaseOffset=-SolarCanBaseZ*Scale.Z;
    return true;
}
void ABotanicusWateringCanActor::UpdatePresentationPose()
{
    if(HasAnyFlags(RF_ClassDefaultObject) || !Mesh || !IsCalibratedSolarCan(Mesh->GetStaticMesh()))return;
    const bool Held=IsValid(Carrier) || ActorHasTag(TEXT("BotanicusEquippedPreview")) ||
        ActorHasTag(TEXT("BotanicusInspectionPreview")) || ActorHasTag(TEXT("BotanicusJuiceRemotePreview"));
    const auto* Defaults=GetClass()->GetDefaultObject<ABotanicusWateringCanActor>();
    // Leave the authored Blueprint pose intact in hand; only world/hologram
    // instances use the verified upright import frame and centred mesh pivot.
    Mesh->SetRelativeLocationAndRotation(Held ? Defaults->Mesh->GetRelativeLocation():FVector::ZeroVector,
        Held ? Defaults->Mesh->GetRelativeRotation():FRotator::ZeroRotator);
}
void ABotanicusWateringCanActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);UpdatePresentationPose();
}
void ABotanicusWateringCanActor::ApplyItemDefinition()
{
    Super::ApplyItemDefinition();UpdatePresentationPose();QueueGroundSettling();
}
void ABotanicusWateringCanActor::QueueGroundSettling()
{
    if(GetWorld() && HasAuthority() && !GroundSettleTimer.IsValid() && Mesh && IsCalibratedSolarCan(Mesh->GetStaticMesh()))
        GroundSettleTimer=GetWorldTimerManager().SetTimerForNextTick(this,&ThisClass::SettleAfterSpawn);
}
void ABotanicusWateringCanActor::SettleAfterSpawn()
{
    // Also reconcile old saved ground transforms, after spawn callers have set
    // preview tags/carriers/throw physics. Never drag an equipped or thrown can.
    if(!GetIsReplicated() || GetOwner() || Carrier || GetAttachParentActor() || !GetActorEnableCollision() ||
       ActorHasTag(TEXT("BotanicusPlacementPreview")) || ActorHasTag(TEXT("BotanicusEquippedPreview")) ||
       ActorHasTag(TEXT("BotanicusInspectionPreview")) || ActorHasTag(TEXT("BotanicusJuiceRemotePreview")) ||
       !Mesh || Mesh->IsSimulatingPhysics() || !IsCalibratedSolarCan(Mesh->GetStaticMesh()))return;
    const FVector Base=Mesh->GetComponentTransform().TransformPosition(FVector(0,0,SolarCanBaseZ));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(WateringCanGroundReconcile),false,this);FHitResult Floor;
    if(GetWorld()->LineTraceSingleByChannel(Floor,Base+FVector(0,0,4),Base-FVector(0,0,150),ECC_Visibility,Query) && Floor.ImpactNormal.Z>=.7f)
    {
        const float Correction=Floor.ImpactPoint.Z+GroundClearance-Base.Z;
        if(FMath::Abs(Correction)>.05f){SetActorLocation(GetActorLocation()+FVector(0,0,Correction),false,nullptr,ETeleportType::TeleportPhysics);ForceNetUpdate();}
    }
}

void ABotanicusWateringCanActor::SetWateringEffectActive(
	bool bActive,
	const FVector& TargetWorldLocation)
{
	// Kept to avoid breaking existing input/gameplay call sites.  The visual
	// implementation is intentionally absent while the new water VFX is paused.
	(void)bActive;
	(void)TargetWorldLocation;
}

void ABotanicusWateringCanActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusWateringCanActor, Carrier);
	DOREPLIFETIME(ABotanicusWateringCanActor, WaterLevel);
}

void ABotanicusWateringCanActor::ConfigureAsLocalPreview(bool bIsValid)
{
    UpdatePresentationPose();
	Super::ConfigureAsLocalPreview(bIsValid);
}

bool ABotanicusWateringCanActor::TryPickUp(
	ABotanicusCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character) ||
		IsValid(Carrier) || IsValid(Character->GetHeldWateringCan()))
	{
		return false;
	}

    if(auto* Station=RefillStation.Get())Station->ReleaseCan(this);
	Carrier = Character;
	Character->SetHeldWateringCan(this);
	SetOwner(Character->GetController());
	ApplyCarrierState();
	ForceNetUpdate();
	return true;
}

void ABotanicusWateringCanActor::Drop()
{
	if (!HasAuthority() || !IsValid(Carrier))
	{
		return;
	}

	ABotanicusCharacter* PreviousCarrier = Carrier;
	FVector DropLocation =
		PreviousCarrier->GetActorLocation() +
		PreviousCarrier->GetActorForwardVector() * 85.0f;
	const float PlacementHalfHeight =
		FMath::Max(2.0f, GetPlacementBoxExtent().GetAbs().Z);

	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusWateringCanDropFloor),
		false);
	FloorQuery.AddIgnoredActor(PreviousCarrier);
	FloorQuery.AddIgnoredActor(this);
	const FVector TraceStart(
		DropLocation.X,
		DropLocation.Y,
		PreviousCarrier->GetActorLocation().Z + 160.0f);
	const FVector TraceEnd(
		DropLocation.X,
		DropLocation.Y,
		PreviousCarrier->GetActorLocation().Z - 500.0f);
	FHitResult FloorHit;
	if (GetWorld()->LineTraceSingleByChannel(
			FloorHit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			FloorQuery) &&
		FloorHit.ImpactNormal.Z >= 0.7f)
	{
		DropLocation.Z =
			FloorHit.ImpactPoint.Z + PlacementHalfHeight + (IsCalibratedSolarCan(Mesh->GetStaticMesh()) ? GroundClearance:2.0f);
	}
	else
	{
		// Safe fallback for unusual levels without a visible floor beneath
		// the player.
		DropLocation.Z =
			PreviousCarrier->GetActorLocation().Z -
			88.0f +
			PlacementHalfHeight +
			2.0f;
	}

	const FRotator DropRotation(0.0f, PreviousCarrier->GetActorRotation().Yaw, 0.0f);
	PreviousCarrier->SetHeldWateringCan(nullptr);
	Carrier = nullptr;
	SetOwner(nullptr);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(
		DropLocation,
		DropRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ApplyCarrierState();
	ForceNetUpdate();
}

bool ABotanicusWateringCanActor::ConsumeWater(float Amount)
{
	if (!HasAuthority() || Amount <= 0.0f || !HasWater())
	{
		return false;
	}
	WaterLevel = FMath::Clamp(WaterLevel - Amount, 0.0f, 1.0f);
	ForceNetUpdate();
	return true;
}

bool ABotanicusWateringCanActor::AddWater(float Amount)
{
	if (!HasAuthority() || Amount <= 0.0f || IsFull())
	{
		return false;
	}
	WaterLevel = FMath::Clamp(WaterLevel + Amount, 0.0f, 1.0f);
	ForceNetUpdate();
	return true;
}

void ABotanicusWateringCanActor::Refill()
{
	if (!HasAuthority())
	{
		return;
	}
	WaterLevel = 1.0f;
	ForceNetUpdate();
}

void ABotanicusWateringCanActor::RestoreWaterLevel(float InWaterLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	WaterLevel = FMath::Clamp(InWaterLevel, 0.0f, 1.0f);
	ForceNetUpdate();
}

void ABotanicusWateringCanActor::OnRep_Carrier()
{
	ApplyCarrierState();
}

void ABotanicusWateringCanActor::ApplyCarrierState()
{
	if (IsValid(Carrier))
	{
		AttachToComponent(
			Carrier->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			TEXT("hand_r"));
		SetActorRelativeLocation(FVector(5.0f, 2.0f, -4.0f));
		SetActorRelativeRotation(FRotator(15.0f, -85.0f, 15.0f));
		SetActorEnableCollision(false);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorEnableCollision(true);
	}
    UpdatePresentationPose();
}

void ABotanicusWateringCanActor::BeginPlay()
{
    Super::BeginPlay();UpdatePresentationPose();QueueGroundSettling();
    bUseMeshNozzleFallback=WaterNozzle && WaterNozzle->GetRelativeLocation().IsNearlyZero();
    // Neutralise legacy Blueprint Niagara components; the single juice owner renders the replacement.
    TArray<UNiagaraComponent*> Legacy; GetComponents(Legacy);
    for (auto* Component : Legacy)
    {
        Component->bAutoActivate=false; Component->DeactivateImmediate(); Component->SetVisibility(false);
    }
}
FVector ABotanicusWateringCanActor::GetJuiceNozzleLocation() const
{
    if (bUseMeshNozzleFallback && Mesh) return Mesh->GetComponentTransform().TransformPosition(GetDefault<UBotanicusJuiceSettings>()->NozzleMeshLocalOffset);
    return WaterNozzle ? WaterNozzle->GetComponentLocation() : GetActorLocation();
}

FVector ABotanicusWateringCanActor::GetFillTargetLocation() const {return WateringCanFillTarget->GetComponentLocation();}
void ABotanicusWateringCanActor::SetRefillStation(ABotanicusWaterReserveActor* Station){RefillStation=Station;}
void ABotanicusWateringCanActor::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(GroundSettleTimer);
    if(HasAuthority())if(auto* Station=RefillStation.Get())Station->ReleaseCan(this);
    Super::EndPlay(Reason);
}

ABotanicusWaterReserveActor* ABotanicusWateringCanActor::GetRefillStation() const {return RefillStation.Get();}
