// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visitors/BotanicusVisitorCharacter.h"

#include "BotanicusGameState.h"
#include "SpecialOrders/BotanicusSpecialOrderComponent.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Catalog/BotanicusItemCatalog.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Interaction/BotanicusInteractableActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "UI/BotanicusVisitorSpeechBubbleWidget.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
FString VisitorColorLabel(FName ColorTag)
{
	if (ColorTag == TEXT("Red"))
	{
		return TEXT("rouge");
	}
	if (ColorTag == TEXT("Pink"))
	{
		return TEXT("rose");
	}
	if (ColorTag == TEXT("White"))
	{
		return TEXT("blanche");
	}
	if (ColorTag == TEXT("Yellow"))
	{
		return TEXT("jaune");
	}
	if (ColorTag == TEXT("Purple"))
	{
		return TEXT("violette");
	}
	return TEXT("verte");
}

FString VisitorTypeLabel(FName TypeTag)
{
	if (TypeTag == TEXT("Flowering"))
	{
		return TEXT("une plante fleurie");
	}
	if (TypeTag == TEXT("Foliage"))
	{
		return TEXT("une plante decorative");
	}
	return TEXT("une plante aromatique");
}

FLinearColor VisitorPlantVisualColor(FName ColorTag)
{
	if (ColorTag == TEXT("Pink"))
	{
		return FLinearColor(0.95f, 0.20f, 0.55f);
	}
	if (ColorTag == TEXT("Purple"))
	{
		return FLinearColor(0.48f, 0.20f, 0.78f);
	}
	return FLinearColor(0.08f, 0.48f, 0.12f);
}
}

ABotanicusVisitorCharacter::ABotanicusVisitorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
	static ConstructorHelpers::FObjectFinder<USoundBase> BellAsset(
		TEXT("/Game/Botanicus/Audio/S_SpecialOrderBell.S_SpecialOrderBell"));
	SpecialOrderBellSound = BellAsset.Object;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Ignore);
	GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->MaxStepHeight = 55.0f;
	GetCharacterMovement()->SetWalkableFloorAngle(50.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT(
			"/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
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

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("VisitorStatus"));
	StatusText->SetupAttachment(GetCapsuleComponent());
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(15.0f);
	StatusText->SetTextRenderColor(FColor(120, 240, 160));
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshStatusText();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	CarriedPotVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("CarriedSalePot"));
	CarriedPotVisual->SetupAttachment(GetCapsuleComponent());
	CarriedPotVisual->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	CarriedPotVisual->SetRelativeLocation(FVector(42.0f, 0.0f, 2.0f));
	CarriedPotVisual->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.2f));
	CarriedPotVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarriedPotVisual->SetVisibility(false);

	CarriedPlantVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("CarriedPlant"));
	CarriedPlantVisual->SetupAttachment(GetCapsuleComponent());
	CarriedPlantVisual->SetStaticMesh(
		SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
	CarriedPlantVisual->SetRelativeLocation(FVector(42.0f, 0.0f, 38.0f));
	CarriedPlantVisual->SetRelativeScale3D(FVector(0.29f, 0.29f, 0.42f));
	CarriedPlantVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarriedPlantVisual->SetVisibility(false);

	SpeechBubbleComponent = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("VisitorSpeechBubble"));
	SpeechBubbleComponent->SetupAttachment(GetCapsuleComponent());
	SpeechBubbleComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));
	SpeechBubbleComponent->SetWidgetSpace(EWidgetSpace::Screen);
	SpeechBubbleComponent->SetDrawAtDesiredSize(true);
	SpeechBubbleComponent->SetPivot(FVector2D(0.5f, 1.0f));
	SpeechBubbleComponent->SetWidgetClass(
		UBotanicusVisitorSpeechBubbleWidget::StaticClass());
	SpeechBubbleComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	SpeechBubbleComponent->SetVisibility(false);
}

void ABotanicusVisitorCharacter::BeginPlay()
{
	Super::BeginPlay();
	OnRep_SpeechLine();
	RefreshCarriedPlantVisuals();
}

void ABotanicusVisitorCharacter::InitializeCircuit(
	const TArray<FVector>& InRoutePoints,
	int32 InCheckoutWaypointIndex)
{
	if (!HasAuthority() ||
		InRoutePoints.Num() < 3 ||
		!InRoutePoints.IsValidIndex(InCheckoutWaypointIndex))
	{
		Destroy();
		return;
	}

	TargetDisplay = nullptr;
	RoutePoints = InRoutePoints;
	EntranceLocation = RoutePoints[0];
	RouteWaypointIndex = 1;
	CheckoutWaypointIndex = InCheckoutWaypointIndex;
	bPlantSelected = false;
	bReturningToRoute = false;
	ResetBrowsingState();
	VisitorState = EBotanicusVisitorState::FollowingRoute;
	DisplaySearchRemaining = FMath::FRandRange(0.0f, 0.5f);
	LastMovementLocation = GetActorLocation();
	StuckDuration = 0.0f;
	RefreshStatusText();
}

void ABotanicusVisitorCharacter::InitializeQueuedCircuit(
	const TArray<FVector>& InRoutePoints,
	int32 InCheckoutWaypointIndex,
	const FVector& InQueueDestination,
	const TArray<FVector>& InArrivalRoute,
	const TArray<FVector>& InDirectReturnRoute)
{
	if (!HasAuthority() ||
		InRoutePoints.Num() < 3 ||
		!InRoutePoints.IsValidIndex(InCheckoutWaypointIndex) ||
		InArrivalRoute.Num() < 2 ||
		InDirectReturnRoute.Num() < 2)
	{
		Destroy();
		return;
	}

	TargetDisplay = nullptr;
	RoutePoints = InArrivalRoute;
	DirectReturnRoute = InDirectReturnRoute;
	PurchaseRoute.Reset();
	const int32 PurchaseStartIndex = InArrivalRoute.Num() - 1;
	for (int32 Index = PurchaseStartIndex;
		 Index < InRoutePoints.Num();
		 ++Index)
	{
		PurchaseRoute.Add(InRoutePoints[Index]);
	}
	PurchaseCheckoutWaypointIndex =
		InCheckoutWaypointIndex - PurchaseStartIndex;
	EntranceLocation = RoutePoints[0];
	RouteWaypointIndex = 0;
	CheckoutWaypointIndex = InCheckoutWaypointIndex;
	QueueDestination = InQueueDestination;
	bPlantSelected = false;
	bReturningToRoute = false;
	ResetBrowsingState();
	VisitorState = EBotanicusVisitorState::Queued;
	LastMovementLocation = GetActorLocation();
	StuckDuration = 0.0f;
	RefreshStatusText();
}

void ABotanicusVisitorCharacter::SetQueueDestination(
	const FVector& InQueueDestination)
{
	if (HasAuthority() &&
		VisitorState == EBotanicusVisitorState::Queued)
	{
		QueueDestination = InQueueDestination;
	}
}

