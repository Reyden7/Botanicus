// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusCartonLabel.h"
#include "Tools/BotanicusMultiToolComponent.h"

#include "Delivery/BotanicusParcelOpeningSettings.h"
#include "Delivery/BotanicusParcelPackagingSettings.h"
#include "Delivery/BotanicusParcelOpeningVisualActor.h"
#include "NiagaraSystem.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"
#include "BotanicusCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BotanicusGameMode.h"
#include "BotanicusPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusDeliveryParcelActor::ABotanicusDeliveryParcelActor()
{
    DeliveryImpactSound=TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Delivery/SW_DeliveryImpact.SW_DeliveryImpact")));
    DeliveryImpactAttenuation=TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/Delivery/SA_DeliveryImpact.SA_DeliveryImpact")));
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(30.0f);

	DeliveryBody = CreateDefaultSubobject<UBoxComponent>(TEXT("Delivery Physics Body"));
	SetRootComponent(DeliveryBody);
	SceneRoot->SetupAttachment(DeliveryBody);

	HandGripFrame = CreateDefaultSubobject<USceneComponent>(TEXT("HandGripFrame"));
	HandGripFrame->SetupAttachment(SceneRoot);
	LeftHandGrip = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHandGrip"));
	LeftHandGrip->SetupAttachment(HandGripFrame);
	RightHandGrip = CreateDefaultSubobject<USceneComponent>(TEXT("RightHandGrip"));
	RightHandGrip->SetupAttachment(HandGripFrame);
	// Wrist poses measured in Running_With_Box at 23/60 s, in the label-facing frame.
	LeftHandGrip->SetRelativeLocationAndRotation(FVector(-15.0, -24.0, -4.0), FRotator(60.05, 46.32, -79.23));
	RightHandGrip->SetRelativeLocationAndRotation(FVector(-15.0, 24.0, -4.0), FRotator(-56.43, 148.97, -58.95));
	DeliveryBody->SetCollisionProfileName(TEXT("PhysicsActor"));
	DeliveryBody->SetGenerateOverlapEvents(false);
	DeliveryBody->SetEnableGravity(true);
	DeliveryBody->BodyInstance.bUseCCD = true;
	DeliveryBody->BodyInstance.bGenerateWakeEvents = true;
	DeliveryBody->SetLinearDamping(0.1f);
	DeliveryBody->SetAngularDamping(1.5f);
    DeliveryBody->SetNotifyRigidBodyCollision(true);
    DeliveryBody->OnComponentHit.AddDynamic(this,&ThisClass::OnDeliveryHit);
	DeliveryBody->OnComponentSleep.AddDynamic(this, &ThisClass::OnDeliveryBodySleep);
	DeliveryBody->OnComponentWake.AddDynamic(this, &ThisClass::OnDeliveryBodyWake);
	DeliveryPhysicalMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("Delivery Cardboard Physics"));
	DeliveryPhysicalMaterial->Friction = 0.75f;
	DeliveryPhysicalMaterial->Restitution = 0.03f;
	DeliveryPhysicalMaterial->bOverrideRestitutionCombineMode = true;
	DeliveryPhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
	DeliveryBody->SetPhysMaterialOverride(DeliveryPhysicalMaterial);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionAction =
		NSLOCTEXT("BotanicusDelivery", "UnpackParcel", "Deballer");
	InteractionName =
		NSLOCTEXT("BotanicusDelivery", "DeliveryCarton", "Carton livre");
	bUseBlueprintAppearance = true;
	bUseSkeletalMeshAppearance = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	Mesh->SetVisibility(false, true);
	Mesh->SetHiddenInGame(true, true);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CartonMeshFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/Box1_anim.Box1_anim"));
	if (CartonMeshFinder.Succeeded())
	{
		SkeletalMeshVisual->SetSkeletalMesh(CartonMeshFinder.Object);
		SkeletalMeshVisual->SetVisibility(true, true);
		SkeletalMeshVisual->SetHiddenInGame(false, true);
		SkeletalMeshVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> OpeningAnimationFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/Box1_anim_Anim.Box1_anim_Anim"));
	OpeningAnimation = OpeningAnimationFinder.Succeeded()
		? OpeningAnimationFinder.Object
		: nullptr;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CardboardFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/cardboard.cardboard"));
	CardboardMaterial = CardboardFinder.Succeeded()
		? CardboardFinder.Object
		: nullptr;
	if (SkeletalMeshVisual && CardboardMaterial)
	{
		const int32 MaterialCount = FMath::Max(
			1,
			SkeletalMeshVisual->GetNumMaterials());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			SkeletalMeshVisual->SetMaterial(MaterialIndex, CardboardMaterial);
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TapeMaterialFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/carton/M_CartonTape.M_CartonTape"));
	TapeMaterial = TapeMaterialFinder.Succeeded()
		? TapeMaterialFinder.Object
		: nullptr;

	for (int32 SegmentIndex = 0;
		 SegmentIndex < TapeSegmentCount;
		 ++SegmentIndex)
	{
		UStaticMeshComponent* Segment =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(
					TEXT("Red Tape Segment %02d"),
					SegmentIndex));
		Segment->SetupAttachment(SceneRoot);
		Segment->SetStaticMesh(
			CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		if (TapeMaterial)
		{
			Segment->SetMaterial(0, TapeMaterial);
		}
		TapeSegments.Add(Segment);
	}

	LeftFlap = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Opened Left Flap"));
	LeftFlap->SetupAttachment(SceneRoot);
	LeftFlap->SetStaticMesh(
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
	LeftFlap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightFlap = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Opened Right Flap"));
	RightFlap->SetupAttachment(SceneRoot);
	RightFlap->SetStaticMesh(
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
	RightFlap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionIndicator =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("Interaction Indicator"));
	InteractionIndicator->SetupAttachment(SceneRoot);
	InteractionIndicator->SetTextRenderColor(FColor(255, 210, 30));
	InteractionIndicator->SetHorizontalAlignment(EHTA_Center);
	InteractionIndicator->SetVerticalAlignment(EVRTA_TextCenter);
	InteractionIndicator->SetWorldSize(25.0f);
	InteractionIndicator->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	LabelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LabelPivot"));
	LabelPivot->SetupAttachment(SkeletalMeshVisual);
	LabelPivot->SetRelativeLocation(FVector(-101.25, 0, 84));
	LabelPivot->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector::RightVector, -FVector::ForwardVector).Rotator());
	LabelSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LabelSurface"));
	LabelSurface->SetupAttachment(LabelPivot);
	LabelSurface->SetRelativeScale3D(FVector(.786, 1.179, 1));
	UBotanicusCartonLabelSubsystem::ConfigureSurface(LabelSurface);

	RefreshParcelAppearance(false);
}

void ABotanicusDeliveryParcelActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld() && !IsRunningCookCommandlet())
	{
		const auto& Model = GetDefault<UBotanicusParcelPackagingSettings>()->GetModel(LabelPreviewShape);
		const FVector HalfExtent = Model.LocalHalfExtent();
		const TGuardValue<FBotanicusParcelPackaging> RestorePackaging(Packaging,
			{LabelPreviewShape, HalfExtent * (0.5 * Model.NominalShortSideCm / FMath::Min(HalfExtent.X, HalfExtent.Y)), Model.EmptyMassKg});
		RefreshParcelAppearance(false);
	}
#endif
}

FTransform ABotanicusDeliveryParcelActor::GetLabelLocalTransform() const
{
	return LabelSurface->GetRelativeTransform() * LabelPivot->GetRelativeTransform();
}

void ABotanicusDeliveryParcelActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshParcelLabel();
	if (GetNetMode() != NM_DedicatedServer && !ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		// Load while the carton is still closed, before the player cuts its tape.
		// First-use compilation/resource setup must not delay the disappearance frame.
		const auto* Settings = GetDefault<UBotanicusParcelOpeningSettings>();
		PreloadedConfetti = Settings->ConfettiSystem.LoadSynchronous();
		if (!bOpened) LoadedCuttingTapeSound = Settings->CuttingTapeSound.LoadSynchronous();
#if WITH_EDITOR
		if (PreloadedConfetti) PreloadedConfetti->RequestCompile(false);
#endif
	}
}

void ABotanicusDeliveryParcelActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, Carrier);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, CarriedInspectionOrientation);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, bHasCarriedInspectionOrientation);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, bCarriedPlacementPreview);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, ItemKey);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, Packaging);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, Quantity);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, CutCoverageMask);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, CuttingAudio);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, bOpened);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, OpeningVisualId);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, OpeningStartedAt);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, OpeningDuration);
}

void ABotanicusDeliveryParcelActor::OnRep_ReplicatedMovement()
{
	if (GetCarrier())
		return;
	Super::OnRep_ReplicatedMovement();
	const FRepMovement& Movement = GetReplicatedMovement();
	if (GetNetMode() == NM_Client && Movement.bRepPhysics && Movement.bSimulatedPhysicSleep)
	{
		// Default physics correction tolerates a few cm of error. At sleep, finish
		// at the server's actual resting pose, including any natural overturning.
		// This never moves the authoritative carton or changes its orientation.
		DeliveryBody->SetWorldLocationAndRotation(
			FRepMovement::RebaseOntoLocalOrigin(Movement.Location, this), Movement.Rotation,
			false, nullptr, ETeleportType::TeleportPhysics);
		DeliveryBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
		DeliveryBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		DeliveryBody->PutAllRigidBodiesToSleep();
	}
}

void ABotanicusDeliveryParcelActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHasCarriedInspectionOrientation && IsValid(Carrier) && !LocalParcelPresentation.IsSet() && VisualHomeTransform.IsSet())
	{
		const FQuat Target = CarriedInspectionOrientation.Quaternion() * VisualHomeTransform.GetValue().GetRotation();
		const FQuat Current = SceneRoot->GetComponentQuat();
		if (!Current.Equals(Target, 1.e-4))
			SceneRoot->SetWorldRotation(FQuat::Slerp(Current, Target, 1.f - FMath::Exp(-20.f * DeltaSeconds)));
	}

	UpdateRobotCarryOffset();

	if (HasAuthority() && ActiveCutter.IsValid() && !bOpened)
	{
		const uint16 PreviousCoverage = CutCoverageMask;
		UpdateCutterTrace();
		if (!bOpened && CutCoverageMask != PreviousCoverage)
		{
			RecordCuttingAudioProgress(FPlatformMath::CountBits(CutCoverageMask & ~PreviousCoverage));
		}
	}
	if (GetNetMode() != NM_DedicatedServer && (CuttingAudio.ProgressAt >= 0.0 || CuttingVoice))
	{
		UpdateCuttingAudio(DeltaSeconds);
	}

	if (ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		if (InteractionIndicator)
		{
			InteractionIndicator->SetVisibility(false);
		}
		return;
	}

	// Guidance remains in the existing interaction HUD; the carton stays visible until E succeeds.
	if (InteractionIndicator) InteractionIndicator->SetVisibility(false);
}

FBotanicusInteractionPrompt
ABotanicusDeliveryParcelActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	Prompt.ActionText = bOpened
		? NSLOCTEXT("BotanicusDelivery", "TakeParcelContents", "Deballer")
		: NSLOCTEXT(
			"BotanicusDelivery",
			"InspectClosedParcel",
			"Prendre et inspecter");
	Prompt.bCanInteract = !GetCarrier();
	return Prompt;
}

void ABotanicusDeliveryParcelActor::InitializeParcel(
	FName InItemKey,
	int32 InQuantity)
{
	RestoreParcelState(InItemKey, InQuantity, 0, false);
}

void ABotanicusDeliveryParcelActor::RestoreParcelState(
	FName InItemKey,
	int32 InQuantity,
	uint16 InCutCoverageMask,
	bool bInOpened,
	const FBotanicusParcelPackaging& InPackaging)
{
	if (!HasAuthority())
	{
		return;
	}
	ItemKey = InItemKey;
	Quantity = FMath::Max(1, InQuantity);
	Packaging = InPackaging.IsValid() ? InPackaging : ResolvePackaging(*GetWorld(), ItemKey, Quantity);
	CutCoverageMask = InCutCoverageMask;
	bOpened = bInOpened;
	if (!OpeningVisualId.IsValid()) OpeningVisualId = FGuid::NewGuid();
	OpeningStartedAt = -1; // A restored open carton never replays the burst.
	OpeningDuration = FMath::Clamp(GetDefault<UBotanicusParcelOpeningSettings>()->OpeningDuration, .2f, 1.f);
	bOpeningVisualReplaced = false;
	RefreshParcelAppearance(false);
	ForceNetUpdate();
	// A carton saved in flight resumes gravity at its saved transform.
	// Previews never simulate. New deliveries start immediately.
	if (HasActorBegunPlay() && GetIsReplicated() &&
		!ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
			[this]() { StartDeliveryPhysics(false); }));
	}
}

