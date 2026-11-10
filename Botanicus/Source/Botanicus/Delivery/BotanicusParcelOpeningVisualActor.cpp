#include "Delivery/BotanicusParcelOpeningVisualActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusParcelOpeningSettings.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"

namespace {
double ParcelVisualClock(const UWorld* World)
{
	const auto* State = World->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}
}

ABotanicusParcelOpeningVisualActor::ABotanicusParcelOpeningVisualActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30);
	PrimaryActorTick.bCanEverTick = true;
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Opening Visual Root"));
	SetRootComponent(VisualRoot);
	CartonVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Opening Carton Visual"));
	CartonVisual->SetupAttachment(VisualRoot);
	CartonVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CartonVisual->SetHiddenInGame(true);
	SetActorEnableCollision(false);
}

ABotanicusParcelOpeningVisualActor* ABotanicusParcelOpeningVisualActor::Start(ABotanicusDeliveryParcelActor* Parcel)
{
	if (!IsValid(Parcel) || !Parcel->HasAuthority()) return nullptr;
	UWorld* World = Parcel->GetWorld();
	const FTransform Anchor = Parcel->GetActorTransform();
	auto* Visual = World->SpawnActorDeferred<ABotanicusParcelOpeningVisualActor>(StaticClass(), Anchor,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Visual) return nullptr;
	const auto* Settings = GetDefault<UBotanicusParcelOpeningSettings>();
	auto& Data = Visual->Presentation;
	Data.SourceId = Parcel->GetOpeningVisualId();
	Data.Mesh = Parcel->SkeletalMeshVisual->GetSkeletalMeshAsset();
	Data.Animation = Parcel->GetOpeningAnimation();
	Data.MeshLocalTransform = Parcel->SkeletalMeshVisual->GetRelativeTransform();
	Data.HalfExtent = Parcel->GetParcelHalfExtent() * Anchor.GetScale3D().GetAbs();
	for (auto* Material : Parcel->SkeletalMeshVisual->GetMaterials()) Data.Materials.Add(Material);
	// E may arrive long after the flaps finished, or on a restored open carton.
	// Finish any remaining animation, then burst now; never age the FX from an old cut.
	const double Now = ParcelVisualClock(World);
	Data.StartedAt = FMath::Max(Parcel->GetOpeningStartedAt(), Now - Parcel->GetOpeningDuration());
	Data.Duration = Parcel->GetOpeningDuration();
	Data.Lifetime = FMath::Clamp(Settings->ConfettiLifetime, .8f, 1.2f);
	Data.Count = FMath::Clamp(Settings->ConfettiCount, 1, 50);
	Data.bReady = true;
	Visual->Source = Parcel;
	Visual->FinishSpawning(Anchor);
	// Keep a distinct replicated actor alive long enough for its initial snapshot.
	// Parcel destruction is immediate after a successful inventory transaction.
	Visual->SetLifeSpan(FMath::Max(.1, Data.StartedAt + Data.Duration - ParcelVisualClock(World)) + Data.Lifetime + 1.0);
	return Visual;
}

void ABotanicusParcelOpeningVisualActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABotanicusParcelOpeningVisualActor, Presentation, COND_InitialOnly);
}

void ABotanicusParcelOpeningVisualActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeLocalPresentation();
}

void ABotanicusParcelOpeningVisualActor::OnRep_Presentation()
{
	if (HasActorBegunPlay()) InitializeLocalPresentation();
}

void ABotanicusParcelOpeningVisualActor::HideLogicalSource()
{
	for (TActorIterator<ABotanicusDeliveryParcelActor> It(GetWorld()); It; ++It)
		if (It->GetOpeningVisualId() == Presentation.SourceId) It->HandOffOpeningVisual();
}

