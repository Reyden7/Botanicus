// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryZoneActor.h"

#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "EngineUtils.h"
#include "Delivery/BotanicusDeliveryShip.h"
#include "BotanicusPlayerController.h"
#include "BotanicusGameMode.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusDeliveryZoneActor::ABotanicusDeliveryZoneActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Delivery Pad"));
	Pad->SetupAttachment(Root);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pad->SetRelativeLocation(FVector(0.0f, 0.0f, 0.25f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		Pad->SetStaticMesh(PlaneFinder.Object);
	}

    WalkableDeck=CreateDefaultSubobject<UBoxComponent>(TEXT("Walkable Delivery Deck"));WalkableDeck->SetupAttachment(Pad);
    WalkableDeck->SetRelativeLocation(FVector(0,0,30.2f));WalkableDeck->SetBoxExtent(FVector(211.4f,112.4f,1.f));
    RampLanding=CreateDefaultSubobject<UBoxComponent>(TEXT("Delivery Ramp Landing"));RampLanding->SetupAttachment(Pad);
    RampLanding->SetRelativeLocation(FVector(.16855f,125.7299f,30.53161f));RampLanding->SetBoxExtent(FVector(79.73f,14.0299f,1.f));
    WalkableRamp=CreateDefaultSubobject<UBoxComponent>(TEXT("Walkable Delivery Ramp"));WalkableRamp->SetupAttachment(Pad);
    // Avoid shear from the instance's unequal XYZ scale. Location stays attached;
    // world orientation and dimensions are calculated from the two ramp endpoints.
    WalkableRamp->SetAbsolute(false,true,true);
    for(auto* Surface:{WalkableDeck.Get(),RampLanding.Get(),WalkableRamp.Get()})
    {Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);Surface->SetGenerateOverlapEvents(false);Surface->CanCharacterStepUpOn=ECB_Yes;Surface->SetHiddenInGame(true);}

    DeliveryDropCenter = CreateDefaultSubobject<USceneComponent>(TEXT("DeliveryDropCenter"));
    DeliveryDropCenter->SetupAttachment(Pad);
    // Centre of the horizontal receiving plate, measured on platforme_de_livraison.
    DeliveryDropCenter->SetRelativeLocation(FVector(-.0343475f,-.318672f,31.391754f));
    DeliveryShipClass = TSoftClassPtr<ABotanicusDeliveryShip>(FSoftObjectPath(
        TEXT("/Game/Botanicus/blueprints/BP_BotanicusDeliveryShip.BP_BotanicusDeliveryShip_C")));

	ZoneLabel = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("DeliveryZoneLabel"));
	ZoneLabel->SetupAttachment(Root);
	ZoneLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	ZoneLabel->SetHorizontalAlignment(EHTA_Center);
	ZoneLabel->SetVerticalAlignment(EVRTA_TextCenter);
	ZoneLabel->SetWorldSize(24.0f);
	ZoneLabel->SetText(
		NSLOCTEXT(
			"BotanicusDelivery",
			"DeliveryZoneLabel",
			"ZONE DE LIVRAISON"));
	ZoneLabel->SetTextRenderColor(FColor(255, 210, 45));
	ZoneLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshVisuals();
}

void ABotanicusDeliveryZoneActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SnapToUnderlyingGround();
	}
	RefreshWalkableCollision();
}

void ABotanicusDeliveryZoneActor::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisuals();
}