FVector ABotanicusVisitorCharacter::GetVariedQueueDestination(
	const FVector& BaseDestination,
	const FVector& QueueDirection) const
{
	const FVector FlatDirection =
		FVector(QueueDirection.X, QueueDirection.Y, 0.0f).
			GetSafeNormal();
	const FVector QueueRight(
		-FlatDirection.Y,
		FlatDirection.X,
		0.0f);
	return BaseDestination +
		FlatDirection * QueueLongitudinalOffset +
		QueueRight * QueueLateralOffset;
}

void ABotanicusVisitorCharacter::AdmitFromQueue()
{
	if (!HasAuthority() ||
		VisitorState != EBotanicusVisitorState::Queued)
	{
		return;
	}

	RouteWaypointIndex = 0;
	DisplaySearchRemaining = FMath::FRandRange(0.0f, 0.5f);
	VisitorState = EBotanicusVisitorState::FollowingRoute;
	LastMovementLocation = GetActorLocation();
	StuckDuration = 0.0f;
	RefreshStatusText();
	ForceNetUpdate();
}

void ABotanicusVisitorCharacter::ConfigureCheckoutQueue(
	const FVector& InQueueDestination,
	bool bIsFront)
{
	if (!HasAuthority() || !IsInCheckoutQueue())
	{
		return;
	}

	CheckoutQueueDestination = InQueueDestination;
	const float DistanceSquared = FVector::DistSquared2D(
		GetActorLocation(),
		CheckoutQueueDestination);
	if (bIsFront &&
		DistanceSquared <= FMath::Square(AcceptanceRadius))
	{
		if (VisitorState != EBotanicusVisitorState::Paying)
		{
			VisitorState = EBotanicusVisitorState::Paying;
			CheckoutStage = 0;
			GetCharacterMovement()->StopMovementImmediately();
			SetSpeechLine(TEXT("Bonjour, je voudrais cette plante."));
			RefreshStatusText();
			ForceNetUpdate();
		}
	}
	else if (VisitorState == EBotanicusVisitorState::Paying)
	{
		VisitorState = EBotanicusVisitorState::CheckoutQueue;
		CheckoutStage = 0;
		SetSpeechLine(TEXT("J'attends mon tour pour payer."));
		RefreshStatusText();
		ForceNetUpdate();
	}
}

int32 ABotanicusVisitorCharacter::GetCheckoutPrice() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(CarriedPlantItemKey) : nullptr;
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	return Definition
		? GameState
			? GameState->GetTrendAdjustedSalePrice(*Definition)
			: FMath::Max(0, Definition->SalePrice)
		: 0;
}

void ABotanicusVisitorCharacter::HandlePlayerCheckoutAction()
{
	if (!HasAuthority() || !CanUsePlayerCheckout())
	{
		return;
	}

	if (CheckoutStage == 0)
	{
		CheckoutStage = 1;
		SetSpeechLine(
			FString::Printf(
				TEXT("Plante scannee : %d credits."),
				GetCheckoutPrice()));
		RefreshStatusText();
		ForceNetUpdate();
		return;
	}

	if (TargetDisplay &&
		TargetDisplay->CompleteVisitorPurchase(this))
	{
		return;
	}
	SetSpeechLine(TEXT("Il y a un probleme avec cette vente."));
}

bool ABotanicusVisitorCharacter::AssignSelfCheckout(
	ABotanicusSelfCheckoutActor* InSelfCheckout)
{
	if (!HasAuthority() ||
		!IsWaitingForCheckoutAssignment() ||
		!IsValid(InSelfCheckout) ||
		!InSelfCheckout->IsOperational() ||
		InSelfCheckout->GetAssignedVisitor())
	{
		return false;
	}

	AssignedSelfCheckout = InSelfCheckout;
	VisitorState = EBotanicusVisitorState::SelfCheckout;
	CheckoutStage = 0;
	SelfCheckoutElapsed = 0.0f;
	CheckoutQueueDestination =
		InSelfCheckout->GetCustomerStandLocation();
	SetSpeechLine(TEXT("Une caisse automatique est libre."));
	RefreshStatusText();
	ForceNetUpdate();
	return true;
}

void ABotanicusVisitorCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusVisitorCharacter, SpecialOrderId);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		VisitorState);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		SpeechLine);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		bCarryingPlant);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		CarriedPlantItemKey);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		CarriedPotItemKey);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		CheckoutStage);
	DOREPLIFETIME(
		ABotanicusVisitorCharacter,
		AssignedSelfCheckout);
}

void ABotanicusVisitorCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	switch (VisitorState)
	{
	case EBotanicusVisitorState::Queued:
		MoveTowards(QueueDestination, DeltaSeconds);
		break;

	case EBotanicusVisitorState::FollowingRoute:
		DisplaySearchRemaining -= DeltaSeconds;
		if (!SpecialOrderId.IsValid() && !bPlantSelected &&
			DisplaySearchRemaining <= 0.0f &&
			RouteWaypointIndex <= CheckoutWaypointIndex &&
			IsInsideSalesArea())
		{
			DisplaySearchRemaining = 0.5f;
			if (TryFindAvailableDisplay())
			{
				break;
			}
		}
		FollowRoute(DeltaSeconds);
		break;

	case EBotanicusVisitorState::Approaching:
		if (!IsValid(TargetDisplay))
		{
			VisitorState = EBotanicusVisitorState::FollowingRoute;
			RefreshStatusText();
			break;
		}
		MoveTowards(TargetDisplay->GetActorLocation(), DeltaSeconds);
		if (FVector::DistSquared2D(
				GetActorLocation(),
				TargetDisplay->GetActorLocation()) <=
			FMath::Square(AcceptanceRadius))
		{
			BeginInspection();
		}
		break;

	case EBotanicusVisitorState::Inspecting:
		GetCharacterMovement()->StopMovementImmediately();
		InspectionRemaining -= DeltaSeconds;
		if (InspectionRemaining <= 0.0f)
		{
			FinishInspection();
		}
		break;

	case EBotanicusVisitorState::CheckoutQueue:
		CheckoutWaitDuration += DeltaSeconds;
		MoveTowards(CheckoutQueueDestination, DeltaSeconds);
		if (FVector::DistSquared2D(
				GetActorLocation(),
				CheckoutQueueDestination) <=
			FMath::Square(AcceptanceRadius * 0.65f))
		{
			GetCharacterMovement()->StopMovementImmediately();
		}
		break;

	case EBotanicusVisitorState::Paying:
		GetCharacterMovement()->StopMovementImmediately();
		CheckoutWaitDuration += DeltaSeconds;
		break;

	case EBotanicusVisitorState::SelfCheckout:
		CheckoutWaitDuration += DeltaSeconds;
		if (!IsValid(AssignedSelfCheckout) ||
			!AssignedSelfCheckout->IsOperational())
		{
			AssignedSelfCheckout = nullptr;
			VisitorState = EBotanicusVisitorState::CheckoutQueue;
			CheckoutStage = 0;
			SelfCheckoutElapsed = 0.0f;
			SetSpeechLine(TEXT("Je vais attendre une autre caisse."));
			RefreshStatusText();
			ForceNetUpdate();
			break;
		}
		CheckoutQueueDestination =
			AssignedSelfCheckout->GetCustomerStandLocation();
		MoveTowards(CheckoutQueueDestination, DeltaSeconds);
		if (FVector::DistSquared2D(
				GetActorLocation(),
				CheckoutQueueDestination) <=
			FMath::Square(AcceptanceRadius * 0.65f))
		{
			GetCharacterMovement()->StopMovementImmediately();
			SelfCheckoutElapsed += DeltaSeconds;
			if (CheckoutStage == 0 &&
				SelfCheckoutElapsed >= 1.0f)
			{
				CheckoutStage = 1;
				SetSpeechLine(
					FString::Printf(
						TEXT("Je scanne ma plante : %d credits."),
						GetCheckoutPrice()));
				RefreshStatusText();
				ForceNetUpdate();
			}
			if (SelfCheckoutElapsed >= 2.5f)
			{
				ABotanicusSalesDisplayActor* Display =
					TargetDisplay;
				AssignedSelfCheckout = nullptr;
				if (!Display ||
					!Display->CompleteVisitorPurchase(this))
				{
					VisitorState =
						EBotanicusVisitorState::CheckoutQueue;
					CheckoutStage = 0;
					SelfCheckoutElapsed = 0.0f;
					SetSpeechLine(
						TEXT("La caisse n'a pas valide mon achat."));
					RefreshStatusText();
					ForceNetUpdate();
				}
			}
		}
		break;

	case EBotanicusVisitorState::SpecialOrderApproaching:
		if (!SpecialOrderCounter.IsValid() || !SpecialOrderCounter->IsOperational())
		{
			if (auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld())) Orders->CustomerDeparted(SpecialOrderId, this);
			break;
		}
		MoveTowards(SpecialOrderStandLocation, DeltaSeconds);
		if (FVector::DistSquared2D(GetActorLocation(), SpecialOrderStandLocation) <= FMath::Square(45.0f))
		{
			GetCharacterMovement()->StopMovementImmediately();
			VisitorState = EBotanicusVisitorState::SpecialOrderWaiting;
			if (auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld())) Orders->CustomerArrived(SpecialOrderId, this);
			TryRingSpecialOrderBell();
			OnRep_VisitorState();
			RefreshSpecialOrderSpeech();
			ForceNetUpdate();
		}
		break;
	case EBotanicusVisitorState::SpecialOrderWaiting:
		if (!SpecialOrderCounter.IsValid() || !SpecialOrderCounter->IsOperational())
		{
			if (auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld())) Orders->CustomerDeparted(SpecialOrderId, this);
		}
		else
		{
			TryRingSpecialOrderBellReminder();
		}
		break;
	case EBotanicusVisitorState::Leaving:
		if (!RoutePoints.IsValidIndex(RouteWaypointIndex))
		{
			Destroy();
			break;
		}
		MoveTowards(
			RoutePoints[RouteWaypointIndex],
			DeltaSeconds);
		if (FVector::DistSquared2D(
				GetActorLocation(),
				RoutePoints[RouteWaypointIndex]) <=
			FMath::Square(AcceptanceRadius))
		{
			++RouteWaypointIndex;
			if (!RoutePoints.IsValidIndex(RouteWaypointIndex))
			{
				Destroy();
			}
		}
		break;
	}
	UpdateStuckDetection(DeltaSeconds);
}

void ABotanicusVisitorCharacter::MoveTowards(
	const FVector& Destination,
	float DeltaSeconds)
{
	FVector Direction = Destination - GetActorLocation();
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		return;
	}

	const FRotator DesiredRotation = Direction.Rotation();
	SetActorRotation(
		FMath::RInterpTo(
			GetActorRotation(),
			DesiredRotation,
			DeltaSeconds,
			6.0f));
	AddMovementInput(Direction, 1.0f);
}

void ABotanicusVisitorCharacter::BeginInspection()
{
	VisitorState = EBotanicusVisitorState::Inspecting;
	CurrentInspectionScore =
		ScoreDisplayForPreferences(TargetDisplay);
	InspectionRemaining =
		FMath::FRandRange(InspectionDuration, InspectionDuration + 2.5f);
	GetCharacterMovement()->StopMovementImmediately();
	if (TargetDisplay)
	{
		SetActorRotation(
			(TargetDisplay->GetActorLocation() -
			 GetActorLocation()).Rotation());
	}
	SetSpeechLine(ChooseInspectionSpeech(TargetDisplay));
	RefreshStatusText();
	ForceNetUpdate();
}

void ABotanicusVisitorCharacter::FinishInspection()
{
	if (!TargetDisplay)
	{
		SetSpeechLine(FString());
		VisitorState = EBotanicusVisitorState::FollowingRoute;
		RefreshStatusText();
		return;
	}

	++CompletedInspections;
	InspectedDisplays.AddUnique(TargetDisplay);
	const FBotanicusItemDefinition* InspectedDefinition =
		TargetDisplay->GetDisplayedPlantDefinitionForVisitor();
	const bool bWithinBudget =
		InspectedDefinition &&
		InspectedDefinition->SalePrice <= ShoppingBudget;
	const bool bAcceptable =
		bWithinBudget &&
		CurrentInspectionScore >= MinimumAcceptableScore;
	if (bAcceptable &&
		(!BestMatchingDisplay.IsValid() ||
			!BestMatchingDisplay->
				IsAvailableForVisitorBrowsing(this) ||
			CurrentInspectionScore > BestMatchingScore))
	{
		BestMatchingDisplay = TargetDisplay;
		BestMatchingScore = CurrentInspectionScore;
	}
	const int32 RequiredInspections =
		FMath::Clamp(
			DesiredInspectionCount,
			1,
			FMath::Max(1, CountBrowsableDisplays()));
	const bool bFinishedComparing =
		CompletedInspections >= RequiredInspections;
	SetSpeechLine(FString());

	if (bFinishedComparing &&
		BestMatchingDisplay.Get() == TargetDisplay &&
		TargetDisplay->TakeReservedPlantForVisitor(this))
	{
		bPlantSelected = true;
		CarriedPlantItemKey =
			TargetDisplay->GetDisplayedPlantItemKey();
		CarriedPotItemKey =
			TargetDisplay->GetDisplayedPotItemKey();
		bCarryingPlant = true;
		RefreshCarriedPlantVisuals();
		RoutePoints = PurchaseRoute;
		RouteWaypointIndex = 0;
		CheckoutWaypointIndex = PurchaseCheckoutWaypointIndex;
		bReturningToRoute = false;
		VisitorState = EBotanicusVisitorState::FollowingRoute;
		RefreshStatusText();
		ForceNetUpdate();
		return;
	}

	TargetDisplay->NotifyVisitorEnded(this);
	TargetDisplay = nullptr;
	if (bFinishedComparing)
	{
		ABotanicusSalesDisplayActor* BestDisplay =
			BestMatchingDisplay.Get();
		if (BestDisplay &&
			BestDisplay->IsAvailableForVisitorBrowsing(this))
		{
			InspectedDisplays.Remove(BestDisplay);
			DesiredInspectionCount = CompletedInspections + 1;
		}
		else
		{
			SwitchToDirectReturnRoute();
			SetSpeechLine(ChooseFinalRefusalSpeech());
			ForceNetUpdate();
			return;
		}
	}
	DisplaySearchRemaining = FMath::FRandRange(0.6f, 1.3f);
	VisitorState = EBotanicusVisitorState::FollowingRoute;
	RefreshStatusText();
	ForceNetUpdate();
}

