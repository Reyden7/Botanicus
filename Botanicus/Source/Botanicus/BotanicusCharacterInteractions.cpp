#include "BotanicusCharacter.h"

#include "GameFramework/GameStateBase.h"
#include "QuickBar/BotanicusQuickBarComponent.h"

double ABotanicusCharacter::GetRobotActionTime() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

FName ABotanicusCharacter::GetRobotActionItemKey() const
{
	return HasAuthority() || IsLocallyControlled()
		? GetQuickBarComponent()->GetSelectedSlot().ItemKey : GetQuickBarComponent()->GetObservedEquippedItem();
}

void ABotanicusCharacter::PlayRobotAction(EBotanicusRobotAction Action, const FVector& Target, FName ItemKey)
{
	if (HasAuthority())
		MulticastRobotAction(Action, Target, GetRobotActionTime(), ItemKey);
}

void ABotanicusCharacter::MulticastRobotAction_Implementation(EBotanicusRobotAction Action, FVector_NetQuantize10 Target,
	double StartedAt, FName ItemKey)
{
	ReceiveRobotAction(Action, Target, StartedAt, ItemKey);
}

void ABotanicusCharacter::ReceiveRobotAction(EBotanicusRobotAction Action, const FVector& Target, double StartedAt, FName ItemKey)
{
	PendingRobotAction = {Action, {}, Target, StartedAt, ItemKey, true, false};
}

void ABotanicusCharacter::SetRobotActionState(EBotanicusRobotAction Action, const UObject* Source,
	bool bEnabled, const FVector& Target, bool bHasTarget)
{
	if (bEnabled)
	{
		FBotanicusRobotActionCue& Cue = ActiveRobotActions.FindOrAdd(Action);
		if (Cue.Source.Get() != Source)
			Cue = {Action, Source, Target, GetRobotActionTime(), NAME_None, bHasTarget, true};
		Cue.Target = Target;
		Cue.bHasTarget = bHasTarget;
	}
	else if (const FBotanicusRobotActionCue* Current = ActiveRobotActions.Find(Action);
		Current && Current->Source.Get() == Source)
	{
		ActiveRobotActions.Remove(Action);
	}
}

FBotanicusRobotActionCue ABotanicusCharacter::GetRobotActionState(EBotanicusRobotAction Action) const
{
	const FBotanicusRobotActionCue* Cue = ActiveRobotActions.Find(Action);
	return Cue && Cue->Source.IsValid() ? *Cue : FBotanicusRobotActionCue();
}

