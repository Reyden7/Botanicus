// Copyright Epic Games, Inc. All Rights Reserved.

#include "BotanicusCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Botanicus.h"
#include "BotanicusGameState.h"
#include "InputCoreTypes.h"
#include "Interaction/BotanicusInteractionComponent.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"

ABotanicusCharacter::ABotanicusCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// True-FPS rotation model: mouse yaw rotates the complete character while
	// mouse pitch remains a view/head movement instead of tilting the capsule.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(GetMesh(), FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	BotanicusInteractionComponent = CreateDefaultSubobject<UBotanicusInteractionComponent>(
		TEXT("Botanicus Interaction Component"));
	QuickBarComponent = CreateDefaultSubobject<UBotanicusQuickBarComponent>(TEXT("Quick Bar Component"));

	// configure the character comps
	// The owner sees the world-space body in true first person. The local head
	// is hidden in BeginPlay to keep the camera out of the skull.
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
}

void ABotanicusCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	if (const ABotanicusGameState* GameState =
			GetWorld()
				? GetWorld()->GetGameState<ABotanicusGameState>()
				: nullptr)
	{
		CustomTimeDilation =
			1.0f /
			FMath::Max(
				1.0f,
				GameState->GetDevelopmentTimeScale());
	}
	ApplyCarryMovementMultiplier();
	ConfigureTrueFirstPersonLocalView();

}

void ABotanicusCharacter::ToggleCrouching()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	SetSprinting(false);
	if (bIsCrouched || Movement->bWantsToCrouch)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABotanicusCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusCharacter,
		CarryMovementMultiplier);
	DOREPLIFETIME(
		ABotanicusCharacter,
		EquipmentCarryRole);
	DOREPLIFETIME(
		ABotanicusCharacter,
		HeldWateringCan);
	DOREPLIFETIME(
		ABotanicusCharacter,
		bIsSprinting);
}

void ABotanicusCharacter::SetSprinting(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting && !bIsCrouched;
	ApplyCarryMovementMultiplier();
	if (!HasAuthority() && IsLocallyControlled())
	{
		ServerSetSprinting(bIsSprinting);
	}
	else
	{
		ForceNetUpdate();
	}
}

void ABotanicusCharacter::ServerSetSprinting_Implementation(
	bool bNewSprinting)
{
	SetSprinting(bNewSprinting);
}

void ABotanicusCharacter::SetHeldWateringCan(
	ABotanicusWateringCanActor* InWateringCan)
{
	if (!HasAuthority())
	{
		return;
	}
	HeldWateringCan = InWateringCan;
	ForceNetUpdate();
}

void ABotanicusCharacter::SetCarryMovementMultiplier(
	float InMultiplier)
{
	if (!HasAuthority())
	{
		return;
	}

	CarryMovementMultiplier =
		FMath::Clamp(InMultiplier, 0.1f, 1.0f);
	ApplyCarryMovementMultiplier();
	ForceNetUpdate();
}

void ABotanicusCharacter::SetEquipmentCarryState(
	EBotanicusEquipmentCarryRole InRole,
	float InMovementMultiplier)
{
	if (!HasAuthority())
	{
		return;
	}

	const EBotanicusEquipmentCarryRole PreviousRole =
		EquipmentCarryRole;
	EquipmentCarryRole = InRole;
	CarryMovementMultiplier =
		InRole == EBotanicusEquipmentCarryRole::None
			? 1.0f
			: FMath::Clamp(
				  InMovementMultiplier,
				  0.1f,
				  1.0f);
	ApplyCarryMovementMultiplier();
	if (PreviousRole != EquipmentCarryRole)
	{
		ReceiveEquipmentCarryStateChanged(EquipmentCarryRole);
	}
	ForceNetUpdate();
}

void ABotanicusCharacter::OnRep_CarryMovementMultiplier()
{
	ApplyCarryMovementMultiplier();
}

void ABotanicusCharacter::OnRep_EquipmentCarryRole()
{
	ApplyCarryMovementMultiplier();
	ReceiveEquipmentCarryStateChanged(EquipmentCarryRole);
}

void ABotanicusCharacter::OnRep_IsSprinting()
{
	ApplyCarryMovementMultiplier();
}

void ABotanicusCharacter::ApplyCarryMovementMultiplier()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}
	if (DefaultMaxWalkSpeed <= 0.0f)
	{
		DefaultMaxWalkSpeed = Movement->MaxWalkSpeed;
	}
	const float SprintMultiplier =
		bIsSprinting && !bIsCrouched
			? SprintSpeedMultiplier
			: 1.0f;
	Movement->MaxWalkSpeed =
		DefaultMaxWalkSpeed * CarryMovementMultiplier * SprintMultiplier;
	Movement->MaxWalkSpeedCrouched =
		DefaultMaxWalkSpeed * CarryMovementMultiplier *
		CrouchedSpeedMultiplier;
}