void ABotanicusParcelOpeningVisualActor::InitializeLocalPresentation()
{
	if (bInitialized || !Presentation.bReady) return;
	bInitialized = true;
	HideLogicalSource();
	if (GetNetMode() == NM_DedicatedServer) return;
	LoadedSystem = GetDefault<UBotanicusParcelOpeningSettings>()->ConfettiSystem.LoadSynchronous();
	// Prepare real particles behind a zero-opacity material during the opening.
	// Revealing an existing renderer and hiding the carton then share one frame.
	if (LoadedSystem)
	{
		if (UMaterialInterface* Base = GetDefault<UBotanicusParcelOpeningSettings>()->ConfettiMaterial.LoadSynchronous())
		{
			ConfettiMaterial = UMaterialInstanceDynamic::Create(Base, this);
			ConfettiMaterial->SetScalarParameterValue(TEXT("Reveal"), 0);
		}
		Confetti = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), LoadedSystem,
			GetActorLocation(), GetActorRotation(), FVector::OneVector, false, false, ENCPoolMethod::None, false);
		if (Confetti)
		{
			Confetti->SetCastShadow(false);
			Confetti->SetVariableMaterial(TEXT("User.PaperMaterial"), ConfettiMaterial);
			Confetti->SetVariableInt(TEXT("User.BurstCount"), Presentation.Count);
			Confetti->SetVariableFloat(TEXT("User.Lifetime"), Presentation.Lifetime);
			Confetti->SetVariableFloat(TEXT("User.Radius"), Presentation.HalfExtent.GetMin() * .85f);
			Confetti->SetVariableFloat(TEXT("User.Size"), 6.f * FMath::Clamp(Presentation.HalfExtent.Size()/35.f,.85f,1.6f));
			const FVector LocalUp = GetActorTransform().InverseTransformVectorNoScale(FVector(0,0,200));
			Confetti->SetVariableVec3(TEXT("User.VelocityMin"), LocalUp - FVector(140,140,60));
			Confetti->SetVariableVec3(TEXT("User.VelocityMax"), LocalUp + FVector(140,140,60));
			Confetti->SetVariableVec3(TEXT("User.Gravity"), GetActorTransform().InverseTransformVectorNoScale(FVector(0,0,-220)));
		}
		PrepareConfetti();
	}
	CartonVisual->SetSkeletalMesh(Presentation.Mesh);
	CartonVisual->SetRelativeTransform(Presentation.MeshLocalTransform);
	for (int32 I=0; I<Presentation.Materials.Num(); ++I) CartonVisual->SetMaterial(I, Presentation.Materials[I]);
	CartonVisual->SetHiddenInGame(false);
	if (Presentation.Animation)
	{
		CartonVisual->PlayAnimation(Presentation.Animation, false);
		CartonVisual->SetPlayRate(Presentation.Animation->GetPlayLength() /
			(FMath::Max(.1f, Presentation.Duration) * FMath::Max(.01f, FMath::Abs(Presentation.Animation->RateScale))));
		const double Phase = FMath::Clamp((ParcelVisualClock(GetWorld()) - Presentation.StartedAt) / Presentation.Duration, 0.0, 1.0);
		CartonVisual->SetPosition(Presentation.Animation->GetPlayLength()*Phase, false);
	}
	if (ParcelVisualClock(GetWorld()) >= Presentation.StartedAt + Presentation.Duration)
		BurstAndHide(ParcelVisualClock(GetWorld()));
}

void ABotanicusParcelOpeningVisualActor::PrepareConfetti()
{
	if (bConfettiPrepared || !IsValid(Confetti) || !LoadedSystem || !ConfettiMaterial) return;
#if WITH_EDITOR
	LoadedSystem->PollForCompilationComplete();
#endif
	if (!LoadedSystem->IsReadyToRun()) return;
	if (!bConfettiActivationRequested)
	{
		Confetti->GetOverrideParameters().Tick();
		Confetti->Activate(true);
		bConfettiActivationRequested = true;
		ConfettiPreparedFrame = GFrameCounter;
		return;
	}
	const auto Controller = Confetti->GetSystemInstanceController();
	// Let real world ticks initialise the renderer/material bindings while Reveal
	// is zero. Pausing immediately after a manual VM tick leaves those unprepared.
	if (!Controller || Controller->GetAge() <= 0 || GFrameCounter <= ConfettiPreparedFrame + 1) return;
	Confetti->SetPaused(true);
	bConfettiPrepared = true;
	ConfettiPreparedFrame = GFrameCounter;
}