void ABotanicusDeliveryZoneActor::RefreshVisuals()
{
	if (Pad)
	{
		Pad->SetRelativeLocation(FVector(0.0f, 0.0f, 0.25f));
		Pad->SetRelativeScale3D(
			FVector(ZoneSize.X / 100.0f, ZoneSize.Y / 100.0f, 1.0f));
		Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	RefreshWalkableCollision();
}

void ABotanicusDeliveryZoneActor::RefreshWalkableCollision()
{
    if(!WalkableDeck || !RampLanding || !WalkableRamp || !Pad)return;
    for(auto* Surface:{WalkableDeck.Get(),RampLanding.Get(),WalkableRamp.Get()})
    {
        Surface->SetCollisionProfileName(bUseWalkablePlatformCollision ? TEXT("BlockAll"):TEXT("NoCollision"));
        Surface->SetCollisionEnabled(bUseWalkablePlatformCollision ? ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        Surface->SetGenerateOverlapEvents(false);Surface->CanCharacterStepUpOn=ECB_Yes;
    }
    if(!bUseWalkablePlatformCollision)return;
    const FTransform PadTransform=Pad->GetComponentTransform();
    const FVector Top=PadTransform.TransformPosition(RampTop),Bottom=PadTransform.TransformPosition(RampBottom);
    const FVector Across=PadTransform.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
    const FVector Along=(Bottom-Top).GetSafeNormal();const FVector Normal=FVector::CrossProduct(Across,Along).GetSafeNormal();
    WalkableRamp->SetWorldRotation(FRotationMatrix::MakeFromXY(Across,Along).ToQuat());
    WalkableRamp->SetWorldScale3D(FVector::OneVector);
    WalkableRamp->SetBoxExtent(FVector(RampHalfWidth*PadTransform.GetScale3D().X,(Bottom-Top).Size()*.5f,1.f));
    WalkableRamp->SetRelativeLocation(PadTransform.InverseTransformPosition((Top+Bottom)*.5f-Normal));
}

FTransform ABotanicusDeliveryZoneActor::GetDeliveryFrame() const
{
    return FTransform(FRotator(0, Pad->GetComponentRotation().Yaw,0),DeliveryDropCenter->GetComponentLocation());
}

FVector ABotanicusDeliveryZoneActor::GetParcelSpawnLocation(int32 ParcelIndex) const
{
    return GetDeliveryFrame().GetLocation() + FVector(0,0,DeliverySpawnHeight);
}

bool ABotanicusDeliveryZoneActor::StartParcelDelivery(ABotanicusDeliveryParcelActor* Parcel,
    int32 ParcelIndex, bool& bOutGroundFallback) const
{
    bOutGroundFallback = false;
    if (!HasAuthority() || !IsValid(Parcel) || !GetWorld()) return false;
    const FTransform Frame = GetDeliveryFrame();
    const FVector Extent = Parcel->GetParcelHalfExtent() * Parcel->GetActorScale3D().GetAbs();
    const FVector2D Size = IsValid(ActiveShip) ? ActiveShip->DropAreaSize : FVector2D(200,200);
    const FVector Cargo = IsValid(ActiveShip) ? ActiveShip->GetCargoLocation()
        : Frame.GetLocation()+FVector(0,0,DeliverySpawnHeight);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusDeliveryDrop),false,Parcel);
    // Test only the creation volume. A carton on the floor must not prohibit stacking.
    for (int32 Attempt=0; Attempt<32; ++Attempt)
    {
        const FVector Local(FMath::FRandRange(-FMath::Max(1.,Size.X)*.5,FMath::Max(1.,Size.X)*.5),
            FMath::FRandRange(-FMath::Max(1.,Size.Y)*.5,FMath::Max(1.,Size.Y)*.5),0);
        const FVector Spawn = FVector(Frame.TransformPosition(Local).X,Frame.TransformPosition(Local).Y,Cargo.Z);
        if (GetWorld()->OverlapBlockingTestByChannel(Spawn,Frame.GetRotation(),ECC_PhysicsBody,
            FCollisionShape::MakeSphere(Extent.Size()+3),Query)) continue;
        Parcel->SetActorLocationAndRotation(Spawn,Frame.GetRotation(),false,nullptr,ETeleportType::TeleportPhysics);
        Parcel->StartDeliveryPhysics();
        UE_LOG(LogTemp,Display,TEXT("DeliveryDrop %s local=(%.2f,%.2f) height=%.2f"),
            *Parcel->GetName(),Local.X,Local.Y,Cargo.Z-Frame.GetLocation().Z);
        return true;
    }
    return false; // Retry without consuming the order or forcing a ground placement.
}