bool ABotanicusVisitorCharacter::TryFindAvailableDisplay()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || IsValid(TargetDisplay))
	{
		return false;
	}

	TArray<ABotanicusSalesDisplayActor*> Candidates;
	for (TActorIterator<ABotanicusSalesDisplayActor> DisplayIt(World);
		 DisplayIt;
		 ++DisplayIt)
	{
		if (!DisplayIt->IsAvailableForVisitorBrowsing(this) ||
			InspectedDisplays.Contains(*DisplayIt))
		{
			continue;
		}
		bool bDisplayInsideSalesArea = false;
		for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
			 ZoneIt;
			 ++ZoneIt)
		{
			if (ZoneIt->GetZoneType() ==
					EBotanicusVisitorZoneType::SalesArea &&
				ZoneIt->ContainsPoint2D(
					DisplayIt->GetActorLocation()))
			{
				bDisplayInsideSalesArea = true;
				break;
			}
		}
		if (!bDisplayInsideSalesArea)
		{
			continue;
		}
		Candidates.Add(*DisplayIt);
	}

	while (Candidates.Num() > 0)
	{
		const int32 CandidateIndex =
			FMath::RandRange(0, Candidates.Num() - 1);
		ABotanicusSalesDisplayActor* Candidate =
			Candidates[CandidateIndex];
		Candidates.RemoveAtSwap(CandidateIndex);

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(BotanicusVisitorDisplayVisibility),
			false,
			this);
		QueryParams.AddIgnoredActor(this);
		FHitResult VisibilityHit;
		const FVector TraceStart =
			GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
		const FVector TraceEnd =
			Candidate->GetActorLocation() +
			FVector(0.0f, 0.0f, 70.0f);
		if (World->LineTraceSingleByChannel(
				VisibilityHit,
				TraceStart,
				TraceEnd,
				ECC_Visibility,
				QueryParams) &&
			VisibilityHit.GetActor() != Candidate)
		{
			continue;
		}

		if (Candidate->TryReserveForVisitor(this))
		{
			RouteResumeLocation = GetActorLocation();
			bReturningToRoute = true;
			TargetDisplay = Candidate;
			VisitorState = EBotanicusVisitorState::Approaching;
			RefreshStatusText();
			ForceNetUpdate();
			return true;
		}
	}
	return false;
}

int32 ABotanicusVisitorCharacter::CountBrowsableDisplays() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ABotanicusSalesDisplayActor> DisplayIt(World);
		 DisplayIt;
		 ++DisplayIt)
	{
		if (!DisplayIt->IsAvailableForVisitorBrowsing(this))
		{
			continue;
		}
		for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
			 ZoneIt;
			 ++ZoneIt)
		{
			if (ZoneIt->GetZoneType() ==
					EBotanicusVisitorZoneType::SalesArea &&
				ZoneIt->ContainsPoint2D(DisplayIt->GetActorLocation()))
			{
				++Count;
				break;
			}
		}
	}
	return Count;
}

int32 ABotanicusVisitorCharacter::ScoreDisplayForPreferences(
	const ABotanicusSalesDisplayActor* Display) const
{
	const FBotanicusItemDefinition* Definition =
		Display
			? Display->GetDisplayedPlantDefinitionForVisitor()
			: nullptr;
	if (!Definition || !Definition->bWholePlant)
	{
		return TNumericLimits<int32>::Lowest();
	}

	int32 Score = Definition->VisitorAppeal;
	Score += Definition->PlantColorTag == PreferredPlantColor
		? 28
		: -6;
	Score += Definition->PlantTypeTag == PreferredPlantType
		? 22
		: -4;
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const int32 EffectivePrice =
		GameState
			? GameState->GetTrendAdjustedSalePrice(*Definition)
			: Definition->SalePrice;
	if (EffectivePrice > ShoppingBudget)
	{
		Score -= 80;
	}
	if (GameState)
	{
		Score += GameState->CountMatchingTrends(*Definition) * 10;
	}
	return FMath::Clamp(Score, -100, 120);
}

void ABotanicusVisitorCharacter::ResetBrowsingState()
{
	static const FName ColorTags[] = {
		TEXT("Green"),
		TEXT("Green"),
		TEXT("Red"),
		TEXT("Pink"),
		TEXT("White"),
		TEXT("Yellow"),
		TEXT("Purple")};
	static const FName TypeTags[] = {
		TEXT("Aromatic"),
		TEXT("Aromatic"),
		TEXT("Flowering"),
		TEXT("Foliage")};

	InspectedDisplays.Reset();
	DesiredInspectionCount = FMath::RandRange(2, 4);
	CompletedInspections = 0;
	PreferredPlantColor =
		ColorTags[FMath::RandRange(
			0,
			UE_ARRAY_COUNT(ColorTags) - 1)];
	PreferredPlantType =
		TypeTags[FMath::RandRange(
			0,
			UE_ARRAY_COUNT(TypeTags) - 1)];
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const float ReputationStars =
		GameState ? GameState->GetShopReputationStars() : 3.0f;
	const int32 ReputationBudgetBonus =
		FMath::RoundToInt((ReputationStars - 3.0f) * 18.0f);
	ShoppingBudget = FMath::Clamp(
		FMath::RandRange(60, 220) + ReputationBudgetBonus,
		45,
		260);
	MinimumAcceptableScore = FMath::RandRange(52, 82);
	CurrentInspectionScore = 0;
	BestMatchingDisplay.Reset();
	BestMatchingScore = TNumericLimits<int32>::Lowest();
	MovementSpeed = FMath::FRandRange(145.0f, 225.0f);
	AcceptanceRadius = FMath::FRandRange(112.0f, 142.0f);
	QueueLongitudinalOffset = FMath::FRandRange(-18.0f, 18.0f);
	QueueLateralOffset = FMath::FRandRange(-32.0f, 32.0f);
	CheckoutQueueArrivalTime = 0.0f;
	CheckoutWaitDuration = 0.0f;
	CheckoutStage = 0;
	if (UCharacterMovementComponent* Movement =
			GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = MovementSpeed;
		Movement->MaxAcceleration =
			FMath::FRandRange(1350.0f, 2100.0f);
		Movement->BrakingDecelerationWalking =
			FMath::FRandRange(900.0f, 1500.0f);
	}
	bCarryingPlant = false;
	bVisitOutcomeRecorded = false;
	CarriedPlantItemKey = NAME_None;
	CarriedPotItemKey = NAME_None;
	SetSpeechLine(FString());
	RefreshCarriedPlantVisuals();
}

