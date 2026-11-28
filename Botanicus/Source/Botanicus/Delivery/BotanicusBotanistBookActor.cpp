#include "Delivery/BotanicusBotanistBookActor.h"
#include "Presentation/BotanicusHeldItemComponent.h"
#include "BotanicusCharacter.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "BotanicusPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BookPickupSeconds = 0.25f;
}

ABotanicusBotanistBookActor::ABotanicusBotanistBookActor()
{
	BookWorldVisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BookWorldVisualPivot"));
	BookWorldVisualPivot->SetupAttachment(SceneRoot);
	SkeletalMeshVisual->SetupAttachment(BookWorldVisualPivot);
	HeldItemGrip->GripSettings = {EBotanicusHeldItemProfile::RightHand, FVector(24, 21, -4), FRotator(84, 90, 0), true};
	HeldItemGrip->GripSettings.bAlignToMesh = true;
	HeldItemGrip->GripSettings.CarrySwayMaxOffset = 3.5f;
	HeldItemGrip->GripSettings.CarrySwayMaxAngle = 6.f;
	OpenGripSettings = {EBotanicusHeldItemProfile::TwoHands, FVector(31, 0, 12), FRotator(0, 90, 0), true};

	ClosedRightHandGrip = CreateDefaultSubobject<USceneComponent>(TEXT("ClosedRightHandGrip"));
	ClosedRightHandGrip->SetupAttachment(SkeletalMeshVisual, TEXT("droite"));
	LeftHandGrip->SetupAttachment(SkeletalMeshVisual, TEXT("gauche"));
	RightHandGrip->SetupAttachment(SkeletalMeshVisual, TEXT("droite"));

	// Last pose of Open and Close in Carnet_Test. The right cover keeps the same pose in both.
	const FTransform RightCover(FQuat(.50052, -.49948, -.50052, -.49948),
		FVector(.11450, 1.41132, .37752), FVector(4.65096));
	const FTransform LeftCover(FQuat(-.50005, -.49996, .50099, -.49900),
		FVector(-.10254, 1.41135, 1.33685), FVector(4.65096));
	const FQuat ClosedRotation = HeldItemGrip->GripSettings.CarryRelativeRotation.Quaternion();
	const FQuat OpenRotation = (OpenGripSettings.CarryRelativeRotation + FRotator(0, 0, ReadingTilt)).Quaternion();
	const FQuat RightWrist = FRotator(-56.43, 148.97, -58.95).Quaternion();
	const FQuat LeftWrist = FRotator(60.05, 46.32, -79.23).Quaternion();
	auto SetCoverContact = [](USceneComponent& Grip, const FTransform& Cover, const FVector& Contact,
		const FQuat& CarryRotation, const FQuat& Wrist)
	{
		Grip.SetRelativeLocationAndRotation(Cover.InverseTransformPosition(Contact),
			((CarryRotation * Cover.GetRotation()).Inverse() * Wrist).Rotator());
		Grip.bEditableWhenInherited = true;
	};
	// The left cover unfolds toward -X, the right cover stays on +X. Contacts sit under the edges.
	SetCoverContact(*ClosedRightHandGrip, RightCover, FVector(8.6, 2.2, .58), ClosedRotation, RightWrist);
	SetCoverContact(*RightHandGrip, RightCover, FVector(8.65, 3.8, .45), OpenRotation, RightWrist);
	SetCoverContact(*LeftHandGrip, LeftCover, FVector(-8.65, 3.8, .75), OpenRotation, LeftWrist);
	bUseBlueprintAppearance = true;
	bUseSkeletalMeshAppearance = true;
	Mesh->SetVisibility(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshVisual->SetVisibility(true);
	SkeletalMeshVisual->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BookMesh(
		TEXT("/Game/Botanicus/Items/livreDeBotanist/Carnet_Test.Carnet_Test"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Open(
		TEXT("/Game/Botanicus/Items/livreDeBotanist/open.open"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Close(
		TEXT("/Game/Botanicus/Items/livreDeBotanist/close.close"));
	SkeletalMeshVisual->SetSkeletalMeshAsset(BookMesh.Object);
	BookOpenAnimation = Open.Object;
	BookCloseAnimation = Close.Object;

	BookCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BookCollision"));
	BookCollision->SetupAttachment(SceneRoot);
	BookCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BookCollision->SetCanEverAffectNavigation(false);
	BookCollision->SetGenerateOverlapEvents(false);
}

FName ABotanicusBotanistBookActor::BookKey()
{
	return TEXT("BotanistBook");
}

FGuid ABotanicusBotanistBookActor::BookId()
{
	return FGuid(0x6F101630, 0x6240A812, 0x945FF23D, 0x925BF441);
}

UClass* ABotanicusBotanistBookActor::ResolveBookClass()
{
	static const TSoftClassPtr<ABotanicusBotanistBookActor> Blueprint(FSoftObjectPath(
		TEXT("/Game/Botanicus/Items/livreDeBotanist/BP_Item_BotanistBook.BP_Item_BotanistBook_C")));
	if (UClass* Class = Blueprint.LoadSynchronous())
		return Class;
	return StaticClass();
}

FBox ABotanicusBotanistBookActor::GetBookLocalBounds() const
{
	if (const USkeletalMesh* Asset = SkeletalMeshVisual->GetSkeletalMeshAsset())
		return Asset->GetBounds().GetBox().TransformBy(SkeletalMeshVisual->GetRelativeTransform() * BookWorldVisualPivot->GetRelativeTransform());
	return FBox(FVector(-15, -12, -2), FVector(15, 12, 2));
}

void ABotanicusBotanistBookActor::RefreshCollision()
{
	const FBox Bounds = GetBookLocalBounds();
	BookCollision->SetRelativeLocation(Bounds.GetCenter());
	BookCollision->SetBoxExtent(Bounds.GetExtent().ComponentMax(FVector(1.)));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector ABotanicusBotanistBookActor::GetPlacementBoxExtent() const
{
	return GetBookLocalBounds().GetExtent() * GetActorScale3D().GetAbs();
}

float ABotanicusBotanistBookActor::GetPlacementPivotToBottomOffset() const
{
	return static_cast<float>(-GetBookLocalBounds().Min.Z * GetActorScale3D().Z);
}

void ABotanicusBotanistBookActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshCollision();
	OnRep_Presentation();
}

void ABotanicusBotanistBookActor::BeginPlay()
{
	Super::BeginPlay();
	InitializePlacedItem(BookKey());
	RefreshCollision();
	OnRep_InitialPlacement();
	OnRep_Presentation();
	OnRep_HeldPresentation();
}

void ABotanicusBotanistBookActor::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(TransitionTimer);
	if (auto* Reader = ActiveReader.Get(); HasAuthority() && Reader && Reason == EEndPlayReason::Destroyed)
		Reader->FinishBotanistBookSession(this);
	Super::EndPlay(Reason);
}

void ABotanicusBotanistBookActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusBotanistBookActor, InitialPlacement);
	DOREPLIFETIME(ABotanicusBotanistBookActor, Presentation);
	DOREPLIFETIME(ABotanicusBotanistBookActor, bHeldPresentation);
}

void ABotanicusBotanistBookActor::ConfirmInitialPlacement(const FTransform* SavedTransform)
{
	check(HasAuthority());
	SetReplicates(true);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	InitialPlacement.bConfirmed = true;
	InitialPlacement.bHasSavedTransform = SavedTransform != nullptr;
	if (SavedTransform)
	{
		InitialPlacement.SavedTransform = *SavedTransform;
		SetActorTransform(*SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	ForceNetUpdate();
}

void ABotanicusBotanistBookActor::OnRep_InitialPlacement()
{
	if (HasAuthority() || !IsNetStartupActor())
	{
		return;
	}
	// Wait for the server's choice before showing the level copy on a client.
	if (InitialPlacement.bConfirmed && InitialPlacement.bHasSavedTransform && !bHeldPresentation && !GetOwner())
	{
		SetActorTransform(InitialPlacement.SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	SetActorHiddenInGame(!InitialPlacement.bConfirmed);
	SetActorEnableCollision(InitialPlacement.bConfirmed && !bHeldPresentation);
}

double ABotanicusBotanistBookActor::ServerTime() const
{
	const auto* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.);
}

bool ABotanicusBotanistBookActor::IsAvailable() const
{
	return Presentation.Phase == EBotanistBookPhase::Closed && !ActiveReader.IsValid() && !IsActorBeingDestroyed();
}

bool ABotanicusBotanistBookActor::CanInteract_Implementation(AActor* Interactor) const
{
	return !bHeldPresentation && !IsActorBeingDestroyed();
}

FBotanicusInteractionPrompt ABotanicusBotanistBookActor::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	auto Prompt = Super::GetInteractionPrompt_Implementation(Interactor);
	Prompt.TargetName = NSLOCTEXT("BotanistBook", "Name", "Carnet du botaniste");
	Prompt.ActionText = IsAvailable()
		? NSLOCTEXT("BotanistBook", "Prompt", "E : ramasser")
		: NSLOCTEXT("BotanistBook", "Busy", "Carnet en cours de consultation");
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	return Prompt;
}

bool ABotanicusBotanistBookActor::BeginReading(ABotanicusPlayerController& Reader)
{
	if (!HasAuthority() || !IsAvailable() || !BookOpenAnimation || !BookCloseAnimation)
		return false;
	ActiveReader = &Reader;
	SetPhase(EBotanistBookPhase::Opening);
	return true;
}

bool ABotanicusBotanistBookActor::RequestClose(ABotanicusPlayerController& Reader)
{
	if (!HasAuthority() || ActiveReader.Get() != &Reader || Presentation.Phase != EBotanistBookPhase::Opened)
		return false;
	SetPhase(EBotanistBookPhase::Closing);
	return true;
}

void ABotanicusBotanistBookActor::AbandonReading(ABotanicusPlayerController& Reader)
{
	if (!HasAuthority() || ActiveReader.Get() != &Reader)
		return;
	ActiveReader.Reset();
	if (Presentation.Phase == EBotanistBookPhase::Opened)
		SetPhase(EBotanistBookPhase::Closing);
}

float ABotanicusBotanistBookActor::GetBookPlayRate(bool bOpening) const
{
	return FMath::Max(0.01f, bOpening ? BookOpenPlayRate : BookClosePlayRate);
}

void ABotanicusBotanistBookActor::SetPhase(EBotanistBookPhase Phase)
{
	Presentation.Phase = Phase;
	Presentation.StartedAt = ServerTime();
	OnRep_Presentation();
	FlushNetDormancy();
	ForceNetUpdate();
	const bool bOpening = Phase == EBotanistBookPhase::Opening;
	if (bOpening || Phase == EBotanistBookPhase::Closing)
	{
		const auto* Sequence = bOpening ? BookOpenAnimation.Get() : BookCloseAnimation.Get();
		const float Duration = Sequence ? Sequence->GetPlayLength() / (FMath::Max(0.01f, Sequence->RateScale) * GetBookPlayRate(bOpening)) : 0.f;
		if (Duration > 0.f)
			GetWorldTimerManager().SetTimer(TransitionTimer, this, &ABotanicusBotanistBookActor::FinishTransition, Duration);
		else
			FinishTransition();
	}
}

void ABotanicusBotanistBookActor::FinishTransition()
{
	if (Presentation.Phase == EBotanistBookPhase::Opening)
	{
		SetPhase(EBotanistBookPhase::Opened);
		if (auto* Reader = ActiveReader.Get())
			Reader->ShowBotanistBookSession(this);
		else
			SetPhase(EBotanistBookPhase::Closing);
	}
	else if (Presentation.Phase == EBotanistBookPhase::Closing)
	{
		SetPhase(EBotanistBookPhase::Closed);
		if (auto* Reader = ActiveReader.Get())
			Reader->FinishBotanistBookSession(this);
		ActiveReader.Reset();
	}
}

void ABotanicusBotanistBookActor::OnRep_Presentation()
{
	if (GetNetMode() == NM_DedicatedServer || !SkeletalMeshVisual)
		return;
	if (!bPickupPresentationInitialized && Presentation.PickupStartedAt >= 0.)
	{
		bPickupPresentationInitialized = true;
		bBlendingPickup = ServerTime() - Presentation.PickupStartedAt < BookPickupSeconds;
		if (bBlendingPickup)
		{
			SkeletalMeshVisual->SetWorldTransform(Presentation.PickupStart);
			OnRep_HeldPresentation();
		}
	}
	const bool bOpenSequence = Presentation.Phase == EBotanistBookPhase::Opening ||
		Presentation.Phase == EBotanistBookPhase::Opened;
	auto* Sequence = bOpenSequence ? BookOpenAnimation.Get() : BookCloseAnimation.Get();
	if (!Sequence)
		return;
	const bool bInTransition = Presentation.Phase == EBotanistBookPhase::Opening ||
		Presentation.Phase == EBotanistBookPhase::Closing;
	const float Length = Sequence->GetPlayLength();
	const float PlayRate = GetBookPlayRate(bOpenSequence);
	const float EffectivePlayRate = FMath::Max(0.01f, Sequence->RateScale) * PlayRate;
	const float Position = bInTransition
		? FMath::Clamp(static_cast<float>(ServerTime() - Presentation.StartedAt) * EffectivePlayRate, 0.f, Length)
		: Length;
	SkeletalMeshVisual->PlayAnimation(Sequence, false);
	SkeletalMeshVisual->SetPlayRate(PlayRate);
	SkeletalMeshVisual->SetPosition(Position, false);
	if (UAnimSingleNodeInstance* Player = SkeletalMeshVisual->GetSingleNodeInstance())
	{
		Player->SetPlaying(bInTransition && Position < Length);
	}
	RefreshHeldGrip();
}

void ABotanicusBotanistBookActor::SetHeldPresentation(APawn& Pawn, bool bAnimatePickup)
{
	check(HasAuthority());
	if (bAnimatePickup)
	{
		Presentation.PickupStart = SkeletalMeshVisual->GetComponentTransform();
		Presentation.PickupStartedAt = ServerTime();
	}
	bHeldPresentation = true;
	Tags.AddUnique(TEXT("BotanicusEquippedPreview"));
	SetOwner(&Pawn);
	if (auto* Character = Cast<ACharacter>(&Pawn))
		AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
	else
		AttachToActor(&Pawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetRootComponent()->SetRelativeTransform(RemoteBookHandTransform);
	OnRep_HeldPresentation();
	if (bAnimatePickup)
	{
		OnRep_Presentation();
	}
	ForceNetUpdate();
}

void ABotanicusBotanistBookActor::OnRep_Owner()
{
	Super::OnRep_Owner();
	OnRep_HeldPresentation();
}

void ABotanicusBotanistBookActor::OnRep_HeldPresentation()
{
	if (bHeldPresentation)
	{
		SetActorEnableCollision(false);
		BookCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BookWorldVisualPivot->SetRelativeTransform(FTransform::Identity);
		// Older instances of BP_Item_BotanistBook inherit the visual on SceneRoot.
		// The book and the animated cover grips must move together with the held pivot.
		if (SkeletalMeshVisual->GetAttachParent() != BookWorldVisualPivot)
			SkeletalMeshVisual->AttachToComponent(BookWorldVisualPivot, FAttachmentTransformRules::KeepRelativeTransform);
	}
	RefreshHeldGrip();
	const auto* Pawn = Cast<APawn>(GetOwner());
	SetActorTickEnabled(bHeldPresentation && GetNetMode() != NM_DedicatedServer &&
		(bBlendingPickup || !Pawn || Pawn->IsLocallyControlled() || Pawn->GetLocalRole() == ROLE_AutonomousProxy));
}

void ABotanicusBotanistBookActor::RefreshHeldGrip()
{
	auto* Character = bHeldPresentation && !bBlendingPickup ? Cast<ABotanicusCharacter>(GetOwner()) : nullptr;
	HeldItemGrip->Bind(Character, *BookWorldVisualPivot, *SkeletalMeshVisual, LeftHandGrip, RightHandGrip,
		EBotanicusHeldItemSource::Book, HeldItemGrip->GripSettings);
}

float ABotanicusBotanistBookActor::GetReadingPoseWeight(float EnterSeconds, float ExitSeconds) const
{
	const float Age = FMath::Max(0.f, static_cast<float>(ServerTime() - Presentation.StartedAt));
	switch (Presentation.Phase)
	{
	case EBotanistBookPhase::Opening:
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Age / FMath::Max(.01f, EnterSeconds), 0.f, 1.f));
	case EBotanistBookPhase::Opened:
		return 1.f;
	case EBotanistBookPhase::Closing:
		if (BookCloseAnimation)
		{
			const float Duration = BookCloseAnimation->GetPlayLength() /
				(FMath::Max(.01f, BookCloseAnimation->RateScale) * GetBookPlayRate(false));
			return FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Duration - Age) / FMath::Max(.01f, ExitSeconds), 0.f, 1.f));
		}
		[[fallthrough]];
	default:
		return 0.f;
	}
}