void ABotanicusDeliveryParcelActor::StartDeliveryPhysics(bool bAddInitialSpin)
{
	if (!HasAuthority() || !GetIsReplicated() ||
		ActorHasTag(TEXT("BotanicusPlacementPreview")) || GetCarrier() || DeliveryBody->IsSimulatingPhysics())
	{
		return;
	}
	SetNetUpdateFrequency(30.0f);
	DeliveryBody->SetSimulatePhysics(true);
	if (bAddInitialSpin)
	{
        bDeliveryImpactArmed=true; DeliveryImpactArmedAt=GetWorld()->GetTimeSeconds();
		DeliveryBody->SetPhysicsAngularVelocityInDegrees(FVector(1.5f, 2.0f, 8.0f));
	}
	DeliveryBody->WakeAllRigidBodies();
	ForceNetUpdate();
}

void ABotanicusDeliveryParcelActor::StopDeliveryPhysics()
{
	if (HasAuthority())
	{
		DeliveryBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
		DeliveryBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		DeliveryBody->SetSimulatePhysics(false);
		ForceNetUpdate();
	}
}

bool ABotanicusDeliveryParcelActor::TryBeginCarry(APawn* Pawn)
{
	if (!HasAuthority() || !IsValid(Pawn) || !Pawn->GetRootComponent() || Carrier || bOpened || ItemKey.IsNone() ||
		(ActiveCutter.IsValid() && ActiveCutter.Get() != Pawn))
		return false;

	EndCutting(Pawn);
	PickupTransform = GetActorTransform();
	StopDeliveryPhysics();
	bHasCarriedInspectionOrientation = false;
	bCarriedPlacementPreview = false;
	Carrier = Pawn;
	OnRep_Carrier(nullptr);
	SetNetDormancy(DORM_Awake);
	FlushNetDormancy();
	ForceNetUpdate();
	return true;
}

void ABotanicusDeliveryParcelActor::EndCarry(const FTransform& DropTransform)
{
	if (!HasAuthority() || !Carrier)
		return;

	const FTransform Destination = DropTransform;
	APawn* PreviousCarrier = Carrier;
	Carrier = nullptr;
	bCarriedPlacementPreview = false;
	SetActorTransform(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	OnRep_Carrier(PreviousCarrier);
	StartDeliveryPhysics(false);
	ForceNetUpdate();
}

void ABotanicusDeliveryParcelActor::SetCarriedInspectionOrientation(const FRotator& WorldRotation)
{
	if (HasAuthority() && Carrier)
	{
		CarriedInspectionOrientation = WorldRotation.GetNormalized();
		bHasCarriedInspectionOrientation = true;
		ForceNetUpdate();
	}
}

void ABotanicusDeliveryParcelActor::SetCarriedPlacementPreview(bool bPreview)
{
	if (HasAuthority() && Carrier && bCarriedPlacementPreview != bPreview)
	{
		bCarriedPlacementPreview = bPreview;
		ForceNetUpdate();
	}
}

void ABotanicusDeliveryParcelActor::OnRep_Carrier(APawn* PreviousCarrier)
{
	if (auto* Previous = Cast<ABotanicusCharacter>(PreviousCarrier))
	{
		Previous->UpdateCarriedParcelForAnimation(this, false);
		RemoveTickPrerequisiteComponent(Previous->GetCharacterMovement());
	}
	if (auto* RobotCarrier = Cast<ABotanicusCharacter>(Carrier))
	{
		RobotCarrier->UpdateCarriedParcelForAnimation(this, true);
		AddTickPrerequisiteComponent(RobotCarrier->GetCharacterMovement());
	}
	if (Carrier)
	{
		bDeliveryImpactArmed = false;
		DeliveryBody->SetSimulatePhysics(false);
	}
	SetActorEnableCollision(Carrier == nullptr);
	RefreshParcelVisualAttachment();
}

const FTransform& ABotanicusDeliveryParcelActor::GetParcelSaveTransform() const
{
	return Carrier ? PickupTransform : GetActorTransform();
}

FTransform ABotanicusDeliveryParcelActor::GetParcelPresentation() const
{
	return FTransform(SceneRoot->GetComponentQuat(), SceneRoot->GetComponentLocation());
}

FVector ABotanicusDeliveryParcelActor::GetLabelNormalInParcelSpace() const
{
	return SceneRoot->GetComponentQuat().UnrotateVector(LabelSurface->GetUpVector());
}

void ABotanicusDeliveryParcelActor::SetLocalParcelPresentation(const FTransform& Pose)
{
	const bool bStartingPresentation = !LocalParcelPresentation.IsSet();
	LocalParcelPresentation = Pose;
	if (bStartingPresentation)
		RefreshParcelVisualAttachment();
	else
		ApplyLocalParcelPresentation();
}

void ABotanicusDeliveryParcelActor::ResetLocalParcelPresentation()
{
	if (LocalParcelPresentation.IsSet())
	{
		LocalParcelPresentation.Reset();
		RefreshParcelVisualAttachment();
	}
}

void ABotanicusDeliveryParcelActor::RefreshParcelVisualAttachment()
{
	const USceneComponent* CarrierRoot = IsValid(Carrier) ? Carrier->GetRootComponent() : nullptr;
	const bool bLocalPresentation = LocalParcelPresentation.IsSet();
	if (!VisualHomeTransform.IsSet())
	{
		if (!CarrierRoot && !bLocalPresentation)
			return;
		VisualHomeTransform = SceneRoot->GetRelativeTransform();
	}

	// Keep the collision body owned by the parcel at the last authoritative
	// position. Only the visible assembly follows the replicated carrier.
	USceneComponent* Parent = CarrierRoot && !bLocalPresentation ? Carrier->GetRootComponent() : DeliveryBody.Get();
	const auto* RobotCarrier = Cast<ABotanicusCharacter>(Carrier);
	const bool bRobotMount = CarrierRoot && !bLocalPresentation && RobotCarrier &&
		RobotCarrier->GetRobotMesh()->GetSkeletalMeshAsset();
	if (bRobotMount)
	{
		// The movement mesh supplies network smoothing. The attachment uses its component, without a bone socket.
		Parent = RobotCarrier->GetMesh();
	}
	if (SceneRoot->GetAttachParent() != Parent)
		SceneRoot->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);

	SceneRoot->SetAbsolute(bLocalPresentation, bLocalPresentation, false);
	if (bLocalPresentation)
	{
		ApplyLocalParcelPresentation();
	}
	else if (CarrierRoot)
	{
		const double Distance = 45.0 + ParcelHalfExtent.Size2D();
		const FVector Scale = GetActorScale3D() * FTransform::GetSafeScaleReciprocal(Parent->GetComponentScale());
		const FQuat Rotation = bRobotMount
			? RobotCarrier->GetBaseRotationOffset().Inverse() * HandGripFrame->GetRelativeRotation().Quaternion().Inverse()
			: FQuat::Identity;
		SceneRoot->SetRelativeTransform(VisualHomeTransform.GetValue() *
			FTransform(Rotation, FVector(Distance, 0, 22), Scale));
		UpdateRobotCarryOffset();
	}
	else
	{
		SceneRoot->SetRelativeTransform(VisualHomeTransform.GetValue());
		VisualHomeTransform.Reset();
	}
}

