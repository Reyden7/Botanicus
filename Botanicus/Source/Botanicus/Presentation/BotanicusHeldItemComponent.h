#pragma once

#include "Components/ActorComponent.h"
#include "Presentation/BotanicusHeldItemProfile.h"
#include "Presentation/BotanicusRobotInteraction.h"
#include "BotanicusHeldItemComponent.generated.h"

class ABotanicusCharacter;
class UPrimitiveComponent;
class USceneComponent;

enum class EBotanicusHeldItemSource : uint8
{
	QuickBar,
	MultiTool,
	Book,
	WateringCan,
	Scanner
};

/** Remote presentation of an equipped item and its two optional IK contacts. */
UCLASS()
class BOTANICUS_API UBotanicusHeldItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Held Item")
	FBotanicusHeldItemSettings GripSettings;

	/** Use this actor's settings instead of the catalogue, including an explicit None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Botanicus|Held Item")
	bool bOverrideCatalogueGrip = false;

	FBotanicusHeldItemSettings ResolveSettings(const FBotanicusHeldItemSettings& Catalogue) const;
	void Bind(ABotanicusCharacter* Character, USceneComponent& InMount, UPrimitiveComponent& InGeometry,
		USceneComponent* InLeftGrip, USceneComponent* InRightGrip, EBotanicusHeldItemSource InSource,
		const FBotanicusHeldItemSettings& Settings);
	void Unbind();
	void SetSettings(const FBotanicusHeldItemSettings& Settings);
	FBotanicusHeldItemPose UpdateMount(float DeltaSeconds);
	void SetToolMotion(const FBotanicusRobotToolMotion& Motion) { ToolMotion = Motion; }
	EBotanicusHeldItemSource GetSource() const { return Source; }
	bool HasMount() const { return Carrier.IsValid() && Mount.IsValid()
		&& ActiveSettings.Profile != EBotanicusHeldItemProfile::None; }
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	TWeakObjectPtr<ABotanicusCharacter> Carrier;
	TWeakObjectPtr<USceneComponent> Mount;
	TWeakObjectPtr<UPrimitiveComponent> Geometry;
	TWeakObjectPtr<USceneComponent> LeftGrip;
	TWeakObjectPtr<USceneComponent> RightGrip;
	EBotanicusHeldItemSource Source = EBotanicusHeldItemSource::QuickBar;
	FBotanicusHeldItemSettings ActiveSettings;
	FBotanicusRobotToolMotion ToolMotion;
	FTransform OriginalRelativeTransform;
	TOptional<FVector> PreviousVelocity;
	FVector SwayResponse = FVector::ZeroVector;
};

