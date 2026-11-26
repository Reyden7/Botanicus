#pragma once

#include "CoreMinimal.h"
#include "BotanicusHeldItemProfile.generated.h"

UENUM(BlueprintType)
enum class EBotanicusHeldItemProfile : uint8
{
	None,
	RightHand,
	LeftHand,
	TwoHands,
	TwoHandsLarge
};

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusHeldItemSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
	EBotanicusHeldItemProfile Profile = EBotanicusHeldItemProfile::None;

	/** Position of the active contact (or midpoint between contacts) in the carrier's frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip", meta=(Units="cm"))
	FVector CarryRelativeLocation = FVector(27.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
	FRotator CarryRelativeRotation = FRotator::ZeroRotator;

	/** Read the hand components instead of fitting contacts to the dimensions of the rendered mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
	bool bUseAuthoredGrips = false;

	/** Optionally angle the tool with the existing aim pitch, within this range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip", meta=(ClampMin="0", ClampMax="90", Units="deg"))
	float AimPitchLimit = 0.0f;

	/** Interpret CarryRelativeRotation in the rendered mesh frame instead of the actor frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
	bool bAlignToMesh = false;

	/** Keep a one-handed object clear of the hip laterally as its volume tilts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
	bool bCarryAlongside = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip", meta=(ClampMin="0", Units="cm"))
	float CarrySwayMaxOffset = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip", meta=(ClampMin="0", Units="deg"))
	float CarrySwayMaxAngle = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip", meta=(ClampMin="1"))
	float CarrySwayDamping = 24.0f;
};

struct FBotanicusHeldItemPose
{
	TOptional<FTransform> Left;
	TOptional<FTransform> Right;
	FVector LeftElbowOffset = FVector::ZeroVector;
	FVector RightElbowOffset = FVector::ZeroVector;
	float LeftInfluence = 1.f;
	float RightInfluence = 1.f;
	float LeftLayerInfluence = 1.f;
	float RightLayerInfluence = 1.f;

	bool IsSet() const { return Left.IsSet() || Right.IsSet(); }
};