FTransform ABotanicusDeliveryParcelActor::GetHandGripTransform(bool bLeftHand) const
{
	return (bLeftHand ? LeftHandGrip : RightHandGrip)->GetComponentTransform();
}

void ABotanicusDeliveryParcelActor::RefreshHandGripFrame()
{
	// X points away from the label, matching the starting orientation used by inspection.
	const FVector LabelNormal = GetLabelNormalInParcelSpace();
	const FQuat FrameRotation = LabelNormal.SizeSquared2D() > KINDA_SMALL_NUMBER
		? FRotator(0.0, LabelNormal.Rotation().Yaw + 180.0, 0.0).Quaternion() : FQuat::Identity;
	const FBox ClosedBounds(-ParcelHalfExtent, ParcelHalfExtent);
	const FVector GripExtent = ClosedBounds.TransformBy(FTransform(FrameRotation.Inverse())).GetExtent();
	HandGripFrame->SetRelativeTransform(FTransform(FrameRotation, FVector::ZeroVector, GripExtent / FVector(22.0)));
}

void ABotanicusDeliveryParcelActor::UpdateRobotCarryOffset()
{
	const auto* RobotCarrier = Cast<ABotanicusCharacter>(Carrier);
	if (!IsValid(RobotCarrier) || LocalParcelPresentation.IsSet() || !VisualHomeTransform.IsSet() ||
		SceneRoot->GetAttachParent() != RobotCarrier->GetMesh())
	{
		return;
	}

	const FTransform& Home = VisualHomeTransform.GetValue();
	const FVector InverseParentScale = FTransform::GetSafeScaleReciprocal(RobotCarrier->GetMesh()->GetComponentScale());
	const FVector CarryScale = GetActorScale3D() * InverseParentScale;
	SceneRoot->SetRelativeScale3D(Home.GetScale3D() * CarryScale);

	const FQuat BaseRotation = RobotCarrier->GetBaseRotationOffset();
	const FQuat PawnRotation = RobotCarrier->GetMesh()->GetComponentQuat() * BaseRotation.Inverse();
	const FVector BoxForward = SceneRoot->GetComponentQuat().UnrotateVector(PawnRotation.GetForwardVector());
	const FVector BoxExtent = ParcelHalfExtent * SceneRoot->GetComponentScale().GetAbs();
	const double FrontClearance = FVector::DotProduct(BoxForward.GetAbs(), BoxExtent);
	const FVector PawnSpaceOffset = RobotCarryOffset + FVector(FrontClearance, 0.0, 0.0);
	const FVector ParentSpaceOffset = BaseRotation.UnrotateVector(PawnSpaceOffset) * InverseParentScale
		- BaseRotation.UnrotateVector(RobotCarrier->GetBaseTranslationOffset());
	const FQuat MountRotation = SceneRoot->GetRelativeRotation().Quaternion() * Home.GetRotation().Inverse();
	SceneRoot->SetRelativeLocation(ParentSpaceOffset + MountRotation.RotateVector(Home.GetLocation() * CarryScale));
}

void ABotanicusDeliveryParcelActor::ApplyLocalParcelPresentation()
{
	const FTransform VisualPose = VisualHomeTransform.GetValue() * LocalParcelPresentation.GetValue();
	SceneRoot->SetWorldLocationAndRotation(VisualPose.GetLocation(), VisualPose.GetRotation());
}

bool ABotanicusDeliveryParcelActor::IsParcelHologramMesh(const UMeshComponent* Component) const
{
	return Component == SkeletalMeshVisual || TapeSegments.Contains(Cast<UStaticMeshComponent>(Component));
}

bool ABotanicusDeliveryParcelActor::IsParcelHologramSlot(const UMeshComponent* Component, int32 Slot) const
{
	return Component != SkeletalMeshVisual || Slot != SkeletalMeshVisual->GetMaterialIndex(TEXT("etiquette"));
}

void ABotanicusDeliveryParcelActor::OnDeliveryBodySleep(UPrimitiveComponent* Component, FName BoneName)
{
	if (HasAuthority())
	{
		// Sleeping bodies already suppress unchanged movement in engine replication.
		SetNetUpdateFrequency(2.0f);
		ForceNetUpdate();
	}
}

void ABotanicusDeliveryParcelActor::OnDeliveryBodyWake(UPrimitiveComponent* Component, FName BoneName)
{
	if (HasAuthority())
	{
		SetNetUpdateFrequency(30.0f);
		ForceNetUpdate();
	}
}

void ABotanicusDeliveryParcelActor::BeginCutting(AActor* Interactor)
{
	if (!HasAuthority() || Carrier || bOpened || ActiveCutter.IsValid())
	{
		return;
	}
	ABotanicusCharacter* Character = Cast<ABotanicusCharacter>(Interactor);
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!Character ||
		!QuickBar ||
		!UBotanicusMultiToolComponent::HasTool(Character,TEXT("BoxCutter"),this))
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Selectionnez le cutter pour couper le scotch rouge."));
		return;
	}
	ActiveCutter = Character;
	bHasLastCutSample = false;
	SendInteractorMessage(
		Interactor,
		TEXT(
			"Maintenez le clic gauche et tracez toute la longueur du scotch rouge."));
}