bool ABotanicusCharacter::CanJumpInternal_Implementation() const
{
	return EquipmentCarryRole ==
			EBotanicusEquipmentCarryRole::None &&
		Super::CanJumpInternal_Implementation();
}

void ABotanicusCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ConfigureTrueFirstPersonLocalView();
}

void ABotanicusCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	// Blueprint defaults and EBS camera changes can overwrite inherited C++
	// component settings. Enforce the true-FPS contract at runtime.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	if (AController* CharacterController = GetController())
	{
		const float ControlYaw =
			CharacterController->GetControlRotation().Yaw;
		SetActorRotation(FRotator(0.0f, ControlYaw, 0.0f));
	}

	if (GetMesh()->bOwnerNoSee ||
		FirstPersonMesh->IsVisible() ||
		!FirstPersonCameraComponent->bUsePawnControlRotation)
	{
		ConfigureTrueFirstPersonLocalView();
	}
	RefreshFirstPersonCrouchOffset();
}

void ABotanicusCharacter::OnStartCrouch(
	float HalfHeightAdjust,
	float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetSprinting(false);
	RefreshFirstPersonCrouchOffset();
}

void ABotanicusCharacter::OnEndCrouch(
	float HalfHeightAdjust,
	float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshFirstPersonCrouchOffset();
}

void ABotanicusCharacter::RefreshFirstPersonCrouchOffset()
{
	if (!FirstPersonCameraComponent || !GetMesh() ||
		!IsLocallyControlled())
	{
		return;
	}

	const FVector StandingRelativeLocation(-2.8f, 5.89f, 0.0f);
	const FTransform HeadSocketTransform =
		GetMesh()->GetSocketTransform(TEXT("head"), RTS_World);
	const FVector StandingWorldLocation =
		HeadSocketTransform.TransformPosition(StandingRelativeLocation);
	const FVector DesiredWorldLocation =
		StandingWorldLocation -
		FVector(
			0.0f,
			0.0f,
			bIsCrouched ? CrouchedCameraOffset : 0.0f);
	FirstPersonCameraComponent->SetRelativeLocation(
		HeadSocketTransform.InverseTransformPosition(
			DesiredWorldLocation));
}

void ABotanicusCharacter::ConfigureTrueFirstPersonLocalView()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Use the full third-person body for the owner and retain the existing
	// animated head socket as the camera anchor. Only the local rendering
	// hides the head; other players still see the complete character.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->HideBoneByName(TEXT("head"), PBO_None);
	FirstPersonMesh->SetVisibility(false, false);
	FirstPersonCameraComponent->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::KeepRelativeTransform,
		TEXT("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(
		FVector(-2.8f, 5.89f, 0.0f),
		FRotator(0.0f, 90.0f, -90.0f));
	RefreshFirstPersonCrouchOffset();
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->Activate(true);

	if (!bTrueFirstPersonConfigured)
	{
		bTrueFirstPersonConfigured = true;
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT(
				"True FPS enforced on runtime pawn %s (class %s)."),
			*GetName(),
			*GetClass()->GetPathName());
	}
}

void ABotanicusCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABotanicusCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABotanicusCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABotanicusCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABotanicusCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABotanicusCharacter::LookInput);

		// Botanicus is keyboard/mouse only. Binding E directly keeps the C++ foundation
		// usable before a dedicated Enhanced Input asset is authored in the editor.
		// UE 5.8 intentionally hides legacy key binding on UEnhancedInputComponent.
		// Binding through its UInputComponent base keeps this bootstrap key available
		// without requiring a binary Input Action asset.
		PlayerInputComponent->BindKey(
			EKeys::E,
			IE_Pressed,
			BotanicusInteractionComponent,
			&UBotanicusInteractionComponent::TryInteract);

		PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot1);
		PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot2);
		PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot3);
		PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot4);
		PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot5);
		PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot6);
		PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot7);
		PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot8);
		PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot9);
		PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectSlot10);
		PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectPreviousSlot);
		PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, QuickBarComponent, &UBotanicusQuickBarComponent::SelectNextSlot);
	}
	else
	{
		UE_LOG(LogBotanicus, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void ABotanicusCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ABotanicusCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ABotanicusCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABotanicusCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ABotanicusCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void ABotanicusCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
