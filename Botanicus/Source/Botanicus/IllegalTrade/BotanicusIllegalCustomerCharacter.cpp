// Copyright Epic Games, Inc. All Rights Reserved.

#include "IllegalTrade/BotanicusIllegalCustomerCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UI/BotanicusIllegalOrderWidget.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusIllegalCustomerCharacter::ABotanicusIllegalCustomerCharacter()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->MaxWalkSpeed = 190.0f;
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_Visibility, ECR_Block);

	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(GetCapsuleComponent());
	BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyVisual->SetVisibility(false);
	BodyVisual->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (AnimFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimFinder.Class);
	}

	OrderWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OrderWidget"));
	OrderWidget->SetupAttachment(GetCapsuleComponent());
	OrderWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 155.0f));
	OrderWidget->SetWidgetSpace(EWidgetSpace::World);
	OrderWidget->SetDrawSize(FVector2D(360.0f, 190.0f));
	OrderWidget->SetPivot(FVector2D(0.5f, 0.5f));
	OrderWidget->SetRelativeScale3D(FVector(0.32f));
	OrderWidget->SetTwoSided(true);
	OrderWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrderWidget->SetWidgetClass(UBotanicusIllegalOrderWidget::StaticClass());
}

void ABotanicusIllegalCustomerCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Existing Blueprint instances may still serialize the old empty
	// CharacterMesh0 template. Restore the visitor mannequin at runtime too.
	if (GetMesh())
	{
		if (!GetMesh()->GetSkeletalMeshAsset())
		{
			GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,
				TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
		}
		if (!GetMesh()->GetAnimClass())
		{
			GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,
				TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed_C")));
		}
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}
	if (OrderWidget)
	{
		if (UClass* WidgetClass = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/Botanicus/UI/IllegalTrade/WBP_IllegalCustomerOrder.WBP_IllegalCustomerOrder_C")))
		{
			OrderWidget->SetWidgetClass(WidgetClass);
		}
		OrderWidget->InitWidget();
	}
	RefreshOrderText();
	if (HasAuthority())
	{
		SpawnDefaultController();
	}
}

void ABotanicusIllegalCustomerCharacter::InitializeIllegalOrder(
	FName InProductItemKey,
	FText InProductName,
	int32 InQuantity,
	int32 InReward,
	float InWaitSeconds,
	const FVector& InTargetLocation,
	const FVector& InExitLocation)
{
	if (!HasAuthority()) return;
	ProductItemKey = InProductItemKey;
	ProductName = InProductName;
	RequestedQuantity = FMath::Max(1, InQuantity);
	RewardCredits = FMath::Max(0, InReward);
	RemainingWaitSeconds = FMath::Max(1.0f, InWaitSeconds);
	TargetLocation = InTargetLocation;
	ExitLocation = InExitLocation;
	CustomerState = EBotanicusIllegalCustomerState::Approaching;
	LastMovementDiagnosticPosition = GetActorLocation();
	RefreshOrderText();
	ForceNetUpdate();
	MoveTo(TargetLocation);
}

void ABotanicusIllegalCustomerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (OrderWidget)
	{
		const APlayerController* LocalPlayer = GetWorld()
			? GetWorld()->GetFirstPlayerController()
			: nullptr;
		const APlayerCameraManager* Camera = LocalPlayer
			? LocalPlayer->PlayerCameraManager
			: nullptr;
		if (Camera)
		{
			OrderWidget->SetWorldRotation(
				(Camera->GetCameraLocation() -
				 OrderWidget->GetComponentLocation()).Rotation());
		}
	}
	if (!HasAuthority()) return;
	if (CustomerState == EBotanicusIllegalCustomerState::Approaching ||
		CustomerState == EBotanicusIllegalCustomerState::Leaving)
	{
		MovementDiagnosticElapsedSeconds += DeltaSeconds;
		if (MovementDiagnosticElapsedSeconds >= 5.0f)
		{
			MovementDiagnosticElapsedSeconds = 0.0f;
			if (FVector::DistSquared2D(
					GetActorLocation(), LastMovementDiagnosticPosition) <
				FMath::Square(50.0f))
			{
				UE_LOG(LogBotanicusIllegalTrade, Warning,
					TEXT("Illegal customer is stuck at %s (state %d)."),
					*GetActorLocation().ToCompactString(),
					static_cast<int32>(CustomerState));
			}
			LastMovementDiagnosticPosition = GetActorLocation();
		}
		MovementRetryElapsedSeconds += DeltaSeconds;
		if (MovementRetryElapsedSeconds >= 2.0f)
		{
			MovementRetryElapsedSeconds = 0.0f;
			if (const AAIController* AIController = Cast<AAIController>(GetController());
				!AIController || AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				MoveTo(CustomerState == EBotanicusIllegalCustomerState::Approaching
					? TargetLocation : ExitLocation);
			}
		}
	}

	if (CustomerState == EBotanicusIllegalCustomerState::Approaching &&
		FVector::DistSquared2D(GetActorLocation(), TargetLocation) <= FMath::Square(175.0f))
	{
		EnterWaitingState();
	}
	else if (CustomerState == EBotanicusIllegalCustomerState::Waiting)
	{
		RemainingWaitSeconds = FMath::Max(0.0f, RemainingWaitSeconds - DeltaSeconds);
		if (RemainingWaitSeconds <= 0.0f)
		{
			UE_LOG(LogBotanicusIllegalTrade, Display,
				TEXT("Order failed: %s x%d."), *ProductItemKey.ToString(), RequestedQuantity);
			if (ABotanicusGameState* State = GetWorld()->GetGameState<ABotanicusGameState>())
			{
				State->AddSuspicion(GetDefault<UBotanicusIllegalTradeSettings>()->SuspicionOnIgnoredCustomer);
			}
			BeginLeaving(false);
		}
	}
	else if (CustomerState == EBotanicusIllegalCustomerState::Leaving &&
		FVector::DistSquared2D(GetActorLocation(), ExitLocation) <= FMath::Square(175.0f))
	{
		Destroy();
		return;
	}
	else if (CustomerState == EBotanicusIllegalCustomerState::Leaving)
	{
		LeavingElapsedSeconds += DeltaSeconds;
		if (LeavingElapsedSeconds >= 20.0f)
		{
			Destroy();
			return;
		}
	}

	ReplicationAccumulator += DeltaSeconds;
	if (ReplicationAccumulator >= 1.0f)
	{
		ReplicationAccumulator = 0.0f;
		RefreshOrderText();
		ForceNetUpdate();
	}
}

FBotanicusInteractionPrompt ABotanicusIllegalCustomerCharacter::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = ProductName.IsEmpty()
		? NSLOCTEXT("BotanicusIllegalTrade", "NightCustomer", "Client clandestin")
		: FText::Format(NSLOCTEXT("BotanicusIllegalTrade", "NightCustomerOrder", "Client clandestin — {0} x{1}"), ProductName, FText::AsNumber(RequestedQuantity));
	Prompt.ActionText = NSLOCTEXT("BotanicusIllegalTrade", "SellOrder", "Vendre la commande");
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	return Prompt;
}

bool ABotanicusIllegalCustomerCharacter::CanInteract_Implementation(AActor* Interactor) const
{
	const ABotanicusGameState* State = GetWorld()
		? GetWorld()->GetGameState<ABotanicusGameState>()
		: nullptr;
	return CustomerState == EBotanicusIllegalCustomerState::Waiting &&
		State && State->IsIllegalTradeActive() &&
		IsValid(Interactor) &&
		FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) <=
			FMath::Square(450.0f);
}

