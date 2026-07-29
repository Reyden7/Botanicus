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
#include "InputCoreTypes.h"
#include "Interaction/BotanicusInteractionComponent.h"
#include "QuickBar/BotanicusQuickBarComponent.h"

ABotanicusCharacter::ABotanicusCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
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
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
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
