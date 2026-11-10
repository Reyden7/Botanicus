// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryZoneActor.h"

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
