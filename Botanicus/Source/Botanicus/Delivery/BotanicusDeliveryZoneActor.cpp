// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryZoneActor.h"

#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "EngineUtils.h"
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

FVector ABotanicusDeliveryZoneActor::GetParcelSpawnLocation(
	int32 ParcelIndex) const
{
	const int32 Column = FMath::Abs(ParcelIndex) % 3;
	const int32 Row = FMath::Abs(ParcelIndex) / 3;
    FVector Spawn=GetActorLocation()+FVector((Column-1)*160.f,(Row%3-1)*160.f,0);
    if(bUseWalkablePlatformCollision && WalkableDeck)
    {
        // Keep the existing delivery grid on its actual supporting surface.
        const FVector Up=WalkableDeck->GetUpVector();
        const FVector Top=WalkableDeck->GetComponentLocation()+Up*WalkableDeck->GetScaledBoxExtent().Z;
        if(Up.Z>.7f)Spawn.Z=Top.Z-(Up.X*(Spawn.X-Top.X)+Up.Y*(Spawn.Y-Top.Y))/Up.Z;
    }
    return Spawn;
}

bool ABotanicusDeliveryZoneActor::StartParcelDelivery(
	ABotanicusDeliveryParcelActor* Parcel, int32 ParcelIndex, bool& bOutGroundFallback) const
{
	bOutGroundFallback = false;
	if (!HasAuthority() || !IsValid(Parcel) || !GetWorld()) return false;
	const FVector Extent = Parcel->GetParcelHalfExtent() * Parcel->GetActorScale3D().GetAbs();
	const float Radius = Extent.Size(); // Includes every orientation during the fall.
	const float Margin = Radius + 25.0f;
	const bool bDeck = bUseWalkablePlatformCollision && WalkableDeck;
	const FVector Up = bDeck ? WalkableDeck->GetUpVector() : GetActorUpVector();
	if (Up.Z < 0.95f) return false;
	const FVector Center = bDeck
		? WalkableDeck->GetComponentLocation() + Up * WalkableDeck->GetScaledBoxExtent().Z
		: GetActorLocation();
	const FQuat Facing = FRotator(0, bDeck ? WalkableDeck->GetComponentRotation().Yaw : GetActorRotation().Yaw, 0).Quaternion();
	const FVector Across = Facing.GetAxisX(), Along = Facing.GetAxisY();
	const FVector Half = bDeck ? WalkableDeck->GetScaledBoxExtent()
		: FVector(ZoneSize.X, ZoneSize.Y, 0) * GetActorScale3D().GetAbs() * 0.5f;
	if (Half.X <= Margin || Half.Y <= Margin) return false;
	const float Step = FMath::Max(160.0f, Radius * 2.0f + 30.0f);
	const int32 Columns = FMath::Max(1, FMath::FloorToInt((Half.X - Margin) * 2 / Step) + 1);
	const int32 Rows = FMath::Max(1, FMath::FloorToInt((Half.Y - Margin) * 2 / Step) + 1);
	TArray<FVector2D> Slots;
	for (int32 X = 0; X < Columns; ++X)
		for (int32 Y = 0; Y < Rows; ++Y)
			Slots.Emplace((X - (Columns - 1) * 0.5f) * Step, (Y - (Rows - 1) * 0.5f) * Step);
	Slots.Sort([](const FVector2D& A, const FVector2D& B) { return A.SizeSquared() < B.SizeSquared(); });
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusDeliveryDrop), false, Parcel);
	FVector Fallback = FVector::ZeroVector;
	bool bHasFallback = false;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FVector2D Slot = Slots[(Index + FMath::Abs(ParcelIndex)) % Slots.Num()];
		FVector Ground = Center + Across * Slot.X + Along * Slot.Y;
		Ground.Z = Center.Z - (Up.X * (Ground.X - Center.X) + Up.Y * (Ground.Y - Center.Y)) / Up.Z;
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByChannel(Floor, Ground + FVector(0, 0, 10),
			Ground - FVector(0, 0, 100), ECC_PhysicsBody, Query) || Floor.ImpactNormal.Z < 0.95f ||
			(bDeck && Floor.GetComponent() != WalkableDeck)) continue;
		Ground.Z = Floor.ImpactPoint.Z;
		bool bReserved = false;
		// Reserve incoming actors immediately, before Chaos has advanced a frame.
		for (TActorIterator<ABotanicusDeliveryParcelActor> It(GetWorld()); It; ++It)
		{
			if (*It == Parcel || It->ActorHasTag(TEXT("BotanicusPlacementPreview"))) continue;
			const float OtherRadius = (It->GetParcelHalfExtent() * It->GetActorScale3D().GetAbs()).Size();
			if (FVector::DistSquared2D(Ground, It->GetActorLocation()) <
				FMath::Square(Radius + OtherRadius + 30.0f)) { bReserved = true; break; }
		}
		const FVector Rest = Ground + FVector(0, 0, Extent.Z + 2.0f);
		if (bReserved || GetWorld()->OverlapBlockingTestByChannel(Rest, Facing, ECC_PhysicsBody,
			FCollisionShape::MakeBox(Extent), Query)) continue;
		const FVector Spawn = Ground + FVector(0, 0, FMath::Max(DeliverySpawnHeight, Radius + 3.0f));
		FHitResult Obstacle;
		if (!GetWorld()->SweepSingleByChannel(Obstacle, Spawn, Ground + FVector(0, 0, Radius + 2.0f),
			FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(Radius), Query))
		{
			Parcel->SetActorLocationAndRotation(Spawn, Facing, false, nullptr, ETeleportType::TeleportPhysics);
			Parcel->StartDeliveryPhysics();
			return true;
		}
		if (!bHasFallback) { Fallback = Rest; bHasFallback = true; }
	}
	if (bHasFallback)
	{
		// An overhead obstruction must never strand a paid delivery on a roof.
		Parcel->SetActorLocationAndRotation(Fallback, Facing, false, nullptr, ETeleportType::TeleportPhysics);
		Parcel->StartDeliveryPhysics(false);
		bOutGroundFallback = true;
		UE_LOG(LogTemp, Warning, TEXT("Delivery %s: no clear vertical corridor; safe ground placement used."), *Parcel->GetName());
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("Delivery zone %s has no safe space for carton %s."), *GetName(), *Parcel->GetName());
	return false; // Existing caller retains its delivery-failure/refund behaviour.
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