bool ABotanicusDeliveryZoneActor::EnqueueOrder(const FBotanicusPendingOrder& Order)
{
    if (!HasAuthority() || !Order.OrderId.IsValid() || Order.ItemKey.IsNone()) return false;
    if (QueuedOrderIds.Contains(Order.OrderId)) return true;
    DeliveryQueue.Add({Order.OrderId,Order.ItemKey,FMath::Max(1,Order.Quantity)});
    QueuedOrderIds.Add(Order.OrderId);
    if (!IsValid(ActiveShip)) StartNextShip();
    return true;
}

void ABotanicusDeliveryZoneActor::StartNextShip()
{
    if (!HasAuthority() || IsValid(ActiveShip) || !HasQueuedOrders()) return;
    UClass* Class = DeliveryShipClass.LoadSynchronous();
    if (!Class)
    {
        UE_LOG(LogTemp,Error,TEXT("Delivery ship Blueprint unavailable: retaining paid orders."));
        GetWorldTimerManager().SetTimer(ShipRestartTimer,this,&ThisClass::StartNextShip,1.f,false);
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ActiveShip = GetWorld()->SpawnActor<ABotanicusDeliveryShip>(Class,GetDeliveryFrame(),Params);
    if (ActiveShip) ActiveShip->StartDelivery(this);
    else GetWorldTimerManager().SetTimer(ShipRestartTimer,this,&ThisClass::StartNextShip,1.f,false);
}

bool ABotanicusDeliveryZoneActor::DropNextOrder(ABotanicusDeliveryShip* Ship)
{
    if (!HasAuthority() || Ship != ActiveShip || DeliveryQueue.IsEmpty()) return false;
    auto* Mode = GetWorld()->GetAuthGameMode<ABotanicusGameMode>();
    if (!Mode) return false;
    const FQueuedDelivery Order = DeliveryQueue[0];
    if (!Mode->IsCatalogDeliveryPending(Order.OrderId))
    {
        DeliveryQueue.RemoveAt(0); QueuedOrderIds.Remove(Order.OrderId); return false;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Parcel = GetWorld()->SpawnActor<ABotanicusDeliveryParcelActor>(GetParcelSpawnLocation(),FRotator::ZeroRotator,Params);
    if (!Parcel) return false;
    Parcel->InitializeParcel(Order.ItemKey,Order.Quantity);
    bool bFallback = false;
    if (!StartParcelDelivery(Parcel,0,bFallback)) { Parcel->Destroy(); return false; }
    // Synchronous game-thread handoff: the next save sees the carton OR the pending
    // order, never a freshly delivered carton plus a copy still due for redelivery.
    Mode->FinishCatalogDelivery(Order.OrderId,true);
    DeliveryQueue.RemoveAt(0); QueuedOrderIds.Remove(Order.OrderId);
    return true;
}

void ABotanicusDeliveryZoneActor::OnShipFinished(ABotanicusDeliveryShip* Ship)
{
    if (Ship != ActiveShip) return;
    ActiveShip = nullptr;
    if (!GetWorld()->bIsTearingDown)
        ShipRestartTimer = GetWorldTimerManager().SetTimerForNextTick(this,&ThisClass::StartNextShip);
}

void ABotanicusDeliveryZoneActor::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(ShipRestartTimer);
    if (HasAuthority() && IsValid(ActiveShip))
    {
        auto* Ship=ActiveShip.Get(); ActiveShip=nullptr; Ship->Destroy();
    }
    Super::EndPlay(Reason);
}

void ABotanicusDeliveryZoneActor::SnapToUnderlyingGround()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusDeliveryZoneGround),
		false,
		this);
	FHitResult GroundHit;
	const FVector Location = GetActorLocation();
	if (World->LineTraceSingleByChannel(
			GroundHit,
			Location + FVector(0.0f, 0.0f, 1.0f),
			Location - FVector(0.0f, 0.0f, 1000.0f),
			ECC_Visibility,
			QueryParams))
	{
		SetActorLocation(
			FVector(Location.X, Location.Y, GroundHit.ImpactPoint.Z),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		ForceNetUpdate();
	}
}