FVector ABotanicusBotanistBookActor::GetReadingLookTarget() const
{
	return (LeftHandGrip->GetComponentLocation() + RightHandGrip->GetComponentLocation()) * .5;
}

void ABotanicusBotanistBookActor::Tick(float DeltaTime)
{
	if (!bHeldPresentation)
	{
		Super::Tick(DeltaTime);
		return;
	}
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
		return;
	const auto* PC = Cast<ABotanicusPlayerController>(Pawn->GetController());
	const bool bLocal = PC && PC->IsLocalPlayerController();
	if (!PC && Pawn->GetLocalRole() == ROLE_AutonomousProxy)
		return;

	const auto* Defaults = GetClass()->GetDefaultObject<ABotanicusBotanistBookActor>();
	const FTransform VisualOffset = Defaults->SkeletalMeshVisual->GetRelativeTransform();
	if (!bLocal && !bBlendingPickup)
	{
		SkeletalMeshVisual->SetRelativeTransform(VisualOffset);
		RefreshHeldGrip();
		SetActorTickEnabled(false);
		return;
	}

	FTransform Target = VisualOffset * GetActorTransform();
	if (bLocal)
	{
		const auto* Character = Cast<ABotanicusCharacter>(Pawn);
		SkeletalMeshVisual->SetVisibility(!PC->IsPlacingBotanistBook() && (!Character ||
			(!Character->IsUsingOrderComputer() && !Character->IsRobotEmotePlaying())));
		FVector Eye;
		FRotator View;
		PC->GetPlayerViewPoint(Eye, View);
		const FTransform Transport(ClosedFirstPersonRotation, ClosedFirstPersonLocation, HeldBookRelativeScale);
		const FTransform Reading(HeldBookRelativeRotation, HeldBookRelativeLocation, HeldBookRelativeScale);
		FTransform Held;
		Held.Blend(Transport, Reading, GetReadingPoseWeight(ReadingBlendIn, ReadingBlendOut));
		Target = VisualOffset * Held * FTransform(View, Eye);
	}
	if (bBlendingPickup)
	{
		const float Alpha = FMath::Clamp(static_cast<float>(ServerTime() - Presentation.PickupStartedAt) / BookPickupSeconds, 0.f, 1.f);
		FTransform Blended;
		Blended.Blend(Presentation.PickupStart, Target, FMath::SmoothStep(0.f, 1.f, Alpha));
		Target = Blended;
		bBlendingPickup = Alpha < 1.f;
	}
	SkeletalMeshVisual->SetWorldTransform(Target);
}