void ABotanicusVisitorCharacter::RecordVisitOutcome(
	bool bPurchasedPlant)
{
	if (!HasAuthority() || bVisitOutcomeRecorded)
	{
		return;
	}
	bVisitOutcomeRecorded = true;
	if (!bPurchasedPlant && CompletedInspections <= 0)
	{
		// An empty shop does not generate a negative review. Only a visitor
		// who actually compared at least one offered plant rates the visit.
		return;
	}

	int32 Satisfaction = 42;
	if (bPurchasedPlant)
	{
		const int32 MatchBonus =
			FMath::Clamp(
				(BestMatchingScore - MinimumAcceptableScore) / 2,
				0,
				20);
		Satisfaction = 70 + MatchBonus;

		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusItemCatalogSubsystem* Catalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusItemCatalogSubsystem>()
				: nullptr;
		const FBotanicusItemDefinition* Definition =
			Catalog ? Catalog->FindItem(CarriedPlantItemKey) : nullptr;
		if (Definition)
		{
			if (Definition->PlantQualityTag ==
				TEXT("Exceptional"))
			{
				Satisfaction += 15;
			}
			else if (Definition->PlantQualityTag ==
				TEXT("Beautiful"))
			{
				Satisfaction += 7;
			}
			if (const ABotanicusGameState* GameState =
					GetWorld()
						? GetWorld()->GetGameState<
							ABotanicusGameState>()
						: nullptr)
			{
				Satisfaction +=
					GameState->CountMatchingTrends(*Definition) * 5;
			}
		}
	}
	if (bPurchasedPlant)
	{
		const int32 WaitingPenalty =
			FMath::Clamp(
				FMath::FloorToInt(
					FMath::Max(0.0f, CheckoutWaitDuration - 15.0f) /
					10.0f) *
					2,
				0,
				12);
		Satisfaction -= WaitingPenalty;
	}
	Satisfaction = FMath::Clamp(Satisfaction, 0, 100);
	if (ABotanicusGameState* GameState =
			GetWorld()
				? GetWorld()->GetGameState<ABotanicusGameState>()
				: nullptr)
	{
		GameState->RecordVisitorSatisfaction(Satisfaction);
	}
}

FString ABotanicusVisitorCharacter::ChooseInspectionSpeech(
	const ABotanicusSalesDisplayActor* Display) const
{
	const FBotanicusItemDefinition* Definition =
		Display
			? Display->GetDisplayedPlantDefinitionForVisitor()
			: nullptr;
	if (!Definition)
	{
		return TEXT("Je vais regarder ailleurs...");
	}
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const int32 EffectivePrice =
		GameState
			? GameState->GetTrendAdjustedSalePrice(*Definition)
			: Definition->SalePrice;
	if (EffectivePrice > ShoppingBudget)
	{
		return FString::Printf(
			TEXT("Tres jolie, mais mon budget est de %d credits."),
			ShoppingBudget);
	}
	if (GameState &&
		GameState->CountMatchingTrends(*Definition) >= 2)
	{
		return TEXT("C'est pile dans les tendances du moment !");
	}
	if (Definition->PlantQualityTag == TEXT("Exceptional"))
	{
		return TEXT("Elle est exceptionnelle, elle a ete parfaitement soignee !");
	}
	if (Definition->PlantQualityTag == TEXT("Beautiful"))
	{
		return TEXT("Elle a l'air en pleine sante !");
	}
	const bool bColorMatches =
		Definition->PlantColorTag == PreferredPlantColor;
	const bool bTypeMatches =
		Definition->PlantTypeTag == PreferredPlantType;
	if (bColorMatches && bTypeMatches)
	{
		return TEXT("C'est exactement ce que je cherchais !");
	}
	if (!bColorMatches)
	{
		return FString::Printf(
			TEXT("Dommage, je l'aurais preferee en %s."),
			*VisitorColorLabel(PreferredPlantColor));
	}
	if (!bTypeMatches)
	{
		return FString::Printf(
			TEXT("J'avais plutot envie d'%s."),
			*VisitorTypeLabel(PreferredPlantType));
	}
	return CurrentInspectionScore >= MinimumAcceptableScore
		? TEXT("Quelle jolie plante !")
		: TEXT("Je vais comparer avec les autres.");
}

FString ABotanicusVisitorCharacter::ChooseFinalRefusalSpeech() const
{
	if (ShoppingBudget < 60)
	{
		return FString::Printf(
			TEXT("Je n'ai rien trouve dans mon budget de %d credits."),
			ShoppingBudget);
	}
	return FString::Printf(
		TEXT("Je cherchais surtout %s %s."),
		*VisitorTypeLabel(PreferredPlantType),
		*VisitorColorLabel(PreferredPlantColor));
}