void ABotanicusParcelOpeningVisualActor::BurstAndHide(double Now)
{
	if (bBurstStarted) return;
	PrepareConfetti();
	// Allow the prepared render state to reach the next frames before the atomic
	// reveal. Inventory has already been transferred; only the cosmetic copy waits.
	if (IsValid(Confetti) && ConfettiMaterial &&
		(!bConfettiPrepared || GFrameCounter <= ConfettiPreparedFrame + 1)) return;
	const float EffectAge = FMath::Max(0.0, Now - Presentation.StartedAt - Presentation.Duration);
	const auto* Settings = GetDefault<UBotanicusParcelOpeningSettings>();
	const auto* PC = GetWorld()->GetFirstPlayerController();
	FVector Eye = GetActorLocation(); FRotator Rotation;
	if (PC) PC->GetPlayerViewPoint(Eye, Rotation);
	int32 Active = 0;
	for (TActorIterator<ABotanicusParcelOpeningVisualActor> It(GetWorld()); It; ++It)
		if (*It != this && It->bBurstStarted && IsValid(It->Confetti)) ++Active;
	if (IsValid(Confetti) && ConfettiMaterial && bConfettiPrepared && EffectAge < Presentation.Lifetime &&
		Active < Settings->MaximumSimultaneousBursts && FVector::DistSquared(Eye, GetActorLocation()) <= FMath::Square(Settings->DisplayDistance))
	{
		// The dedicated emitter uses local coordinates while paused: moved or tilted
		// cartons carry their prepared paper volume to the exact final transform.
		Confetti->SetWorldLocationAndRotation(GetActorLocation(), GetActorRotation());
		Confetti->SetVariableVec3(TEXT("User.Gravity"), GetActorTransform().InverseTransformVectorNoScale(FVector(0,0,-220)));
		Confetti->GetOverrideParameters().Tick();
		Confetti->SetPaused(false);
		if (EffectAge > 1.f/60.f) Confetti->AdvanceSimulation(FMath::FloorToInt(EffectAge*60.f),1.f/60.f);
		ConfettiMaterial->SetScalarParameterValue(TEXT("Reveal"), 1);
		if (USoundBase* Sound = Settings->OptionalPopSound.LoadSynchronous())
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
	else if (IsValid(Confetti)) { Confetti->DestroyComponent(); Confetti = nullptr; }
	bBurstStarted = true;
	HideLogicalSource();
	if (Presentation.Animation)
	{
		CartonVisual->SetPosition(Presentation.Animation->GetPlayLength(), false);
		CartonVisual->RefreshBoneTransforms();
	}
	CartonVisual->SetHiddenInGame(true);
	CartonVisual->SetVisibility(false);
	CartonVisual->SetComponentTickEnabled(false);
	if (HasAuthority()) { SetNetUpdateFrequency(2); ForceNetUpdate(); }
}

void ABotanicusParcelOpeningVisualActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Presentation.bReady) return;
	InitializeLocalPresentation();
	PrepareConfetti();
	const double Now = ParcelVisualClock(GetWorld());
	if (!bBurstStarted && HasAuthority() && Source.IsValid()) SetActorTransform(Source->GetActorTransform());
	if (Now >= Presentation.StartedAt + Presentation.Duration) BurstAndHide(Now);
	if (Now >= Presentation.StartedAt + Presentation.Duration + Presentation.Lifetime + .15)
	{
		if (IsValid(Confetti)) Confetti->DestroyComponent();
		Confetti = nullptr;
		SetActorTickEnabled(false);
	}
}

void ABotanicusParcelOpeningVisualActor::EndPlay(const EEndPlayReason::Type Reason)
{
	// No burst here: actor destruction must never create a second explosion.
	if (IsValid(Confetti)) Confetti->DestroyComponent();
	Super::EndPlay(Reason);
}