void ABotanicusIllegalCustomerCharacter::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor)) return;
	ABotanicusCharacter* Character = Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar = Character ? Character->GetQuickBarComponent() : nullptr;
	ABotanicusGameState* State = GetWorld()->GetGameState<ABotanicusGameState>();
	if (!State || ProductItemKey.IsNone() || RequestedQuantity <= 0)
	{
		return;
	}
	if (!QuickBar || QuickBar->GetTotalQuantity(ProductItemKey) < RequestedQuantity)
	{
		if (APlayerController* PlayerController = Character ? Cast<APlayerController>(Character->GetController()) : nullptr)
		{
			PlayerController->ClientMessage(TEXT("Il vous manque des Noctiflores pour cette commande."));
		}
		return;
	}
	if (!QuickBar->RemoveItem(ProductItemKey, RequestedQuantity)) return;
	UE_LOG(LogBotanicusIllegalTrade, Display,
		TEXT("Order completed: %s x%d for %d credits."),
		*ProductItemKey.ToString(), RequestedQuantity, RewardCredits);
	State->AddSharedFunds(RewardCredits);
	State->AddSuspicion(GetDefault<UBotanicusIllegalTradeSettings>()->SuspicionPerSale);
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		PlayerController->ClientMessage(*FString::Printf(TEXT("Commande clandestine vendue : +%d crédits."), RewardCredits));
	}
	if (ABotanicusGameMode* GameMode = GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
	BeginLeaving(true);
}

void ABotanicusIllegalCustomerCharacter::ForceCustomerToLeave()
{
	if (HasAuthority() && CustomerState != EBotanicusIllegalCustomerState::Leaving)
	{
		BeginLeaving(false);
	}
}

void ABotanicusIllegalCustomerCharacter::MoveTo(const FVector& Destination)
{
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		const EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
			Destination, 75.0f, true, true, true, true);
		if (Result == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogBotanicusIllegalTrade, Warning,
				TEXT("No navigation path for illegal customer from %s to %s."),
				*GetActorLocation().ToCompactString(),
				*Destination.ToCompactString());
		}
	}
}

void ABotanicusIllegalCustomerCharacter::EnterWaitingState()
{
	CustomerState = EBotanicusIllegalCustomerState::Waiting;
	UE_LOG(LogBotanicusIllegalTrade, Display,
		TEXT("Illegal customer reached the rear waiting point at %s."),
		*GetActorLocation().ToCompactString());
	MovementRetryElapsedSeconds = 0.0f;
	if (AAIController* AIController = Cast<AAIController>(GetController())) AIController->StopMovement();
	RefreshOrderText();
	ForceNetUpdate();
}

void ABotanicusIllegalCustomerCharacter::BeginLeaving(bool bOrderSold)
{
	CustomerState = EBotanicusIllegalCustomerState::Leaving;
	LeavingElapsedSeconds = 0.0f;
	MovementRetryElapsedSeconds = 0.0f;
	RefreshOrderText();
	ForceNetUpdate();
	MoveTo(ExitLocation);
	UE_LOG(LogBotanicusIllegalTrade, Display, TEXT("Illegal customer leaving (%s)."), bOrderSold ? TEXT("sale completed") : TEXT("no sale"));
}

void ABotanicusIllegalCustomerCharacter::RefreshOrderText()
{
	if (OrderWidget)
	{
		// A fulfilled or expired order must not keep looking actionable.
		OrderWidget->SetVisibility(
			CustomerState != EBotanicusIllegalCustomerState::Leaving);
		if (UBotanicusIllegalOrderWidget* Widget =
				Cast<UBotanicusIllegalOrderWidget>(OrderWidget->GetWidget()))
		{
			Widget->SetOrderData(
				ProductName,
				RequestedQuantity,
				RewardCredits,
				RemainingWaitSeconds,
				CustomerState == EBotanicusIllegalCustomerState::Waiting,
				CustomerState == EBotanicusIllegalCustomerState::Leaving);
		}
	}
}

void ABotanicusIllegalCustomerCharacter::OnRep_OrderState()
{
	RefreshOrderText();
}

void ABotanicusIllegalCustomerCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, CustomerState);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, ProductItemKey);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, ProductName);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, RequestedQuantity);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, RewardCredits);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, RemainingWaitSeconds);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, TargetLocation);
	DOREPLIFETIME(ABotanicusIllegalCustomerCharacter, ExitLocation);
}
