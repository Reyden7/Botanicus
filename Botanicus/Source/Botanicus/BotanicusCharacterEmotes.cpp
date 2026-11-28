#include "BotanicusCharacter.h"
#include "TimerManager.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"

#include "Animation/AnimSequence.h"
#include "BotanicusPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Presentation/BotanicusEmoteMotion.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"

namespace
{
	constexpr double RequestInterval = .12;
	constexpr float MovementThreshold = 10.f;
}

const USkeleton* ABotanicusCharacter::GetRobotEmoteSkeleton() const
{
	const USkeletalMesh* MeshAsset = RobotMesh->GetSkeletalMeshAsset();
	return MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
}

bool ABotanicusCharacter::IsRobotEmotePlaying() const
{
	return !EmoteState.EmoteId.IsNone() &&
		(!PredictedEmoteStop.IsSet() || *PredictedEmoteStop != EmoteState.Revision);
}

double ABotanicusCharacter::GetRobotEmoteElapsedTime() const
{
	const auto Source = GetCharacterMovement()->GetRootMotionSource(BotanicusEmoteMotion::SourceName);
	if (Source && Source->GetScriptStruct() == FBotanicusEmoteMotionSource::StaticStruct() &&
		static_cast<const FBotanicusEmoteMotionSource&>(*Source).Revision == EmoteState.Revision)
	{
		return Source->GetTime();
	}
	return FMath::Max(0., GetRobotActionTime() - EmoteState.StartedAt);
}

void ABotanicusCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (IsRobotEmotePlaying())
		return;
	if (EmoteEndControlYaw.IsSet())
	{
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(*EmoteEndControlYaw, NewControlRotation.Yaw)) < .1)
			return;
		EmoteEndControlYaw.Reset();
	}
	Super::FaceRotation(NewControlRotation, DeltaTime);
}

void ABotanicusCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (!IsRobotEmotePlaying())
		Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void ABotanicusCharacter::BeginRobotEmote()
{
	auto* Move = GetCharacterMovement();
	Move->RemoveRootMotionSource(BotanicusEmoteMotion::SourceName);
	EmoteEndControlYaw.Reset();
	ConsumeMovementInputVector();
	StopJumping();
	Move->StopMovementImmediately();

	// Replication and the local listen server use the same one-shot playback path.
	StopRobotEmoteAudio();
	PlayRobotEmoteAudio();

	const FBotanicusEmoteEntry* Entry = EmoteCatalog ? EmoteCatalog->Find(EmoteState.EmoteId) : nullptr;
	const UAnimSequence* Sequence = Entry ? EmoteCatalog->GetMotionSequence(*Entry) : nullptr;
	if (!Sequence)
		return;

	auto Source = MakeShared<FBotanicusEmoteMotionSource>();
	Source->Sequence = Sequence;
	Source->Revision = EmoteState.Revision;
	Source->InitialYaw = EmoteState.InitialYaw;
	Source->PlayRate = Entry->GetEffectivePlayRate();
	Source->Duration = Entry->bLoop ? -1.f : Sequence->GetPlayLength() / Source->PlayRate;
	Source->SetTime(FMath::Max(0., GetRobotActionTime() - EmoteState.StartedAt));
	Move->ApplyRootMotionSource(Source);
}

void ABotanicusCharacter::ReleaseRobotEmoteControls()
{
	auto* Move = GetCharacterMovement();
	StopRobotEmoteAudio();
	if (GetLocalRole() != ROLE_SimulatedProxy)
	{
		Move->StopMovementImmediately();
		// Movement can process a jump before removing the source on the same tick.
		if (const auto Source = Move->GetRootMotionSource(BotanicusEmoteMotion::SourceName))
			Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
		if (GetController())
			EmoteEndControlYaw = GetControlRotation().Yaw;
	}
	Move->RemoveRootMotionSource(BotanicusEmoteMotion::SourceName);
}

void ABotanicusCharacter::OnRep_EmoteState(const FBotanicusEmoteState& PreviousState)
{
	const bool bAlreadyReleased = PredictedEmoteStop.IsSet() && *PredictedEmoteStop == PreviousState.Revision;
	PredictedEmoteStop.Reset();
	if (IsRobotEmotePlaying())
		BeginRobotEmote();
	else if (!PreviousState.EmoteId.IsNone() && !bAlreadyReleased)
		ReleaseRobotEmoteControls();
}

void ABotanicusCharacter::RequestRobotEmote(FName EmoteId)
{
	if (IsLocallyControlled())
		ServerPlayRobotEmote(EmoteId, FRotator::CompressAxisToShort(GetActorRotation().Yaw));
}

void ABotanicusCharacter::ServerPlayRobotEmote_Implementation(FName EmoteId, uint16 RequestedYaw)
{
	const double Now = GetRobotActionTime();
	if (Now - LastEmoteRequest < RequestInterval || EmoteState.EmoteId == EmoteId)
		return;
	LastEmoteRequest = Now;

	auto* PC = Cast<ABotanicusPlayerController>(GetController());
	const FBotanicusEmoteEntry* Entry = EmoteCatalog ? EmoteCatalog->Find(EmoteId) : nullptr;
	if (!PC || !Entry || !Entry->IsPlayable(GetRobotEmoteSkeleton()) || !PC->CanUseRobotEmotes(true))
		return;

	const auto* Move = GetCharacterMovement();
	const bool bReplacing = IsRobotEmotePlaying();
	if (!Move->IsMovingOnGround() || bIsCrouched || bPressedJump ||
		(!bReplacing && (Move->Velocity.SizeSquared2D() > FMath::Square(MovementThreshold) ||
			Move->GetCurrentAcceleration().SizeSquared2D() > FMath::Square(MovementThreshold))))
		return;

	// The reliable request can arrive before the latest movement packet carrying the new view yaw.
	// Use the owner's body heading at selection, with the normal network yaw precision.
	if (!IsLocallyControlled())
		SetActorRotation(FRotator(0., FRotator::DecompressAxisFromShort(RequestedYaw), 0.));
	EmoteState = {EmoteId, Now, static_cast<uint16>(EmoteState.Revision + 1), static_cast<float>(GetActorRotation().Yaw)};
	if (IsSprinting())
		SetSprinting(false);
	BeginRobotEmote();
	ForceNetUpdate();
}