void ABotanicusVisitorCharacter::SetSpeechLine(
	const FString& NewSpeech)
{
	SpeechLine = NewSpeech;
	OnRep_SpeechLine();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void ABotanicusVisitorCharacter::RefreshCarriedPlantVisuals()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* PlantItemDefinition =
		Catalog ? Catalog->FindItem(CarriedPlantItemKey) : nullptr;
	const FBotanicusPlantDefinition* PlantDefinition =
		Plants ? Plants->FindPlantByHarvestItem(CarriedPlantItemKey) : nullptr;

	constexpr float CarryCentreX = 42.0f;
	constexpr float CarryCentreZ = 2.0f;
	float PlantBaseHeight = 18.0f;
	if (CarriedPotVisual)
	{
		CarriedPotVisual->SetVisibility(bCarryingPlant);
		const FName PotKey = CarriedPotItemKey.IsNone()
			? FName(TEXT("SalePot"))
			: CarriedPotItemKey;
		const FBotanicusItemDefinition* PotDefinition =
			Catalog ? Catalog->FindItem(PotKey) : nullptr;
		UStaticMesh* PotMesh = nullptr;
		FVector PotScale = FVector(0.24f, 0.24f, 0.20f);
		FRotator PotRotation = FRotator::ZeroRotator;
		const UStaticMeshComponent* AuthoredPotComponent = nullptr;
		if (PotDefinition)
		{
			if (UClass* PotActorClass =
					PotDefinition->WorldActorClass.LoadSynchronous())
			{
				if (const ABotanicusInteractableActor* PotDefaults =
						Cast<ABotanicusInteractableActor>(
							PotActorClass->GetDefaultObject()))
				{
					AuthoredPotComponent = PotDefaults->Mesh;
				}
			}
			if (AuthoredPotComponent &&
				AuthoredPotComponent->GetStaticMesh())
			{
				PotMesh = AuthoredPotComponent->GetStaticMesh();
				PotScale = AuthoredPotComponent->GetRelativeScale3D();
				PotRotation = AuthoredPotComponent->GetRelativeRotation();
			}
			else
			{
				PotMesh = PotDefinition->WorldMesh.LoadSynchronous();
				PotScale = PotDefinition->WorldScale;
			}
		}
		if (!PotMesh)
		{
			PotMesh = LoadObject<UStaticMesh>(
				nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		}
		if (PotMesh)
		{
			CarriedPotVisual->SetStaticMesh(PotMesh);
			CarriedPotVisual->SetRelativeRotation(PotRotation);
			CarriedPotVisual->SetRelativeScale3D(PotScale);
			CarriedPotVisual->EmptyOverrideMaterials();
			if (AuthoredPotComponent)
			{
				for (int32 MaterialIndex = 0;
					 MaterialIndex < AuthoredPotComponent->GetNumMaterials();
					 ++MaterialIndex)
				{
					CarriedPotVisual->SetMaterial(
						MaterialIndex,
						AuthoredPotComponent->GetMaterial(MaterialIndex));
				}
			}
			const FBox PotBounds = PotMesh->GetBoundingBox().TransformBy(
				FTransform(PotRotation, FVector::ZeroVector, PotScale));
			CarriedPotVisual->SetRelativeLocation(FVector(
				CarryCentreX - PotBounds.GetCenter().X,
				-PotBounds.GetCenter().Y,
				CarryCentreZ - PotBounds.GetCenter().Z));
			PlantBaseHeight = CarryCentreZ +
				PotBounds.GetSize().Z * 0.32f;
		}
	}

	if (!CarriedPlantVisual)
	{
		return;
	}
	CarriedPlantVisual->SetVisibility(bCarryingPlant);
	UStaticMesh* MaturePlantMesh =
		PlantDefinition && !PlantDefinition->MatureGrowthMesh.IsNull()
			? PlantDefinition->MatureGrowthMesh.LoadSynchronous()
			: nullptr;
	if (MaturePlantMesh)
	{
		if (CarriedPlantVisual->GetStaticMesh() != MaturePlantMesh)
		{
			CarriedPlantVisual->SetStaticMesh(MaturePlantMesh);
			CarriedPlantVisual->EmptyOverrideMaterials();
			CarriedPlantMaterial = nullptr;
		}
		const FBox PlantBounds = MaturePlantMesh->GetBoundingBox();
		const float DesiredHeight = FMath::Max(
			0.1f, PlantDefinition->MatureGrowthVisualHeight) * 0.70f;
		const float UniformScale = DesiredHeight /
			FMath::Max(0.01f, PlantBounds.GetSize().Z);
		const FVector PlantCentre = PlantBounds.GetCenter();
		CarriedPlantVisual->SetRelativeScale3D(FVector(UniformScale));
		CarriedPlantVisual->SetRelativeLocation(FVector(
			CarryCentreX - PlantCentre.X * UniformScale,
			-PlantCentre.Y * UniformScale,
			PlantBaseHeight - PlantBounds.Min.Z * UniformScale));
		return;
	}

	if (UStaticMesh* FallbackMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		if (CarriedPlantVisual->GetStaticMesh() != FallbackMesh)
		{
			CarriedPlantVisual->SetStaticMesh(FallbackMesh);
			CarriedPlantVisual->EmptyOverrideMaterials();
			CarriedPlantMaterial = nullptr;
		}
	}
	FVector PlantScale(0.29f, 0.29f, 0.42f);
	if (PlantItemDefinition)
	{
		if (PlantItemDefinition->PlantTypeTag == TEXT("Flowering"))
		{
			PlantScale = FVector(0.24f, 0.24f, 0.50f);
		}
		else if (PlantItemDefinition->PlantTypeTag == TEXT("Foliage"))
		{
			PlantScale = FVector(0.40f, 0.35f, 0.32f);
		}
		else if (PlantItemDefinition->PlantColorTag == TEXT("Purple"))
		{
			PlantScale = FVector(0.21f, 0.21f, 0.52f);
		}
		if (!CarriedPlantMaterial)
		{
			CarriedPlantMaterial = CarriedPlantVisual->
				CreateAndSetMaterialInstanceDynamic(0);
		}
		if (CarriedPlantMaterial)
		{
			CarriedPlantMaterial->SetVectorParameterValue(
				TEXT("Color"),
				VisitorPlantVisualColor(
					PlantItemDefinition->PlantColorTag));
		}
	}
	CarriedPlantVisual->SetRelativeLocation(FVector(
		CarryCentreX, 0.0f, PlantBaseHeight + 32.0f));
	CarriedPlantVisual->SetRelativeScale3D(PlantScale);
}

bool ABotanicusVisitorCharacter::IsInsideSalesArea() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		if (ZoneIt->GetZoneType() ==
				EBotanicusVisitorZoneType::SalesArea &&
			ZoneIt->ContainsPoint2D(GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

void ABotanicusVisitorCharacter::FollowRoute(float DeltaSeconds)
{
	if (bReturningToRoute)
	{
		MoveTowards(RouteResumeLocation, DeltaSeconds);
		if (FVector::DistSquared2D(
				GetActorLocation(),
				RouteResumeLocation) <=
			FMath::Square(AcceptanceRadius))
		{
			bReturningToRoute = false;
		}
		return;
	}

	if (!RoutePoints.IsValidIndex(RouteWaypointIndex))
	{
		SwitchToDirectReturnRoute();
		return;
	}

	const int32 DestinationWaypointIndex = RouteWaypointIndex;
	MoveTowards(RoutePoints[RouteWaypointIndex], DeltaSeconds);
	if (FVector::DistSquared2D(
			GetActorLocation(),
			RoutePoints[RouteWaypointIndex]) <=
		FMath::Square(AcceptanceRadius))
	{
		++RouteWaypointIndex;
		if (DestinationWaypointIndex == CheckoutWaypointIndex && SpecialOrderId.IsValid())
		{
			VisitorState = EBotanicusVisitorState::SpecialOrderApproaching;
			RefreshStatusText();
			ForceNetUpdate();
		}
		else if (DestinationWaypointIndex == CheckoutWaypointIndex &&
			bPlantSelected)
		{
			VisitorState = EBotanicusVisitorState::CheckoutQueue;
			CheckoutQueueDestination =
				RoutePoints[DestinationWaypointIndex];
			CheckoutQueueArrivalTime =
				GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			CheckoutWaitDuration = 0.0f;
			CheckoutStage = 0;
			GetCharacterMovement()->StopMovementImmediately();
			SetSpeechLine(TEXT("Je vais faire la queue pour payer."));
			RefreshStatusText();
			ForceNetUpdate();
		}
	}
}

void ABotanicusVisitorCharacter::SwitchToDirectReturnRoute()
{
	RecordVisitOutcome(false);
	if (TargetDisplay)
	{
		TargetDisplay->NotifyVisitorEnded(this);
		TargetDisplay = nullptr;
	}
	RoutePoints = DirectReturnRoute;
	RouteWaypointIndex = 0;
	CheckoutWaypointIndex = INDEX_NONE;
	bReturningToRoute = false;
	VisitorState = EBotanicusVisitorState::Leaving;
	LastMovementLocation = GetActorLocation();
	StuckDuration = 0.0f;
	RefreshStatusText();
	ForceNetUpdate();
}

void ABotanicusVisitorCharacter::UpdateStuckDetection(
	float DeltaSeconds)
{
	if (VisitorState == EBotanicusVisitorState::SpecialOrderWaiting ||
		VisitorState == EBotanicusVisitorState::Inspecting ||
		VisitorState == EBotanicusVisitorState::CheckoutQueue ||
		VisitorState == EBotanicusVisitorState::Paying ||
		VisitorState == EBotanicusVisitorState::SelfCheckout ||
		VisitorState == EBotanicusVisitorState::Queued)
	{
		LastMovementLocation = GetActorLocation();
		StuckDuration = 0.0f;
		return;
	}

	if (FVector::DistSquared2D(
			GetActorLocation(),
			LastMovementLocation) < 4.0f)
	{
		StuckDuration += DeltaSeconds;
	}
	else
	{
		LastMovementLocation = GetActorLocation();
		StuckDuration = 0.0f;
	}

	if (StuckDuration >= 5.0f)
	{
		Destroy();
	}
}

void ABotanicusVisitorCharacter::BeginDeparture(
	bool bKeepPurchasedPlant)
{
	RecordVisitOutcome(bKeepPurchasedPlant);
	if (TargetDisplay)
	{
		TargetDisplay->NotifyVisitorEnded(this);
		TargetDisplay = nullptr;
	}
	AssignedSelfCheckout = nullptr;
	SelfCheckoutElapsed = 0.0f;
	SetSpeechLine(FString());
	if (!bKeepPurchasedPlant)
	{
		bCarryingPlant = false;
		CarriedPlantItemKey = NAME_None;
		CarriedPotItemKey = NAME_None;
		RefreshCarriedPlantVisuals();
	}
	VisitorState = EBotanicusVisitorState::Leaving;
	LastMovementLocation = GetActorLocation();
	StuckDuration = 0.0f;
	RefreshStatusText();
}

void ABotanicusVisitorCharacter::BeginShopClosureDeparture()
{
	if (!HasAuthority() ||
		VisitorState == EBotanicusVisitorState::Leaving)
	{
		return;
	}

	if (SpecialOrderId.IsValid())
	{
		if (auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld())) Orders->CustomerDeparted(SpecialOrderId, this);
		return;
	}
	const EBotanicusVisitorState PreviousState = VisitorState;
	const bool bWasInsideShop =
		IsInsideSalesArea() ||
		PreviousState == EBotanicusVisitorState::Approaching ||
		PreviousState == EBotanicusVisitorState::Inspecting ||
		PreviousState == EBotanicusVisitorState::CheckoutQueue ||
		PreviousState == EBotanicusVisitorState::Paying ||
		PreviousState == EBotanicusVisitorState::SelfCheckout;
	const bool bMustTurnAround =
		PreviousState == EBotanicusVisitorState::Queued ||
		(!bWasInsideShop &&
			RouteWaypointIndex <= CheckoutWaypointIndex);
	const bool bMustResumeRoute =
		bReturningToRoute ||
		PreviousState == EBotanicusVisitorState::Approaching ||
		PreviousState == EBotanicusVisitorState::Inspecting;

	// A closure is not a failed shopping visit: release any reserved plant
	// without recording a negative satisfaction result.
	if (TargetDisplay)
	{
		TargetDisplay->NotifyVisitorEnded(this);
		TargetDisplay = nullptr;
	}
	bPlantSelected = false;
	bReturningToRoute = false;
	bCarryingPlant = false;
	CarriedPlantItemKey = NAME_None;
	CarriedPotItemKey = NAME_None;
	CheckoutStage = 0;
	AssignedSelfCheckout = nullptr;
	SelfCheckoutElapsed = 0.0f;
	RefreshCarriedPlantVisuals();

	if (bMustTurnAround && RoutePoints.Num() > 0)
	{
		const TArray<FVector> ArrivalRoute = RoutePoints;
		const int32 PreviousWaypointIndex =
			PreviousState == EBotanicusVisitorState::Queued
				? 0
				: FMath::Clamp(
					RouteWaypointIndex - 1,
					0,
					ArrivalRoute.Num() - 1);
		RoutePoints.Reset(PreviousWaypointIndex + 1);
		for (int32 PointIndex = PreviousWaypointIndex;
			 PointIndex >= 0;
			 --PointIndex)
		{
			RoutePoints.Add(ArrivalRoute[PointIndex]);
		}
		RouteWaypointIndex = 0;
		CheckoutWaypointIndex = INDEX_NONE;
		SetSpeechLine(TEXT("Oh, le magasin ferme. Je fais demi-tour."));
	}
	else
	{
		if (bMustResumeRoute && RoutePoints.Num() > 0)
		{
			const TArray<FVector> OriginalRoute = RoutePoints;
			RoutePoints.Reset();
			RoutePoints.Add(RouteResumeLocation);
			for (int32 PointIndex =
					 FMath::Clamp(
						 RouteWaypointIndex,
						 0,
						 OriginalRoute.Num());
				 PointIndex < OriginalRoute.Num();
				 ++PointIndex)
			{
				RoutePoints.Add(OriginalRoute[PointIndex]);
			}
			RouteWaypointIndex = 0;
			CheckoutWaypointIndex = INDEX_NONE;
		}
		// Visitors already browsing keep following the authored circuit to
		// the checkout-side exit, but skip inspection and payment.
		SetSpeechLine(TEXT("Le magasin ferme, je vais vers la sortie."));
	}

	VisitorState = EBotanicusVisitorState::Leaving;
	LastMovementLocation = GetActorLocation();
	StuckDuration = 0.0f;
	RefreshStatusText();
	ForceNetUpdate();
}

void ABotanicusVisitorCharacter::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed && SpecialOrderId.IsValid())
	{
		if (auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld())) Orders->CustomerDeparted(SpecialOrderId, this);
	}
	if (HasAuthority() && TargetDisplay)
	{
		TargetDisplay->NotifyVisitorEnded(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABotanicusVisitorCharacter::RefreshStatusText()
{
	if (!StatusText)
	{
		return;
	}

	switch (VisitorState)
	{
	case EBotanicusVisitorState::SpecialOrderApproaching:
		StatusText->SetText(FText::FromString(TEXT("COMMANDE SPÉCIALE\nVERS LE COMPTOIR")));
		break;
	case EBotanicusVisitorState::SpecialOrderWaiting:
		StatusText->SetText(FText::FromString(TEXT("COMMANDE SPÉCIALE\nEN ATTENTE")));
		break;
	case EBotanicusVisitorState::Queued:
		StatusText->SetText(
			FText::FromString(TEXT("VISITEUR\nFILE D'ATTENTE")));
		break;
	case EBotanicusVisitorState::FollowingRoute:
		StatusText->SetText(
			FText::FromString(
				bCarryingPlant
					? TEXT("VISITEUR\nPORTE SA PLANTE\nVERS LA CAISSE")
					: TEXT("VISITEUR\nCIRCUIT DE VISITE")));
		break;
	case EBotanicusVisitorState::Approaching:
		StatusText->SetText(FText::FromString(TEXT("VISITEUR\nEN ROUTE")));
		break;
	case EBotanicusVisitorState::Inspecting:
		StatusText->SetText(FText::FromString(TEXT("VISITEUR\nREGARDE LA PLANTE")));
		break;
	case EBotanicusVisitorState::CheckoutQueue:
		StatusText->SetText(
			FText::FromString(TEXT("VISITEUR\nFILE DE LA CAISSE")));
		break;
	case EBotanicusVisitorState::Paying:
		StatusText->SetText(
			FText::FromString(
				CheckoutStage == 0
					? TEXT("VISITEUR\nATTEND LE SCAN")
					: TEXT("VISITEUR\nATTEND LE PAIEMENT")));
		break;
	case EBotanicusVisitorState::SelfCheckout:
		StatusText->SetText(
			FText::FromString(
				CheckoutStage == 0
					? TEXT("VISITEUR\nCAISSE AUTO : SCAN")
					: TEXT("VISITEUR\nCAISSE AUTO : PAIEMENT")));
		break;
	case EBotanicusVisitorState::Leaving:
		StatusText->SetText(
			FText::FromString(
				bCarryingPlant
					? TEXT("VISITEUR\nRETOUR PARKING\nAVEC SON ACHAT")
					: TEXT("VISITEUR\nRETOUR PARKING")));
		break;
	}
}

void ABotanicusVisitorCharacter::OnRep_VisitorState()
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,
		IsWaitingForSpecialOrder() ? ECR_Block : ECR_Ignore);
	RefreshStatusText();
}