void ABotanicusDeliveryParcelActor::EndCutting(AActor* Interactor)
{
	if (!HasAuthority() ||
		(ActiveCutter.IsValid() && ActiveCutter.Get() != Interactor))
	{
		return;
	}
	ActiveCutter.Reset();
	bHasLastCutSample = false;
	if (CuttingAudio.ActiveWindow > 0.f)
	{
		CuttingAudio = {};
		ForceNetUpdate();
	}
}

double ABotanicusDeliveryParcelActor::GetCuttingAudioTime() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ABotanicusDeliveryParcelActor::RecordCuttingAudioProgress(int32 AddedSegments)
{
	// A segment every 100 ms is the reference speed, in either carton format.
	constexpr float ReferenceSecondsPerSegment = .10f;
	const double Now = GetCuttingAudioTime();
	const double Elapsed = Now - CuttingAudio.ProgressAt;
	const bool bContinuous = CuttingAudio.ProgressAt >= 0.0 && Elapsed > 0.0 &&
		Elapsed <= CuttingAudio.ActiveWindow;
	const float SecondsPerSegment = bContinuous ? static_cast<float>(Elapsed) / AddedSegments : ReferenceSecondsPerSegment;
	CuttingAudio.ProgressAt = Now;
	CuttingAudio.SpeedRatio = FMath::Clamp(ReferenceSecondsPerSegment / SecondsPerSegment, .1f, 4.f);
	// Hold through the gaps in the discrete coverage mask, then fade if it stalls.
	CuttingAudio.ActiveWindow = FMath::Clamp(SecondsPerSegment * 1.65f, .20f, .45f);
	if (!bContinuous) CuttingAudio.ActiveWindow = .32f;
}

void ABotanicusDeliveryParcelActor::UpdateCuttingAudio(float DeltaSeconds)
{
	const double Age = GetCuttingAudioTime() - CuttingAudio.ProgressAt;
	const bool bAdvancing = !bOpened && !Carrier && CuttingAudio.ProgressAt >= 0.0 &&
		Age <= CuttingAudio.ActiveWindow;
	if (!CuttingVoice && (!bAdvancing || !LoadedCuttingTapeSound)) return;
	if (!bAdvancing && bCuttingVoicePaused && !bOpened) return;

	const auto* Settings = GetDefault<UBotanicusParcelOpeningSettings>();
	const float TargetPitch = FMath::Clamp(Settings->CuttingSoundReferencePlayRate * CuttingAudio.SpeedRatio,
		Settings->CuttingSoundMinPlayRate, FMath::Max(Settings->CuttingSoundMinPlayRate, Settings->CuttingSoundMaxPlayRate));
	if (!CuttingVoice)
	{
		CuttingVoice = UGameplayStatics::SpawnSoundAttached(LoadedCuttingTapeSound, GetRootComponent(),
			NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
			.001f, TargetPitch, 0.f, Settings->CartonAudioAttenuation.LoadSynchronous(), nullptr, false);
		if (!CuttingVoice) return;
		CuttingVoicePitch = TargetPitch;
	}
	if (bAdvancing && bCuttingVoicePaused)
	{
		CuttingVoice->SetPaused(false);
		bCuttingVoicePaused = false;
	}
	if (bAdvancing)
	{
		const float Blend = 1.f - FMath::Exp(-Settings->CuttingSoundSpeedSmoothing * DeltaSeconds);
		CuttingVoicePitch = FMath::Lerp(CuttingVoicePitch, TargetPitch, Blend);
		CuttingVoice->SetPitchMultiplier(CuttingVoicePitch);
	}
	CuttingVoiceGain = FMath::FInterpConstantTo(CuttingVoiceGain, bAdvancing ? 1.f : 0.f,
		DeltaSeconds, 1.f / FMath::Max(.001f, Settings->CuttingSoundFadeDuration));
	CuttingVoice->SetVolumeMultiplier(CuttingVoiceGain * Settings->CuttingSoundVolume);
	if (!bAdvancing && CuttingVoiceGain <= 0.f)
	{
		if (bOpened)
		{
			CuttingVoice->DestroyComponent();
			CuttingVoice = nullptr;
		}
		else
		{
			CuttingVoice->SetPaused(true);
			bCuttingVoicePaused = true;
		}
	}
}

void ABotanicusDeliveryParcelActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* RobotCarrier = Cast<ABotanicusCharacter>(Carrier))
	{
		RobotCarrier->UpdateCarriedParcelForAnimation(this, false);
	}
	if (CuttingVoice)
	{
		CuttingVoice->DestroyComponent();
		CuttingVoice = nullptr;
	}
	Super::EndPlay(Reason);
}

void ABotanicusDeliveryParcelActor::UpdateCutterTrace()
{
	ABotanicusCharacter* Character = ActiveCutter.Get();
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	APlayerController* Controller =
		Character
			? Cast<APlayerController>(Character->GetController())
			: nullptr;
	if (!Character ||
		!QuickBar ||
		!Controller ||
		!UBotanicusMultiToolComponent::HasTool(Character,TEXT("BoxCutter"),this) ||
		FVector::DistSquared(
			Character->GetActorLocation(),
			GetActorLocation()) > FMath::Square(450.0f))
	{
		EndCutting(Character);
		return;
	}

	FVector TraceStart;
	FVector TraceDirection;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Controller->GetViewportSize(ViewportWidth, ViewportHeight);
	const bool bHasExactCrosshairRay =
		ViewportWidth > 0 &&
		ViewportHeight > 0 &&
		Controller->DeprojectScreenPositionToWorld(
			ViewportWidth * 0.5f,
			ViewportHeight * 0.5f,
			TraceStart,
			TraceDirection);
	if (!bHasExactCrosshairRay)
	{
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(TraceStart, ViewRotation);
		TraceDirection = ViewRotation.Vector();
	}
	TraceDirection.Normalize();
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusParcelCut),
		false,
		Character);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(
			Hit,
			TraceStart,
			TraceStart + TraceDirection * 700.0f,
			ECC_Visibility,
			QueryParams) ||
		Hit.GetActor() != this)
	{
		bHasLastCutSample = false;
		return;
	}

	const FVector LocalHit =
		GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
	const float TapeHalfLength = ParcelHalfExtent.X * 0.92f;
	if (!IsCuttingAreaHit(Hit))
	{
		bHasLastCutSample = false;
		return;
	}

	const float Normalized =
		FMath::Clamp(
			(LocalHit.X + TapeHalfLength) /
				(TapeHalfLength * 2.0f),
			0.0f,
			0.9999f);
	const int32 SegmentIndex =
		FMath::Clamp(
			FMath::FloorToInt(Normalized * TapeSegmentCount),
			0,
			TapeSegmentCount - 1);
	if (bHasLastCutSample &&
		FMath::Abs(LocalHit.X - LastCutLocalX) <=
			TapeHalfLength * 0.45f)
	{
		const float LastNormalized =
			FMath::Clamp(
				(LastCutLocalX + TapeHalfLength) /
					(TapeHalfLength * 2.0f),
				0.0f,
				0.9999f);
		const int32 LastSegment =
			FMath::Clamp(
				FMath::FloorToInt(
					LastNormalized * TapeSegmentCount),
				0,
				TapeSegmentCount - 1);
		for (int32 Index = FMath::Min(LastSegment, SegmentIndex);
			 Index <= FMath::Max(LastSegment, SegmentIndex);
			 ++Index)
		{
			MarkTapeSegment(Index);
		}
	}
	else
	{
		MarkTapeSegment(SegmentIndex);
	}
	LastCutLocalX = LocalHit.X;
	bHasLastCutSample = true;
}

