#pragma once

#include "CoreMinimal.h"
#include "BotanicusRobotInteraction.generated.h"

class UAnimMontage;
class UCurveFloat;

UENUM(BlueprintType)
enum class EBotanicusRobotAction : uint8
{
	None,
	Plant,
	Water,
	PourSoil,
	Harvest,
	CutTape,
	Whip,
	Scan,
	ScanConfirm,
	ReadBook,
	CollectCoins,
	ReceiveCoins
};

USTRUCT(BlueprintType)
struct FBotanicusRobotActionProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05", Units="s"))
	float Duration = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.01", Units="s"))
	float BlendIn = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.01", Units="s"))
	float BlendOut = 0.10f;

	/** Optional one-shot envelope over normalized time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing")
	TObjectPtr<UCurveFloat> MotionCurve;

	/** Optional upper-body animation driven by the same gameplay cue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> Montage;

	/** Use the montage for arm motion when present. Contact strength is still controlled by IKInfluence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(EditCondition="Montage != nullptr"))
	bool bUseMontageInsteadOfMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(Units="cm"))
	FVector LocalHandOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(ClampMin="0", Units="cm"))
	float ReachTowardTarget = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact")
	FRotator LocalWristRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool")
	FRotator ObjectTilt = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool", meta=(ClampMin="0", ClampMax="1"))
	float ToolAimInfluence = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(ClampMin="0", ClampMax="1"))
	float IKInfluence = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(Units="cm"))
	FVector ElbowOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta=(ClampMin="0", ClampMax="1"))
	float LookInfluence = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool", meta=(ClampMin="0", Units="cm"))
	float SecondaryMotion = 0.f;
};

struct FBotanicusRobotActionCue
{
	EBotanicusRobotAction Action = EBotanicusRobotAction::None;
	TWeakObjectPtr<const UObject> Source;
	FVector Target = FVector::ZeroVector;
	double StartedAt = 0.0;
	FName ItemKey;
	bool bHasTarget = false;
	bool bContinuous = false;
};

struct FBotanicusRobotToolMotion
{
	FVector Offset = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

