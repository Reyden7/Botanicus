// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusWateringCanActor.h"

#include "BotanicusCharacter.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Water/BotanicusWaterSourceComponent.h"
#include "Water/BotanicusWaterTrajectory.h"

ABotanicusWateringCanActor::ABotanicusWateringCanActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> WaterJetFinder(
		TEXT("/Game/Botanicus/VFX/Watering/NS_BotanicusWateringJet.NS_BotanicusWateringJet"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> WaterImpactFinder(
		TEXT("/Game/Botanicus/VFX/Watering/NS_BotanicusWateringImpact.NS_BotanicusWateringImpact"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> StreamMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StreamMaterialFinder(
		TEXT("/Game/Botanicus/Materials/Effects/M_BotanicusWaterStreamStylized.M_BotanicusWaterStreamStylized"));
	WaterDropletSystem = WaterJetFinder.Object;
	WaterImpactSystem = WaterImpactFinder.Object;
	WaterStreamSegmentMesh = StreamMeshFinder.Object;
	WaterStreamMaterial = StreamMaterialFinder.Object;

	WaterJetEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Water Jet Niagara"));
	WaterJetEffect->SetupAttachment(SceneRoot);
	WaterJetEffect->SetAutoActivate(false);
	WaterJetEffect->SetCastShadow(false);
	WaterJetEffect->bEditableWhenInherited = true;
	if (WaterJetFinder.Succeeded())
	{
		WaterJetEffect->SetAsset(WaterDropletSystem);
	}

	WaterImpactEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Water Impact Niagara"));
	WaterImpactEffect->SetupAttachment(SceneRoot);
	WaterImpactEffect->SetAutoActivate(false);
	WaterImpactEffect->SetCastShadow(false);
	if (WaterImpactFinder.Succeeded())
	{
		WaterImpactEffect->SetAsset(WaterImpactSystem);
	}

	// The existing Niagara component is already placed at the spout in the
	// watering-can Blueprint. Inheriting that transform gives the gameplay
	// nozzle a useful default while still allowing a precise Blueprint offset.
	WaterNozzle = CreateDefaultSubobject<USceneComponent>(TEXT("Water Nozzle"));
	WaterNozzle->SetupAttachment(WaterJetEffect);
	WaterSourceComponent = CreateDefaultSubobject<UBotanicusWaterSourceComponent>(
		TEXT("Water Source"));
	WaterSourceComponent->SetNozzleComponent(WaterNozzle);
}

void ABotanicusWateringCanActor::BeginPlay()
{
	Super::BeginPlay();

	// BP_Item_WateringCan historically stored the spout transform on the old
	// Niagara component. Preserve that world transform once, then make the
	// nozzle the independent parent used by gameplay and cosmetics.
	if (WaterNozzle && SceneRoot)
	{
		WaterNozzle->DetachFromComponent(
			FDetachmentTransformRules::KeepWorldTransform);
		WaterNozzle->AttachToComponent(
			SceneRoot,
			FAttachmentTransformRules::KeepWorldTransform);
		// The old effect carried a non-uniform visual scale. A nozzle is a point
		// transform, so that scale must not leak into the new effect or physics.
		WaterNozzle->SetWorldScale3D(FVector::OneVector);
	}
	if (WaterJetEffect && WaterNozzle)
	{
		WaterJetEffect->DetachFromComponent(
			FDetachmentTransformRules::KeepWorldTransform);
		WaterJetEffect->AttachToComponent(
			WaterNozzle,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		WaterJetEffect->SetRelativeScale3D(FVector(0.15f));
		if (WaterDropletSystem)
		{
			// A Blueprint override may still reference NS_AnimeWater. Force the
			// mesh-free cosmetic system so the Blender curve is never rendered.
			WaterJetEffect->SetAsset(WaterDropletSystem);
		}
	}
	if (WaterImpactEffect && WaterImpactSystem)
	{
		WaterImpactEffect->SetAsset(WaterImpactSystem);
	}

	CreateWaterStreamVisualPool();
	HideWaterStreamVisuals();
}

void ABotanicusWateringCanActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshWateringEffect(DeltaSeconds);
}

void ABotanicusWateringCanActor::SetWateringEffectActive(bool bActive)
{
	if (WaterSourceComponent)
	{
		WaterSourceComponent->SetWaterSourceActive(bActive);
	}
	bWateringEffectActive = bActive && WaterSourceComponent &&
		WaterStreamSegmentMesh;
	if (bWateringEffectActive)
	{
		if (WaterJetEffect && WaterJetEffect->GetAsset() &&
			!WaterJetEffect->IsActive())
		{
			WaterJetEffect->Activate(true);
		}
	}
	else
	{
		if (WaterJetEffect)
		{
			WaterJetEffect->DeactivateImmediate();
		}
		if (WaterImpactEffect)
		{
			WaterImpactEffect->DeactivateImmediate();
		}
		HideWaterStreamVisuals();
	}
}

void ABotanicusWateringCanActor::RefreshWateringEffect(float DeltaSeconds)
{
	if (!bWateringEffectActive || !WaterSourceComponent || !SceneRoot)
	{
		return;
	}

	(void)DeltaSeconds;
	FBotanicusWaterStreamParams StreamParams;
	if (!WaterSourceComponent->BuildStreamParams(StreamParams))
	{
		HideWaterStreamVisuals();
		return;
	}

	TArray<const AActor*> IgnoredActors;
	if (Carrier)
	{
		IgnoredActors.Add(Carrier);
	}
	FBotanicusWaterHit WaterHit;
	TArray<FBotanicusWaterTrajectorySample> CollisionSamples;
	const bool bHasImpact = BotanicusWaterTrajectory::TraceFirstBlockingHit(
		GetWorld(),
		StreamParams,
		this,
		IgnoredActors,
		0.0f,
		WaterHit,
		&CollisionSamples);
	if (CollisionSamples.Num() < 2 || WaterStreamSegments.IsEmpty())
	{
		HideWaterStreamVisuals();
		return;
	}

	const float EndTime = bHasImpact
		? WaterHit.TimeOfImpact
		: CollisionSamples.Last().Time;
	float StreamLength = bHasImpact ? WaterHit.DistanceToImpact : 0.0f;
	if (!bHasImpact)
	{
		for (int32 Index = 1; Index < CollisionSamples.Num(); ++Index)
		{
			StreamLength += FVector::Distance(
				CollisionSamples[Index - 1].Position,
				CollisionSamples[Index].Position);
		}
	}
	const int32 SegmentCount = FMath::Clamp(
		FMath::CeilToInt(
			StreamLength / FMath::Max(5.0f, StreamParams.TargetSegmentLength)),
		1,
		WaterStreamSegments.Num());
	const FTransform RootTransform = SceneRoot->GetComponentTransform();
	const float BaseScale = StreamParams.StreamWidth / 100.0f;

	for (int32 Index = 0; Index < WaterStreamSegments.Num(); ++Index)
	{
		USplineMeshComponent* Segment = WaterStreamSegments[Index];
		if (!Segment)
		{
			continue;
		}
		if (Index >= SegmentCount || EndTime <= KINDA_SMALL_NUMBER)
		{
			Segment->SetVisibility(false);
			continue;
		}

		const float StartAlpha = static_cast<float>(Index) / SegmentCount;
		const float EndAlpha = static_cast<float>(Index + 1) / SegmentCount;
		const float StartTime = EndTime * StartAlpha;
		const float SegmentEndTime = EndTime * EndAlpha;
		const float SegmentDuration = SegmentEndTime - StartTime;
		const FVector StartWorld =
			BotanicusWaterTrajectory::PositionAtTime(StreamParams, StartTime);
		const FVector EndWorld = bHasImpact && Index == SegmentCount - 1
			? WaterHit.Location
			: BotanicusWaterTrajectory::PositionAtTime(
				StreamParams,
				SegmentEndTime);
		const FVector StartTangentWorld =
			BotanicusWaterTrajectory::VelocityAtTime(StreamParams, StartTime) *
			SegmentDuration;
		const FVector EndTangentWorld = (bHasImpact && Index == SegmentCount - 1
			? WaterHit.ImpactVelocity
			: BotanicusWaterTrajectory::VelocityAtTime(
				StreamParams,
				SegmentEndTime)) * SegmentDuration;

		Segment->SetStartAndEnd(
			RootTransform.InverseTransformPosition(StartWorld),
			RootTransform.InverseTransformVectorNoScale(StartTangentWorld),
			RootTransform.InverseTransformPosition(EndWorld),
			RootTransform.InverseTransformVectorNoScale(EndTangentWorld),
			false);
		const float StartTaper = FMath::Lerp(1.0f, 0.68f, StartAlpha);
		const float EndTaper = FMath::Lerp(1.0f, 0.68f, EndAlpha);
		Segment->SetStartScale(
			FVector2D(BaseScale * StartTaper),
			false);
		Segment->SetEndScale(
			FVector2D(BaseScale * EndTaper),
			false);
		Segment->SetVisibility(true);
		Segment->UpdateMesh();
	}

	const FVector InitialVelocity =
		StreamParams.Direction * StreamParams.InitialSpeed;
	const FVector ImpactPoint = bHasImpact
		? WaterHit.Location
		: CollisionSamples.Last().Position;
	if (WaterJetEffect && WaterJetEffect->GetAsset())
	{
		WaterJetEffect->SetWorldLocationAndRotation(
			StreamParams.Origin,
			FRotationMatrix::MakeFromZ(StreamParams.Direction).Rotator());
		WaterJetEffect->SetVariableVec3(TEXT("User.Origin"), StreamParams.Origin);
		WaterJetEffect->SetVariableVec3(TEXT("User.Direction"), StreamParams.Direction);
		WaterJetEffect->SetVariableVec3(TEXT("User.InitialVelocity"), InitialVelocity);
		WaterJetEffect->SetVariableVec3(TEXT("User.Gravity"), StreamParams.Gravity);
		WaterJetEffect->SetVariableVec3(TEXT("User.ImpactPoint"), ImpactPoint);
		WaterJetEffect->SetVariableVec3(
			TEXT("User.ImpactNormal"),
			bHasImpact ? WaterHit.Normal : FVector::UpVector);
		WaterJetEffect->SetVariableFloat(TEXT("User.TimeOfImpact"), EndTime);
		WaterJetEffect->SetVariableFloat(TEXT("User.InitialSpeed"), StreamParams.InitialSpeed);
		WaterJetEffect->SetVariableFloat(TEXT("User.FlowRate"), StreamParams.FlowRate);
		WaterJetEffect->SetVariableFloat(TEXT("User.StreamWidth"), StreamParams.StreamWidth);
		WaterJetEffect->SetVariableFloat(TEXT("User.DropletSpread"), StreamParams.DropletSpread);
		WaterJetEffect->SetVariableFloat(
			TEXT("User.DropletSpeedVariation"),
			StreamParams.DropletSpeedVariation);
		WaterJetEffect->SetVariableFloat(
			TEXT("User.MaxSimulationTime"),
			StreamParams.MaxSimulationTime);
		WaterJetEffect->SetVariableFloat(TEXT("User.MaxDistance"), StreamParams.MaxDistance);
		WaterJetEffect->SetVariableBool(TEXT("User.HasImpact"), bHasImpact);
	}

	if (WaterImpactEffect && WaterImpactEffect->GetAsset())
	{
		if (bHasImpact)
		{
			WaterImpactEffect->SetWorldLocationAndRotation(
				WaterHit.Location + WaterHit.Normal * 1.5f,
				FRotationMatrix::MakeFromZ(WaterHit.Normal).Rotator());
			WaterImpactEffect->SetWorldScale3D(
				FVector(FMath::Clamp(StreamParams.StreamWidth / 45.0f, 0.07f, 0.22f)));
			if (!WaterImpactEffect->IsActive())
			{
				WaterImpactEffect->Activate(true);
			}
		}
		else if (WaterImpactEffect->IsActive())
		{
			WaterImpactEffect->DeactivateImmediate();
		}
	}
}

void ABotanicusWateringCanActor::CreateWaterStreamVisualPool()
{
	if (!WaterStreamSegmentMesh || !SceneRoot || !WaterStreamSegments.IsEmpty())
	{
		return;
	}

	const int32 SegmentPoolSize = FMath::Clamp(MaximumVisualSegments, 4, 32);
	WaterStreamSegments.Reserve(SegmentPoolSize);
	for (int32 Index = 0; Index < SegmentPoolSize; ++Index)
	{
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
		if (!Segment)
		{
			continue;
		}
		AddInstanceComponent(Segment);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetupAttachment(SceneRoot);
		Segment->RegisterComponent();
		Segment->SetStaticMesh(WaterStreamSegmentMesh);
		Segment->SetForwardAxis(ESplineMeshAxis::Z, false);
		if (WaterStreamMaterial)
		{
			Segment->SetMaterial(0, WaterStreamMaterial);
		}
		Segment->SetCollisionProfileName(
			UCollisionProfile::NoCollision_ProfileName);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetCanEverAffectNavigation(false);
		Segment->SetCastShadow(false);
		Segment->bReceivesDecals = false;
		Segment->SetVisibility(false);
		WaterStreamSegments.Add(Segment);
	}
}

void ABotanicusWateringCanActor::HideWaterStreamVisuals()
{
	for (USplineMeshComponent* Segment : WaterStreamSegments)
	{
		if (Segment)
		{
			Segment->SetVisibility(false);
		}
	}
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
			FloorHit.ImpactPoint.Z + PlacementHalfHeight + 2.0f;
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
}