void ABotanicusDeliveryParcelActor::MarkTapeSegment(
	int32 SegmentIndex)
{
	if (!HasAuthority() ||
		SegmentIndex < 0 ||
		SegmentIndex >= TapeSegmentCount ||
		bOpened)
	{
		return;
	}
	const uint16 SegmentBit =
		static_cast<uint16>(1u << SegmentIndex);
	if ((CutCoverageMask & SegmentBit) != 0)
	{
		return;
	}
	CutCoverageMask |= SegmentBit;
	const bool bReachedStart = (CutCoverageMask & 1u) != 0;
	const bool bReachedEnd =
		(CutCoverageMask & (1u << (TapeSegmentCount - 1))) != 0;
	if (CountCutSegments() >= TapeSegmentCount - 2 &&
		bReachedStart &&
		bReachedEnd)
	{
		ABotanicusCharacter* CutterCharacter = ActiveCutter.Get();
		bOpened = true;
		OpeningStartedAt = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		EndCutting(CutterCharacter);
		SendInteractorMessage(
			CutterCharacter,
			TEXT("Scotch coupe : appuyez sur E pour deballer le carton."));
	}
	RefreshParcelAppearance();
	ForceNetUpdate();
	if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

int32 ABotanicusDeliveryParcelActor::CountCutSegments() const
{
	int32 Count = 0;
	for (int32 SegmentIndex = 0;
		 SegmentIndex < TapeSegmentCount;
		 ++SegmentIndex)
	{
		Count +=
			(CutCoverageMask & (1u << SegmentIndex)) != 0 ? 1 : 0;
	}
	return Count;
}

UClass* ABotanicusDeliveryParcelActor::ResolveParcelClass(const UWorld& World, FName ItemId, int32 Count,
	const FBotanicusParcelPackaging& SavedPackaging)
{
	const EBotanicusParcelShape Shape = SavedPackaging.IsValid() ? SavedPackaging.Shape : ResolvePackaging(World, ItemId, Count).Shape;
	static const TSoftClassPtr<ABotanicusDeliveryParcelActor> Square(FSoftObjectPath(TEXT(
		"/Game/Botanicus/Items/itemsMesh/carton/carton2/Labels/BP_CartonSquare.BP_CartonSquare_C")));
	static const TSoftClassPtr<ABotanicusDeliveryParcelActor> Rectangular(FSoftObjectPath(TEXT(
		"/Game/Botanicus/Items/itemsMesh/carton/carton2/Labels/BP_CartonRectangular.BP_CartonRectangular_C")));
	if (UClass* Variant = (Shape == EBotanicusParcelShape::Rectangular ? Rectangular : Square).LoadSynchronous())
		return Variant;
	return StaticClass();
}

FBotanicusParcelPackaging ABotanicusDeliveryParcelActor::ResolvePackaging(const UWorld& World, FName ItemId, int32 Count)
{
	const UGameInstance* GameInstance = World.GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemId) : nullptr;

	return GetDefault<UBotanicusParcelPackagingSettings>()->Resolve(ItemId, Count, Definition);
}

bool ABotanicusDeliveryParcelActor::ApplyParcelPackaging()
{
	if (!Packaging.IsValid())
		return false;

	const bool bGeometryChanged = !ParcelHalfExtent.Equals(Packaging.HalfExtent);
	ParcelHalfExtent = Packaging.HalfExtent;
	if (!DeliveryBody->GetUnscaledBoxExtent().Equals(ParcelHalfExtent))
		DeliveryBody->SetBoxExtent(ParcelHalfExtent);
	if (!FMath::IsNearlyEqual(AppliedMassKg, Packaging.MassKg))
	{
		AppliedMassKg = Packaging.MassKg;
		DeliveryBody->SetMassOverrideInKg(NAME_None, AppliedMassKg);
	}
	if (AppliedShape == Packaging.Shape && !bGeometryChanged)
		return false;

	const auto& Model = GetDefault<UBotanicusParcelPackagingSettings>()->GetModel(Packaging.Shape);
	USkeletalMesh* NewMesh = Model.Mesh.LoadSynchronous();
	UAnimSequence* NewAnimation = Model.OpeningAnimation.LoadSynchronous();
	AppliedShape = Packaging.Shape;
	if (!NewMesh || !NewAnimation || NewAnimation->GetSkeleton() != NewMesh->GetSkeleton())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid delivery carton mesh/animation pair for %s; using the existing carton."),
			*ItemKey.ToString());
		const USkeletalMesh* FallbackMesh = SkeletalMeshVisual->GetSkeletalMeshAsset();
		if (!FallbackMesh) return false;
		const FBoxSphereBounds Bounds = FallbackMesh->GetBounds();
		const FVector Scale = ParcelHalfExtent / Bounds.BoxExtent.ComponentMax(FVector(0.01));
		SkeletalMeshVisual->SetRelativeTransform(FTransform(FQuat::Identity, -Bounds.Origin * Scale, Scale));
		return true;
	}

	SkeletalMeshVisual->SetSkeletalMesh(NewMesh);
	// Retain the authored cardboard and logo. Only the old sticker overlay is replaced.
	SkeletalMeshVisual->EmptyOverrideMaterials();
	UBotanicusCartonLabelSubsystem::SuppressImportedLabel(SkeletalMeshVisual);
	const FQuat Rotation = FRotator(0, Model.MeshYaw, 0).Quaternion();
	const double Scale = ParcelHalfExtent.X / Model.LocalHalfExtent().X;
	SkeletalMeshVisual->SetRelativeTransform(FTransform(Rotation, -Rotation.RotateVector(Model.ClosedCenter) * Scale, FVector(Scale)));
	OpeningAnimation = NewAnimation;
	return true;
}

