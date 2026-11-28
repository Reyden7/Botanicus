#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "BotanicusEmoteMotion.generated.h"

class UAnimSequence;

namespace BotanicusEmoteMotion
{
	extern const FName SourceName;
	FTransform ProjectPlanar(const FTransform& First, const FTransform& Pose);
}

USTRUCT()
struct FBotanicusEmoteMotionSource : public FRootMotionSource
{
	GENERATED_BODY()

	FBotanicusEmoteMotionSource();

	UPROPERTY()
	TObjectPtr<const UAnimSequence> Sequence;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	uint16 Revision = 0;

	UPROPERTY()
	float InitialYaw = 0.f;

	virtual FRootMotionSource* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool Matches(const FRootMotionSource* Other) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character,
		const UCharacterMovementComponent& MoveComponent) override;
};

template<>
struct TStructOpsTypeTraits<FBotanicusEmoteMotionSource> : TStructOpsTypeTraitsBase2<FBotanicusEmoteMotionSource>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

UCLASS()
class UBotanicusEmoteMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
	virtual bool CanCrouchInCurrentState() const override;
};