void ABotanicusVisitorCharacter::OnRep_SpeechLine()
{
	if (!SpeechBubbleComponent)
	{
		return;
	}
	SpeechBubbleComponent->SetVisibility(!SpeechLine.IsEmpty());
	if (UBotanicusVisitorSpeechBubbleWidget* Bubble =
			Cast<UBotanicusVisitorSpeechBubbleWidget>(
				SpeechBubbleComponent->GetUserWidgetObject()))
	{
		Bubble->SetSpeech(SpeechLine);
	}
}

void ABotanicusVisitorCharacter::OnRep_CarriedPlant()
{
	RefreshCarriedPlantVisuals();
	RefreshStatusText();
}

void ABotanicusVisitorCharacter::BeginSpecialOrderVisit(FGuid Id, ABotanicusCashRegisterActor* Counter)
{
	if (!HasAuthority() || !Counter || !PurchaseRoute.IsValidIndex(PurchaseCheckoutWaypointIndex)) return;
	int32 CounterSlot = 0;
	for (TActorIterator<ABotanicusVisitorCharacter> It(GetWorld()); It; ++It)
		if (*It != this && It->SpecialOrderCounter == Counter && It->SpecialOrderId.IsValid()) ++CounterSlot;
	SpecialOrderId = Id;
	bSpecialOrderBellRung = false;
	NextSpecialOrderBellTime = 0.0f;
	SpecialOrderCounter = Counter;
	SpecialOrderStandLocation = Counter->GetCustomerStandLocation() - Counter->GetActorRightVector() * (170.0f + 100.0f * CounterSlot);
	// RoutePoints still contains the authored parking-to-sales arrival route here.
	// Keep every one of those waypoints, then append the normal sales-to-checkout
	// and checkout-to-parking route. Replacing it with PurchaseRoute would make
	// the customer walk straight from the parking to the sales area through walls.
	const int32 ArrivalPointCount = RoutePoints.Num();
	for (int32 Index = 1; Index < PurchaseRoute.Num(); ++Index)
	{
		RoutePoints.Add(PurchaseRoute[Index]);
	}
	RouteWaypointIndex = 0;
	CheckoutWaypointIndex = ArrivalPointCount - 1 + PurchaseCheckoutWaypointIndex;
	bPlantSelected = false;
	bReturningToRoute = false;
	VisitorState = EBotanicusVisitorState::FollowingRoute;
	SetSpeechLine(TEXT("Je voudrais passer une commande spéciale au comptoir."));
	OnRep_VisitorState();
	ForceNetUpdate();
}