void ABotanicusDeliveryParcelActor::RefreshParcelAppearance(bool bPlayOpeningAnimation)
{
	const bool bModelChanged = ApplyParcelPackaging();
	RefreshAnimatedCarton(bPlayOpeningAnimation, bModelChanged);
	RefreshParcelLabel();
	RefreshHandGripFrame();

	const float TapeLength = ParcelHalfExtent.X * 2.0f * 0.92f;
	const float SegmentLength = TapeLength / TapeSegmentCount;
	const float TapeWidth =
		FMath::Clamp(ParcelHalfExtent.Y * 0.24f, 8.0f, 16.0f);
	// The engine cube is 100 cm high. A Z scale of 0.001 produces a
	// 1 mm adhesive strip: visible from above but virtually flat in profile.
	constexpr float TapeThicknessScale = 0.001f;
	constexpr float TapeSurfaceOffset = 0.055f;
	for (int32 SegmentIndex = 0;
		 SegmentIndex < TapeSegments.Num();
		 ++SegmentIndex)
	{
		UStaticMeshComponent* Segment = TapeSegments[SegmentIndex];
		if (!Segment)
		{
			continue;
		}
		Segment->SetRelativeLocation(
			FVector(
				-TapeLength * 0.5f +
					(SegmentIndex + 0.5f) * SegmentLength,
				0.0f,
				ParcelHalfExtent.Z + TapeSurfaceOffset));
		Segment->SetRelativeScale3D(
			FVector(
				SegmentLength / 100.0f,
				TapeWidth / 100.0f,
				TapeThicknessScale));
		Segment->SetVisibility(
			!bOpened &&
			(CutCoverageMask & (1u << SegmentIndex)) == 0);
	}

	const FVector FlapScale(
		ParcelHalfExtent.X * 0.0196f,
		ParcelHalfExtent.Y * 0.0094f,
		0.018f);
	if (LeftFlap)
	{
		LeftFlap->SetRelativeLocation(
			FVector(
				0.0f,
				-ParcelHalfExtent.Y * 0.52f,
				ParcelHalfExtent.Z + 5.0f));
		LeftFlap->SetRelativeScale3D(FlapScale);
		LeftFlap->SetRelativeRotation(FRotator(-28.0f, 0.0f, 0.0f));
		LeftFlap->SetVisibility(false);
	}
	if (RightFlap)
	{
		RightFlap->SetRelativeLocation(
			FVector(
				0.0f,
				ParcelHalfExtent.Y * 0.52f,
				ParcelHalfExtent.Z + 5.0f));
		RightFlap->SetRelativeScale3D(FlapScale);
		RightFlap->SetRelativeRotation(FRotator(28.0f, 0.0f, 0.0f));
		RightFlap->SetVisibility(false);
	}
	if (InteractionIndicator)
	{
		InteractionIndicator->SetRelativeLocation(
			FVector(0.0f, 0.0f, ParcelHalfExtent.Z + 70.0f));
	}
}

void ABotanicusDeliveryParcelActor::RefreshParcelLabel()
{
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer || ActorHasTag(TEXT("BotanicusPlacementPreview")))
		return;
#if WITH_EDITORONLY_DATA
	if (!GetWorld()->IsGameWorld())
	{
		UBotanicusCartonLabelSubsystem::Apply(LabelSurface, LabelPreviewItemId, 0);
		return;
	}
#endif
	if (ItemKey != LastLabelItemId || Quantity != LastLabelQuantity)
	{
		LastLabelItemId = ItemKey;
		LastLabelQuantity = Quantity;
		UBotanicusCartonLabelSubsystem::Apply(LabelSurface, ItemKey, Quantity);
	}
}

void ABotanicusDeliveryParcelActor::RefreshAnimatedCarton(
	bool bPlayOpeningAnimation, bool bModelChanged)
{
	if (!SkeletalMeshVisual || !SkeletalMeshVisual->GetSkeletalMeshAsset())
	{
		return;
	}

	const bool bShowCarton = !bOpeningVisualReplaced;
	SkeletalMeshVisual->SetVisibility(bShowCarton);
	SkeletalMeshVisual->SetHiddenInGame(!bShowCarton, true);

	if (!OpeningAnimation)
	{
		bLastRenderedOpenedState = bOpened;
		return;
	}

	const bool bJustOpened = bOpened && (!bLastRenderedOpenedState || bModelChanged);
	if (bOpened)
	{
		if (bJustOpened)
		{
			SkeletalMeshVisual->PlayAnimation(OpeningAnimation, false);
			SkeletalMeshVisual->SetPlayRate(OpeningAnimation->GetPlayLength() /
				(OpeningDuration * FMath::Max(.01f, FMath::Abs(OpeningAnimation->RateScale))));
			if (OpeningStartedAt >= 0 && GetWorld() && GetWorld()->GetGameState())
			{
				const double Phase = FMath::Clamp((GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - OpeningStartedAt) / OpeningDuration, 0.0, 1.0);
				SkeletalMeshVisual->SetPosition(OpeningAnimation->GetPlayLength() * Phase, false);
			}
			if (!bPlayOpeningAnimation || OpeningStartedAt < 0)
			{
				SkeletalMeshVisual->SetPosition(
					OpeningAnimation->GetPlayLength(),
					false);
				SkeletalMeshVisual->SetPlayRate(0.0f);
			}
		}
	}
	else if (bModelChanged || bLastRenderedOpenedState || !Packaging.IsValid())
	{
		SkeletalMeshVisual->PlayAnimation(OpeningAnimation, false);
		SkeletalMeshVisual->SetPosition(0.0f, false);
		SkeletalMeshVisual->SetPlayRate(0.0f);
	}
	bLastRenderedOpenedState = bOpened;
}

void ABotanicusDeliveryParcelActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!HasAuthority() || !bInteractionEnabled)
	{
		return;
	}
	if (!bOpened)
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"Le carton est ferme : selectionnez le cutter et coupez tout le scotch rouge."));
		return;
	}
	if (ReleaseContents(Interactor))
	{
		// Only a successful authoritative transfer creates the independent cosmetic.
		ABotanicusParcelOpeningVisualActor::Start(this);
		SetInteractionEnabled(false);
		Destroy();
	}
}

bool ABotanicusDeliveryParcelActor::ReleaseContents(
	AActor* Interactor)
{
	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Character || !Definition)
	{
		return false;
	}

	if (Definition->WeightClass ==
		EBotanicusItemWeightClass::Hotbar)
	{
		UBotanicusQuickBarComponent* QuickBar =
			Character->GetQuickBarComponent();
		int32 AddedSlotIndex = INDEX_NONE;
		if (!QuickBar ||
			!QuickBar->AddItem(ItemKey, Quantity, AddedSlotIndex))
		{
			SendInteractorMessage(
				Interactor,
				TEXT(
					"Hotbar pleine : liberez de la place avant de deballer."));
			return false;
		}
		UBotanicusAudioWorldSubsystem::ConfirmPickup(Character, this);
		return true;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation =
		GetActorLocation() +
		GetActorRightVector() * (ParcelHalfExtent.Y + 65.0f) +
		FVector(0.0f, 0.0f, 35.0f);
	UClass* ContentClass =
		Definition->WorldActorClass.LoadSynchronous();
	if (Definition->WeightClass ==
		EBotanicusItemWeightClass::Handheld)
	{
		if (!ContentClass ||
			!ContentClass->IsChildOf(
				ABotanicusPlaceableItemActor::StaticClass()))
		{
			return false;
		}
		ABotanicusPlaceableItemActor* Item =
			GetWorld()->SpawnActor<ABotanicusPlaceableItemActor>(
				ContentClass,
				SpawnLocation,
				GetActorRotation(),
				SpawnParameters);
		if (!Item)
		{
			return false;
		}
		Item->InitializePlacedItem(ItemKey, Quantity);
		return true;
	}

	if (!ContentClass ||
		!ContentClass->IsChildOf(
			ABotanicusLargeEquipmentActor::StaticClass()))
	{
		ContentClass =
			ABotanicusLargeEquipmentActor::StaticClass();
	}
	ABotanicusLargeEquipmentActor* Equipment =
		GetWorld()->SpawnActor<ABotanicusLargeEquipmentActor>(
			ContentClass,
			SpawnLocation,
			GetActorRotation(),
			SpawnParameters);
	if (!Equipment)
	{
		return false;
	}
	Equipment->InitializeEquipment(ItemKey);
	return true;
}

void ABotanicusDeliveryParcelActor::SendInteractorMessage(
	AActor* Interactor,
	const FString& Message) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller)
	{
		Controller->ClientMessage(Message);
	}
}

void ABotanicusDeliveryParcelActor::OnRep_ParcelState()
{
	// Either actor channel may arrive first. Match the cosmetic snapshot by identity.
	for (TActorIterator<ABotanicusParcelOpeningVisualActor> It(GetWorld()); It; ++It)
		if (OpeningVisualId.IsValid() && It->GetSourceId() == OpeningVisualId) HandOffOpeningVisual();
	RefreshParcelAppearance();
}

void ABotanicusDeliveryParcelActor::HandOffOpeningVisual()
{
	if (bOpeningVisualReplaced) return;
	bOpeningVisualReplaced = true;
	SkeletalMeshVisual->SetHiddenInGame(true, true);
	SkeletalMeshVisual->SetVisibility(false);
	SkeletalMeshVisual->SetComponentTickEnabled(false);
	for (UStaticMeshComponent* Segment : TapeSegments) Segment->SetVisibility(false);
}

void ABotanicusDeliveryParcelActor::OnDeliveryHit(UPrimitiveComponent* Component,AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit)
{
    if (!HasAuthority() || !bDeliveryImpactArmed || GetWorld()->GetTimeSeconds()-DeliveryImpactArmedAt<.15) return;
    const float ImpactSpeed=NormalImpulse.Size()/FMath::Max(1.f,DeliveryBody->GetMass());
    if (ImpactSpeed<MinimumDeliveryImpactSpeed) return;
    bDeliveryImpactArmed=false;
    const auto* State=GetWorld()->GetGameState();
    MulticastDeliveryImpact(Hit.ImpactPoint,FMath::FRandRange(.98f,1.02f),State ? State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds());
}
void ABotanicusDeliveryParcelActor::MulticastDeliveryImpact_Implementation(FVector_NetQuantize Position,float Pitch,double StartedAt)
{
    if(GetNetMode()==NM_DedicatedServer)return;
    const auto* State=GetWorld()->GetGameState();
    if(State && State->GetServerWorldTimeSeconds()-StartedAt>.5)return;
    if(auto* Audio=GetWorld()->GetSubsystem<UBotanicusAudioWorldSubsystem>())
        if(auto* Voice=Audio->PlaySpatialTransition(this,DeliveryImpactSound.LoadSynchronous(),Position,DeliveryImpactVolume,0,DeliveryImpactAttenuation.LoadSynchronous(),2500))
            Voice->SetPitchMultiplier(Pitch);
}

bool ABotanicusDeliveryParcelActor::IsCuttingAreaHit(const FHitResult& Hit) const
{
    if(bOpened || Hit.GetActor()!=this)return false;
    const FVector P=GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
    const float HalfLength=ParcelHalfExtent.X*.92f;
    const float HalfWidth=FMath::Clamp(ParcelHalfExtent.Y*.24f,8.f,16.f)*.5f+1.f;
    return FMath::Abs(P.X)<=HalfLength+4 && FMath::Abs(P.Y)<=HalfWidth && FMath::Abs(P.Z-ParcelHalfExtent.Z)<=18;
}

bool ABotanicusDeliveryParcelActor::IsBeingCutBy(const AActor* User) const {return !bOpened && ActiveCutter.Get()==User;}
