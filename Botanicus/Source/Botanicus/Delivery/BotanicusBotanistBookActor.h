#pragma once

#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "BotanicusBotanistBookActor.generated.h"

class ABotanicusPlayerController;
class UAnimSequence;
class UBoxComponent;

UENUM()
enum class EBotanistBookPhase : uint8
{
	Closed, Opening, Opened, Closing
};

USTRUCT()
struct FBotanistBookPresentation
{
	GENERATED_BODY()

	UPROPERTY() EBotanistBookPhase Phase = EBotanistBookPhase::Closed;
	UPROPERTY() double StartedAt = 0.;
	UPROPERTY() FTransform PickupStart = FTransform::Identity;
	UPROPERTY() double PickupStartedAt = -1.;
};

USTRUCT()
struct FBotanistBookInitialPlacement
{
	GENERATED_BODY()

	UPROPERTY() bool bConfirmed = false;
	UPROPERTY() bool bHasSavedTransform = false;
	UPROPERTY() FTransform SavedTransform = FTransform::Identity;
};

UCLASS()
class BOTANICUS_API ABotanicusBotanistBookActor : public ABotanicusPlaceableItemActor
{
	GENERATED_BODY()

public:
	ABotanicusBotanistBookActor();
	static FName BookKey();
	static FGuid BookId();
	static UClass* ResolveBookClass();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnRep_Owner() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FVector GetPlacementBoxExtent() const override;
	virtual float GetPlacementPivotToBottomOffset() const override;
	virtual FBotanicusInteractionPrompt GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book") TObjectPtr<UAnimSequence> BookOpenAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book") TObjectPtr<UAnimSequence> BookCloseAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book", meta=(ClampMin="0.01")) float BookOpenPlayRate = 2.f;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book", meta=(ClampMin="0.01")) float BookClosePlayRate = 2.f;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book", meta=(ClampMin="1", Units="cm")) float BookInteractionRange = 250.f;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held") FVector HeldBookRelativeLocation = FVector(54, 13, -13);
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held") FRotator HeldBookRelativeRotation = FRotator(0, 90, 55);
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held") FVector HeldBookRelativeScale = FVector::OneVector;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held") FTransform RemoteBookHandTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Book|Held", meta=(DisplayName="Closed Book Right Hand Grip"))
	TObjectPtr<USceneComponent> ClosedRightHandGrip;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held")
	FVector ClosedFirstPersonLocation = FVector(54, 25, -25);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held")
	FRotator ClosedFirstPersonRotation = FRotator(80, 90, 0);

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Reading")
	float ReadingTilt = 30.f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Reading", meta=(ClampMin="0.01", Units="s"))
	float ReadingBlendIn = .10f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Reading", meta=(ClampMin="0.01", Units="s"))
	float ReadingBlendOut = .12f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Reading", meta=(ClampMin="0.01", Units="s"))
	float SecondHandBlendTime = .08f;

	/** The comfortable downward look applied as the robot settles into reading. */
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Reading", meta=(ClampMin="0", ClampMax="45", Units="deg"))
	float ReadingHeadPitch = 24.f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Book|Held")
	FBotanicusHeldItemSettings OpenGripSettings;

	virtual bool UsesDedicatedHeldPresentation() const override { return true; }
	bool IsReadingPose() const { return Presentation.Phase != EBotanistBookPhase::Closed; }
	float GetReadingPoseWeight(float EnterSeconds, float ExitSeconds) const;
	FVector GetReadingLookTarget() const;
	bool IsHeldPresentation() const { return bHeldPresentation; }
	bool IsAvailable() const;
	bool BeginReading(ABotanicusPlayerController& Reader);
	bool RequestClose(ABotanicusPlayerController& Reader);
	void AbandonReading(ABotanicusPlayerController& Reader);
	void SetHeldPresentation(APawn& Pawn, bool bAnimatePickup = false);
	void ConfirmInitialPlacement(const FTransform* SavedTransform);
	FBox GetBookLocalBounds() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true")) TObjectPtr<USceneComponent> BookWorldVisualPivot;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UBoxComponent> BookCollision;
	UPROPERTY(ReplicatedUsing=OnRep_InitialPlacement) FBotanistBookInitialPlacement InitialPlacement;
	UPROPERTY(ReplicatedUsing=OnRep_HeldPresentation) bool bHeldPresentation = false;
	UPROPERTY(ReplicatedUsing=OnRep_Presentation) FBotanistBookPresentation Presentation;
	TWeakObjectPtr<ABotanicusPlayerController> ActiveReader;
	FTimerHandle TransitionTimer;
	bool bBlendingPickup = false;
	bool bPickupPresentationInitialized = false;

	UFUNCTION() void OnRep_Presentation();
	UFUNCTION() void OnRep_HeldPresentation();
	UFUNCTION() void OnRep_InitialPlacement();
	void SetPhase(EBotanistBookPhase Phase);
	void RefreshHeldGrip();
	void FinishTransition();
	void RefreshCollision();
	float GetBookPlayRate(bool bOpening) const;
	double ServerTime() const;
};