void ABotanicusCharacter::StopRobotEmote()
{
	if (HasAuthority() && !EmoteState.EmoteId.IsNone())
	{
		EmoteState.EmoteId = NAME_None;
		ReleaseRobotEmoteControls();
		ForceNetUpdate();
	}
}

void ABotanicusCharacter::ServerCancelRobotEmote_Implementation(uint16 Revision)
{
	if (Revision == EmoteState.Revision)
		StopRobotEmote();
}

void ABotanicusCharacter::InterruptLocalRobotEmote()
{
	if (IsLocallyControlled() && IsRobotEmotePlaying())
	{
		if (!HasAuthority())
		{
			PredictedEmoteStop = EmoteState.Revision;
			ReleaseRobotEmoteControls();
		}
		ServerCancelRobotEmote(EmoteState.Revision);
	}
}

void ABotanicusCharacter::PlayRobotEmoteAudio()
{
	if (GetNetMode() == NM_DedicatedServer || !IsRobotEmotePlaying() || !EmoteCatalog || !EmoteCatalog->EmoteAttenuation)
		return;

	const FBotanicusEmoteEntry* Entry = EmoteCatalog->Find(EmoteState.EmoteId);
	if (!Entry || !Entry->Sound || Entry->Sound->IsLooping())
		return;

	const double Elapsed = GetRobotActionTime() - EmoteState.StartedAt - FMath::Max(0.f, Entry->SoundDelay);
	// Keep the previous cue's fade intact when a new emote arrives before it finishes.
	const float Wait = FMath::Max(static_cast<float>(-Elapsed), RobotEmoteAudio->IsPlaying() ? .025f : 0.f);
	if (Wait > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(RobotEmoteAudioTimer, this, &ABotanicusCharacter::PlayRobotEmoteAudio,
			Wait, false);
		return;
	}

	const float Pitch = FMath::Clamp(Entry->SoundPitch, .125f, 4.f);
	const float Offset = FMath::Max(0., Elapsed) * Pitch * Entry->Sound->GetPitchMultiplier();
	if (Offset >= Entry->Sound->GetDuration())
		return;

	RobotEmoteAudioFadeOut = FMath::Clamp(Entry->SoundFadeOut, .15f, .25f);
	RobotEmoteAudio->SetAttenuationSettings(EmoteCatalog->EmoteAttenuation);
	RobotEmoteAudio->SetSound(Entry->Sound);
	RobotEmoteAudio->SetVolumeMultiplier(FMath::Max(0.f, Entry->SoundVolume));
	RobotEmoteAudio->SetPitchMultiplier(Pitch);
	RobotEmoteAudio->Play(Offset);
}

void ABotanicusCharacter::StopRobotEmoteAudio()
{
	GetWorldTimerManager().ClearTimer(RobotEmoteAudioTimer);
	if (RobotEmoteAudio->IsPlaying() && RobotEmoteAudio->GetPlayState() != EAudioComponentPlayState::FadingOut)
		RobotEmoteAudio->FadeOut(RobotEmoteAudioFadeOut, 0.f);
}

void ABotanicusCharacter::ResetRobotEmoteAudio()
{
	GetWorldTimerManager().ClearTimer(RobotEmoteAudioTimer);
	RobotEmoteAudio->Stop();
}

void ABotanicusCharacter::UpdateRobotEmote()
{
	const FBotanicusEmoteEntry* Entry = EmoteCatalog && IsRobotEmotePlaying() ? EmoteCatalog->Find(EmoteState.EmoteId) : nullptr;
	auto* PC = Cast<ABotanicusPlayerController>(GetController());
	if (Entry && (HasAuthority() || IsLocallyControlled()) && (!PC || !PC->CanUseRobotEmotes(false)))
	{
		if (HasAuthority())
			StopRobotEmote();
		else
			InterruptLocalRobotEmote();
	}
	else if (Entry && !Entry->bLoop && (HasAuthority() || IsLocallyControlled()))
	{
		const double Duration = Entry->Animation->GetPlayLength() / Entry->GetEffectivePlayRate();
		if (GetRobotEmoteElapsedTime() >= Duration)
		{
			if (HasAuthority())
				StopRobotEmote();
			else
			{
				PredictedEmoteStop = EmoteState.Revision;
				ReleaseRobotEmoteControls();
			}
		}
	}

	if (IsLocallyControlled())
	{
		AActor* HiddenItem = nullptr;
		if (ShouldHideEmoteHeldItems())
		{
			HiddenItem = GetHeldWateringCan();
			if (!HiddenItem && PC)
				HiddenItem = PC->GetCheckoutScanner();
		}
		UpdateHeldItemVisibility(HiddenItem);
	}
}
