// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presentation/BotanicusEmoteCatalog.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Presentation/BotanicusHeldItemProfile.h"
#include "Presentation/BotanicusRobotInteraction.h"
#include "Engine/NetSerialization.h"
#include "BotanicusCharacter.generated.h"

class UInputComponent;
class UAudioComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class USpringArmComponent;
enum class EVisibilityBasedAnimTickOption : uint8;
class USpotLightComponent;
class ABotanicusAmbienceDirector;
class UInputAction;
class UBotanicusInteractionComponent;
class UBotanicusQuickBarComponent;
class UBotanicusMultiToolComponent;
class ABotanicusWateringCanActor;
class ABotanicusDeliveryParcelActor;
class ABotanicusComputerActor;
class ABotanicusPlaceableItemActor;
class UBotanicusHeldItemComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UENUM(BlueprintType)
enum class EBotanicusEquipmentCarryRole : uint8
{
	None,
	Solo,
	Primary,
	Helper
};

/**
 *  A basic first person character
 */
UCLASS(abstract)
class ABotanicusCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** World-space robot used by other viewers. Configure its mesh and transform in the player Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> RobotMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Emotes|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> EmoteCameraPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Emotes|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> EmoteSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Emotes|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> EmoteCamera;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(ClampMin="80", Units="cm"))
	float EmoteCameraDistance = 300.f;

	/** Height of the target measured from the centre of the standing capsule. */
	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(Units="cm"))
	float EmoteCameraHeightOffset = -16.f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(Units="cm"))
	float EmoteCameraHorizontalOffset = 0.f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera")
	FRotator EmoteCameraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(ClampMin="40", ClampMax="110", Units="deg"))
	float EmoteCameraFOV = 70.f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(ClampMin="0", Units="s"))
	float EmoteCameraBlendInDuration = .35f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(ClampMin="0", Units="s"))
	float EmoteCameraBlendOutDuration = .20f;

	UPROPERTY(EditAnywhere, Category="Botanicus|Emotes|Camera", meta=(DisplayName="Emote Camera Collision Enabled"))
	bool bEmoteCameraCollisionEnabled = true;

	struct FEmoteCameraVisibility
	{
		bool bLegacyOwnerNoSee;
		bool bRobotOwnerNoSee;
		bool bRobotVisible;
		bool bRobotHiddenInGame;
		bool bArmsVisible;
		EVisibilityBasedAnimTickOption AnimTickOption;
	};
	TOptional<FEmoteCameraVisibility> SavedEmoteCameraVisibility;
	bool bEmoteCameraShowsRobot = false;
	float EmoteCameraBlend = 0.f;
	double EmoteCameraHeading = 0.;
	double EmoteCameraYawDelta = 0.;
	double EmoteCameraObstacleAngle = 0.;
	FVector EmoteCameraOffset = FVector::ZeroVector;
	FRotator EmoteCameraOrbit = FRotator::ZeroRotator;

	FVector ResolveEmoteCameraObstacles(const FVector& Focus, const FVector& DesiredOffset);
	FVector SweepEmoteCamera(const FVector& Focus, const FVector& Destination) const;
	void SetEmoteCameraBodyVisible(bool bShowRobot);
	void ResetEmoteCamera();

	UPROPERTY()
	TObjectPtr<USceneComponent> HeadFlashlightAim;

	/** Edit the light's intensity, range, cone, color and relative transform on this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Botanicus|Flashlight", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpotLightComponent> HeadFlashlight;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Flashlight")
	bool bAutoFlashlightEnabled = true;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Flashlight", meta=(ClampMin="0", ClampMax="1"))
	float BrightnessThreshold = 0.50f;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Flashlight", meta=(ClampMin="0", ClampMax="0.5"))
	float BrightnessHysteresis = 0.05f;
	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Flashlight", meta=(ClampMin="0", Units="s"))
	float FlashlightFadeDuration = 0.25f;

	UPROPERTY(Replicated)
	bool bHeadFlashlightOn = false;

	// Movement replication supplies yaw unless the player turns in place
	// and the authoritative pawn's body rotation does not follow the view.
	static constexpr uint16 NoFlashlightYawOverride = 256;
	UPROPERTY(Replicated)
	uint16 FlashlightYawOverride = NoFlashlightYawOverride;
	bool bPredictedHeadFlashlightOn = false;

	// Unset keeps the existing automatic light policy. This state lives only on the current pawn.
	TOptional<bool> ManualFlashlightOverride;

	UFUNCTION(Server, Reliable)
	void ServerSetManualFlashlightOverride(bool bOn);
	float FlashlightFullIntensity = 0.f;
	float FlashlightFadeAlpha = 0.f;
	FTimerHandle FlashlightSampleTimer;
	TWeakObjectPtr<ABotanicusAmbienceDirector> FlashlightAmbience;

	void InitializeHeadFlashlight();
	void ConfigureFlashlightSampling();
	void SampleHeadFlashlight();
	void UpdateHeadFlashlight(float DeltaSeconds);

	/** Detects and authoritatively executes gameplay interactions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBotanicusInteractionComponent* BotanicusInteractionComponent;

	/** Ten keyboard-accessible private inventory slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBotanicusQuickBarComponent* QuickBarComponent;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Components",meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBotanicusMultiToolComponent> MultiToolComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	explicit ABotanicusCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void EndViewTarget(APlayerController* PC) override;
	virtual void Jump() override;
	bool AddEmoteCameraOrbit(float Yaw, float Pitch);
	bool IsEmoteCameraInUse() const { return IsRobotEmotePlaying() || SavedEmoteCameraVisibility.IsSet(); }
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f) override;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.f, bool bForce = false) override;
	void ToggleHeadFlashlight();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void NotifyControllerChanged() override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnStartCrouch(
		float HalfHeightAdjust,
		float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(
		float HalfHeightAdjust,
		float ScaledHalfHeightAdjust) override;
	void ConfigureTrueFirstPersonLocalView();
	void RefreshFirstPersonCrouchOffset();

	bool bTrueFirstPersonConfigured = false;

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	USkeletalMeshComponent* GetRobotMesh() const { return RobotMesh; }
	void RegisterHeldItem(UBotanicusHeldItemComponent* Item, bool bRegister);
	void SetObservedQuickBarItem(FName ItemKey);
	ABotanicusPlaceableItemActor* GetObservedQuickBarItem() const;
	UBotanicusHeldItemComponent* GetRobotHeldGrip() const;

	UBotanicusEmoteCatalog* GetRobotEmoteCatalog() const { return EmoteCatalog; }
	const USkeleton* GetRobotEmoteSkeleton() const;
	const FBotanicusEmoteState& GetRobotEmoteState() const { return EmoteState; }
	bool IsRobotEmotePlaying() const;
	double GetRobotEmoteElapsedTime() const;
	bool ShouldHideEmoteHeldItems() const { return IsRobotEmotePlaying() || bEmoteCameraShowsRobot; }
	void StopRobotEmote();
	void InterruptLocalRobotEmote();
	void RequestRobotEmote(FName EmoteId);
	FBotanicusHeldItemPose UpdateHeldItemPose(float DeltaSeconds, const FBotanicusRobotToolMotion& Motion = {}, bool bSuppressHeldItem = false);
	ABotanicusComputerActor* GetOrderComputer() const { return OrderComputer; }
	void SetOrderComputer(ABotanicusComputerActor* Computer);
	bool IsUsingOrderComputer() const;
	double GetRobotActionTime() const;
	FName GetRobotActionItemKey() const;
	void PlayRobotAction(EBotanicusRobotAction Action, const FVector& Target, FName ItemKey = NAME_None);
	void ReceiveRobotAction(EBotanicusRobotAction Action, const FVector& Target, double StartedAt, FName ItemKey);
	void SetRobotActionState(EBotanicusRobotAction Action, const UObject* Source,
		bool bEnabled, const FVector& Target = FVector::ZeroVector, bool bHasTarget = false,
		const FVector& TracePosition = FVector::ZeroVector);
	FBotanicusRobotActionCue GetRobotActionState(EBotanicusRobotAction Action) const;
	const FBotanicusRobotActionCue& GetPendingRobotAction() const { return PendingRobotAction; }
	ABotanicusDeliveryParcelActor* GetCarriedParcelForAnimation() const;
	void UpdateCarriedParcelForAnimation(ABotanicusDeliveryParcelActor* Parcel, bool bIsCarrier);

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Returns the multiplayer-safe interaction component. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Interaction")
	UBotanicusInteractionComponent* GetInteractionComponent() const { return BotanicusInteractionComponent; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Quick Bar")
	UBotanicusQuickBarComponent* GetQuickBarComponent() const { return QuickBarComponent; }

	void SetCarryMovementMultiplier(float InMultiplier);
	void SetSprinting(bool bNewSprinting);
	void ToggleCrouching();

	UFUNCTION(BlueprintPure, Category="Botanicus|Movement")
	bool IsSprinting() const { return bIsSprinting; }

	void SetEquipmentCarryState(
		EBotanicusEquipmentCarryRole InRole,
		float InMovementMultiplier);

	UFUNCTION(BlueprintPure, Category="Botanicus|Equipment Carry")
	EBotanicusEquipmentCarryRole GetEquipmentCarryRole() const
	{
		return EquipmentCarryRole;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Equipment Carry")
	bool IsCarryingEquipment() const
	{
		return EquipmentCarryRole !=
			EBotanicusEquipmentCarryRole::None;
	}

	UFUNCTION(BlueprintPure, Category="Botanicus|Tools")
	ABotanicusWateringCanActor* GetHeldWateringCan() const
	{
		return HeldWateringCan;
	}

	/** Server-only link used by physical handheld tools. */
	void SetHeldWateringCan(ABotanicusWateringCanActor* InWateringCan);

	UFUNCTION(
		BlueprintImplementableEvent,
		Category="Botanicus|Equipment Carry",
		meta=(DisplayName="On Equipment Carry State Changed"))
	void ReceiveEquipmentCarryStateChanged(
		EBotanicusEquipmentCarryRole NewRole);

