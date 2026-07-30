// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "BotanicusCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UBotanicusInteractionComponent;
class UBotanicusQuickBarComponent;
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

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Detects and authoritatively executes gameplay interactions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBotanicusInteractionComponent* BotanicusInteractionComponent;

	/** Ten keyboard-accessible private inventory slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBotanicusQuickBarComponent* QuickBarComponent;

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
	ABotanicusCharacter();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual bool CanJumpInternal_Implementation() const override;
	void ConfigureTrueFirstPersonLocalView();

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

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Returns the multiplayer-safe interaction component. */
	UFUNCTION(BlueprintPure, Category="Botanicus|Interaction")
	UBotanicusInteractionComponent* GetInteractionComponent() const { return BotanicusInteractionComponent; }

	UFUNCTION(BlueprintPure, Category="Botanicus|Quick Bar")
	UBotanicusQuickBarComponent* GetQuickBarComponent() const { return QuickBarComponent; }

	void SetCarryMovementMultiplier(float InMultiplier);
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

	void ApplyCarryMovementMultiplier();

	UPROPERTY(ReplicatedUsing=OnRep_CarryMovementMultiplier)
	float CarryMovementMultiplier = 1.0f;

	UPROPERTY(ReplicatedUsing=OnRep_EquipmentCarryRole)
	EBotanicusEquipmentCarryRole EquipmentCarryRole =
		EBotanicusEquipmentCarryRole::None;

	float DefaultMaxWalkSpeed = 0.0f;
};