bool ABotanicusVisitorCharacter::TryRingSpecialOrderBell()
{
	if (!HasAuthority() || !IsWaitingForSpecialOrder() || !SpecialOrderId.IsValid() ||
		!SpecialOrderCounter.IsValid() || bSpecialOrderBellRung) return false;
	bSpecialOrderBellRung = true;
	NextSpecialOrderBellTime = GetWorld()->GetTimeSeconds() + 60.0f;
	// An event, not a replicated-state side effect: late joins never replay a past ring.
	MulticastPlaySpecialOrderBell(SpecialOrderCounter->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f));
	return true;
}

bool ABotanicusVisitorCharacter::TryRingSpecialOrderBellReminder()
{
	if (!HasAuthority() || !IsWaitingForSpecialOrder() || !bSpecialOrderBellRung ||
		!SpecialOrderCounter.IsValid() || GetWorld()->GetTimeSeconds() < NextSpecialOrderBellTime) return false;
	const auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld());
	const auto* Order = Orders ? Orders->FindOrder(SpecialOrderId) : nullptr;
	if (!Order || Order->Customer != this || Order->Status != EBotanicusSpecialOrderStatus::Offered ||
		Orders->GetRemainingSeconds(*Order) <= 0.0f) return false;
	// Do not extend the customer's patience or catch up missed reminders in a burst.
	NextSpecialOrderBellTime = GetWorld()->GetTimeSeconds() + 60.0f;
	MulticastPlaySpecialOrderBell(SpecialOrderCounter->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f));
	return true;
}

void ABotanicusVisitorCharacter::MulticastPlaySpecialOrderBell_Implementation(FVector_NetQuantize BellLocation)
{
	if (GetNetMode() != NM_DedicatedServer && SpecialOrderBellSound && SpecialOrderBellVolume > 0.0f)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SpecialOrderBellSound, BellLocation,
			SpecialOrderBellVolume);
	}
}

void ABotanicusVisitorCharacter::FinishSpecialOrderVisit(bool bSucceeded)
{
	if (!HasAuthority()) return;
	bVisitOutcomeRecorded = true; // The shared order component records the one review.
	SpecialOrderId.Invalidate();
	SpecialOrderCounter = nullptr;
	if (RoutePoints.IsValidIndex(RouteWaypointIndex)) BeginDeparture(false);
	else SwitchToDirectReturnRoute();
	OnRep_VisitorState();
	SetSpeechLine(bSucceeded ? TEXT("Merci à toute l'équipe ! C'est exactement ce qu'il me fallait.") : TEXT("J'ai trop attendu… Je dois partir."));
}

void ABotanicusVisitorCharacter::RefreshSpecialOrderSpeech()
{
	if (!HasAuthority()) return;
	if (const auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld()))
		SetSpeechLine(Orders->GetRequestText(SpecialOrderId).ToString());
}

FBotanicusInteractionPrompt ABotanicusVisitorCharacter::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	if (const auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld()))
	{
		if (const auto* Order = Orders->FindOrder(SpecialOrderId))
		{
			Prompt.TargetName = FText::FromString(FString::Printf(
				TEXT("Client — commande spéciale #%d"), Order->Number));
			Prompt.ActionText = FText::FromString(Order->Status == EBotanicusSpecialOrderStatus::Offered
				? TEXT("POUR PRENDRE LA COMMANDE") : (Prompt.bCanInteract ? TEXT("POUR LIVRER LA PLANTE") : TEXT("PLANTE SÉLECTIONNÉE NON CONFORME")));
		}
	}
	return Prompt;
}

bool ABotanicusVisitorCharacter::CanInteract_Implementation(AActor* Interactor) const
{
	const auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld());
	return IsWaitingForSpecialOrder() && Orders && Orders->CanInteract(SpecialOrderId, Interactor);
}

void ABotanicusVisitorCharacter::Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority())
		if (auto* Orders = UBotanicusSpecialOrderComponent::Get(GetWorld())) Orders->Interact(SpecialOrderId, Interactor);
}