private:
	UFUNCTION()
	void OnRep_CarryMovementMultiplier();

	UFUNCTION()
	void OnRep_EquipmentCarryRole();

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	void ApplyCarryMovementMultiplier();

	UPROPERTY(ReplicatedUsing=OnRep_CarryMovementMultiplier)
	float CarryMovementMultiplier = 1.0f;

	UPROPERTY(ReplicatedUsing=OnRep_EquipmentCarryRole)
	EBotanicusEquipmentCarryRole EquipmentCarryRole =
		EBotanicusEquipmentCarryRole::None;

	UPROPERTY(Replicated)
	TObjectPtr<ABotanicusWateringCanActor> HeldWateringCan;

	UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Movement", meta=(ClampMin="1.0"))
	float SprintSpeedMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Movement", meta=(ClampMin="0.1", ClampMax="1.0"))
	float CrouchedSpeedMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Movement", meta=(ClampMin="0.0"))
	float CrouchedCameraOffset = 48.0f;

	float DefaultMaxWalkSpeed = 0.0f;

	// Derived from the parcel's replicated Carrier for animation lookup.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRobotAction(EBotanicusRobotAction Action, FVector_NetQuantize10 Target, double StartedAt, FName ItemKey);

	UPROPERTY(Replicated)
	TObjectPtr<ABotanicusComputerActor> OrderComputer;
	void UpdateHeldItemVisibility(AActor* ItemToHide);
	TWeakObjectPtr<AActor> HiddenHeldItem;
	struct FHiddenHeldPrimitive
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		bool bMainPass;
		bool bDepthPass;
		bool bCustomDepth;
		bool bCastShadow;
	};
	TArray<FHiddenHeldPrimitive> HiddenHeldPrimitives;

	UPROPERTY(EditDefaultsOnly, Category="Botanicus|Emotes")
	TObjectPtr<UBotanicusEmoteCatalog> EmoteCatalog;

	UPROPERTY(ReplicatedUsing=OnRep_EmoteState)
	FBotanicusEmoteState EmoteState;

	TOptional<uint16> PredictedEmoteStop;
	double LastEmoteRequest = -1.0;
	void UpdateRobotEmote();
	TOptional<double> EmoteEndControlYaw;
	void BeginRobotEmote();
	void ReleaseRobotEmoteControls();

	UPROPERTY(VisibleAnywhere, Category="Botanicus|Emotes")
	TObjectPtr<UAudioComponent> RobotEmoteAudio;

	FTimerHandle RobotEmoteAudioTimer;
	float RobotEmoteAudioFadeOut = .20f;
	void ResetRobotEmoteAudio();
	void PlayRobotEmoteAudio();
	void StopRobotEmoteAudio();

	UFUNCTION(Server, Reliable)
	void ServerPlayRobotEmote(FName EmoteId, uint16 RequestedYaw);

	UFUNCTION(Server, Reliable)
	void ServerCancelRobotEmote(uint16 Revision);

	UFUNCTION()
	void OnRep_EmoteState(const FBotanicusEmoteState& PreviousState);
	TMap<EBotanicusRobotAction, FBotanicusRobotActionCue> ActiveRobotActions;
	FBotanicusRobotActionCue PendingRobotAction;
	TArray<TWeakObjectPtr<UBotanicusHeldItemComponent>> HeldItemSources;
	TWeakObjectPtr<ABotanicusPlaceableItemActor> ObservedQuickBarItem;
	TWeakObjectPtr<ABotanicusDeliveryParcelActor> CarriedParcelForAnimation;
};

