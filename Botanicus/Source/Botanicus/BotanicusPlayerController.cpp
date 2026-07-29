// Copyright Epic Games, Inc. All Rights Reserved.


#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "LandscapeProxy.h"
#include "TimerManager.h"
#include "BotanicusCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Botanicus.h"
#include "Online/BotanicusMultiplayerSubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SVirtualJoystick.h"

ABotanicusPlayerController::ABotanicusPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABotanicusCameraManager::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ABotanicusPlayerController::BotanicusHost(int32 MaxPlayers)
{
	if (UBotanicusMultiplayerSubsystem* Multiplayer = GetBotanicusMultiplayerSubsystem())
	{
		Multiplayer->HostSession(
			MaxPlayers,
			TEXT("/Game/FirstPerson/Lvl_FirstPerson"));
	}
}

void ABotanicusPlayerController::BotanicusFind()
{
	if (UBotanicusMultiplayerSubsystem* Multiplayer = GetBotanicusMultiplayerSubsystem())
	{
		Multiplayer->FindSessions();
	}
}

void ABotanicusPlayerController::BotanicusJoin(int32 ResultIndex)
{
	if (UBotanicusMultiplayerSubsystem* Multiplayer = GetBotanicusMultiplayerSubsystem())
	{
		Multiplayer->JoinSessionByIndex(ResultIndex);
	}
}

void ABotanicusPlayerController::BotanicusLeave()
{
	if (UBotanicusMultiplayerSubsystem* Multiplayer = GetBotanicusMultiplayerSubsystem())
	{
		Multiplayer->LeaveSession();
	}
}

void ABotanicusPlayerController::BotanicusInvite()
{
	if (UBotanicusMultiplayerSubsystem* Multiplayer = GetBotanicusMultiplayerSubsystem())
	{
		Multiplayer->ShowSteamInviteOverlay();
	}
}

void ABotanicusPlayerController::BotanicusOnlineStatus()
{
	if (UBotanicusMultiplayerSubsystem* Multiplayer = GetBotanicusMultiplayerSubsystem())
	{
		const UEnum* StateEnum = StaticEnum<EBotanicusOnlineState>();
		const FString OnlineStateName = StateEnum
			? StateEnum->GetNameStringByValue(static_cast<int64>(Multiplayer->GetOnlineState()))
			: TEXT("Unknown");

		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Online subsystem=%s SteamAvailable=%s ActiveSession=%s State=%s"),
			*Multiplayer->GetOnlineSubsystemName(),
			Multiplayer->IsSteamAvailable() ? TEXT("true") : TEXT("false"),
			Multiplayer->HasActiveSession() ? TEXT("true") : TEXT("false"),
			*OnlineStateName);
	}
}

void ABotanicusPlayerController::BotanicusTestBuildingGrouping()
{
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!IsStructuralBuildingActor(Actor))
		{
			continue;
		}

		const TArray<AActor*> Group = BuildCompleteBuildingGroup(Actor);
		int32 StructuralCount = 0;
		for (AActor* BuildingActor : Group)
		{
			StructuralCount += IsStructuralBuildingActor(BuildingActor) ? 1 : 0;
		}

		const FVector Pivot = CalculateBuildingGroupPivot(Group);
		float LandscapeHeight = 0.0f;
		const bool bLandscapeFound = FindLandscapeHeight(
			FVector2D(Pivot.X, Pivot.Y),
			LandscapeHeight);

		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Whole-building grouping test: seed=%s total=%d structural=%d contents=%d landscape=%s groundOffset=%.1f"),
			*Actor->GetName(),
			Group.Num(),
			StructuralCount,
			Group.Num() - StructuralCount,
			bLandscapeFound ? TEXT("found") : TEXT("missing"),
			bLandscapeFound ? Pivot.Z - LandscapeHeight : 0.0f);
		return;
	}

	UE_LOG(
		LogBotanicus,
		Warning,
		TEXT("Whole-building grouping test found no EBS structural actor."));
}

void ABotanicusPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ForceFirstPersonView();
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::ForceFirstPersonView);
	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogBotanicus, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ABotanicusPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetBuildingGroupHighlighted(false);
	ClearServerBuildingGroupMove();

	if (IsValid(BuildingCameraActor))
	{
		BuildingCameraActor->Destroy();
		BuildingCameraActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ABotanicusPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateBuildingGroupPreview(DeltaTime);
}

void ABotanicusPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAxis(
		TEXT("MoveForward"),
		this,
		&ABotanicusPlayerController::MoveBuildingCameraForward);
	InputComponent->BindAxis(
		TEXT("MoveRight"),
		this,
		&ABotanicusPlayerController::MoveBuildingCameraRight);

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ABotanicusPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// EBS binds V directly and cycles first person -> top down -> third person.
	// Botanicus never exposes that three-state cycle.
	if (Params.Key == EKeys::V)
	{
		return true;
	}

	// T replaces EBS' radial/square-menu shortcut and is the sole top-down
	// camera toggle. Consume every T event so the original Blueprint binding
	// cannot also execute.
	if (Params.Key == EKeys::T)
	{
		if (Params.Event == IE_Pressed)
		{
			if (bBuildingTopDownViewActive && LocalBuildingGroup.Num() > 0)
			{
				CancelBuildingGroupMove();
			}
			ToggleBuildingTopDownView();
		}

		return true;
	}

	if (bBuildingTopDownViewActive)
	{
		if (Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed)
		{
			if (LocalBuildingGroup.Num() > 0)
			{
				ConfirmBuildingGroupMove();
			}
			else
			{
				TrySelectBuildingGroup();
			}
			return true;
		}

		if (Params.Key == EKeys::E &&
			Params.Event == IE_Pressed &&
			LocalBuildingGroup.Num() > 0)
		{
			ConfirmBuildingGroupMove();
			return true;
		}

		if ((Params.Key == EKeys::RightMouseButton ||
			 Params.Key == EKeys::Escape) &&
			Params.Event == IE_Pressed &&
			LocalBuildingGroup.Num() > 0)
		{
			CancelBuildingGroupMove();
			return true;
		}

		if (Params.Event == IE_Pressed &&
			LocalBuildingGroup.Num() > 0)
		{
			if (Params.Key == EKeys::MouseScrollUp)
			{
				RotateBuildingGroup(1.0f);
				return true;
			}
			if (Params.Key == EKeys::MouseScrollDown)
			{
				RotateBuildingGroup(-1.0f);
				return true;
			}
		}
	}

	return Super::InputKey(Params);
}

void ABotanicusPlayerController::ToggleBuildingTopDownView()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bBuildingTopDownViewActive)
	{
		ExitBuildingTopDownView();
	}
	else
	{
		EnterBuildingTopDownView();
	}
}

void ABotanicusPlayerController::EnterBuildingTopDownView()
{
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return;
	}

	if (!IsValid(BuildingCameraActor))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.ObjectFlags |= RF_Transient;

		BuildingCameraActor = World->SpawnActor<ACameraActor>(
			ControlledPawn->GetActorLocation(),
			FRotator(-90.0f, 0.0f, 0.0f),
			SpawnParameters);
	}

	if (!IsValid(BuildingCameraActor))
	{
		UE_LOG(LogBotanicus, Error, TEXT("Could not create the top-down building camera."));
		return;
	}

	const FVector CameraLocation =
		ControlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, BuildingCameraHeight);
	BuildingCameraActor->SetActorLocationAndRotation(
		CameraLocation,
		FRotator(-90.0f, 0.0f, 0.0f));

	if (UCameraComponent* Camera = BuildingCameraActor->GetCameraComponent())
	{
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
		Camera->SetFieldOfView(60.0f);
	}

	// Advance EBS from first-person to its top-down/cursor-trace state.
	AdvanceEbsViewMode();
	bBuildingTopDownViewActive = true;

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	SetViewTargetWithBlend(
		BuildingCameraActor,
		BuildingCameraBlendTime,
		EViewTargetBlendFunction::VTBlend_Cubic);

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Building top-down view enabled. Character movement locked; camera pan active."));
}

void ABotanicusPlayerController::ExitBuildingTopDownView()
{
	APawn* ControlledPawn = GetPawn();

	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}

	// EBS cycles top down -> third person -> first person. Advancing twice
	// returns its internal state directly to first person.
	AdvanceEbsViewMode();
	AdvanceEbsViewMode();
	bBuildingTopDownViewActive = false;

	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());

	if (ControlledPawn)
	{
		ForceFirstPersonView();
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Building top-down view disabled. Returned directly to first person."));
}

void ABotanicusPlayerController::ForceFirstPersonView()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!BotanicusCharacter)
	{
		return;
	}

	UCameraComponent* FirstPersonCamera =
		BotanicusCharacter->GetFirstPersonCameraComponent();

	TInlineComponentArray<UCameraComponent*> CharacterCameras(
		BotanicusCharacter);
	for (UCameraComponent* Camera : CharacterCameras)
	{
		if (!Camera)
		{
			continue;
		}

		if (Camera == FirstPersonCamera)
		{
			Camera->Activate(true);
		}
		else
		{
			Camera->Deactivate();
		}
	}

	if (FirstPersonCamera)
	{
		SetViewTargetWithBlend(
			BotanicusCharacter,
			BuildingCameraBlendTime,
			EViewTargetBlendFunction::VTBlend_Cubic);

		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Native first-person camera forced; third-person cameras disabled."));
	}
}

void ABotanicusPlayerController::AdvanceEbsViewMode()
{
	UFunction* ChangeViewModeFunction = FindFunction(TEXT("ChangeViewMode"));
	if (!ChangeViewModeFunction)
	{
		UE_LOG(
			LogBotanicus,
			Warning,
			TEXT("EBS ChangeViewMode function was not found on %s."),
			*GetClass()->GetName());
		return;
	}

	TArray<uint8, TInlineAllocator<64>> Parameters;
	Parameters.SetNumZeroed(ChangeViewModeFunction->ParmsSize);
	ProcessEvent(
		ChangeViewModeFunction,
		Parameters.Num() > 0 ? Parameters.GetData() : nullptr);
}

void ABotanicusPlayerController::MoveBuildingCameraForward(float AxisValue)
{
	if (!bBuildingTopDownViewActive ||
		!IsValid(BuildingCameraActor) ||
		FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	BuildingCameraActor->AddActorWorldOffset(
		FVector(AxisValue * BuildingCameraPanSpeed * DeltaSeconds, 0.0f, 0.0f));
}

void ABotanicusPlayerController::MoveBuildingCameraRight(float AxisValue)
{
	if (!bBuildingTopDownViewActive ||
		!IsValid(BuildingCameraActor) ||
		FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	BuildingCameraActor->AddActorWorldOffset(
		FVector(0.0f, AxisValue * BuildingCameraPanSpeed * DeltaSeconds, 0.0f));
}

void ABotanicusPlayerController::TrySelectBuildingGroup()
{
	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit) || !IsValid(CursorHit.GetActor()))
	{
		return;
	}

	ServerBeginBuildingGroupMove(CursorHit.GetActor());
}

void ABotanicusPlayerController::ConfirmBuildingGroupMove()
{
	if (LocalBuildingGroup.Num() > 0)
	{
		ServerConfirmBuildingGroupMove();
	}
}

void ABotanicusPlayerController::CancelBuildingGroupMove()
{
	if (LocalBuildingGroup.Num() > 0)
	{
		ServerCancelBuildingGroupMove();
	}
}

void ABotanicusPlayerController::RotateBuildingGroup(float Direction)
{
	if (LocalBuildingGroup.Num() == 0 || FMath::IsNearlyZero(Direction))
	{
		return;
	}

	LocalBuildingYaw = FMath::UnwindDegrees(
		LocalBuildingYaw + Direction * BuildingRotationStep);
	BuildingPreviewUpdateAccumulator =
		1.0f / FMath::Max(BuildingPreviewUpdatesPerSecond, 1.0f);
}

void ABotanicusPlayerController::UpdateBuildingGroupPreview(float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		!bBuildingTopDownViewActive ||
		LocalBuildingGroup.Num() == 0)
	{
		return;
	}

	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit))
	{
		return;
	}

	LocalBuildingPivot = CursorHit.ImpactPoint;

	const FQuat DeltaRotation =
		FRotator(0.0f, LocalBuildingYaw, 0.0f).Quaternion();
	for (int32 Index = 0;
		 Index < LocalBuildingGroup.Num() &&
		 Index < LocalBuildingOriginalTransforms.Num();
		 ++Index)
	{
		AActor* Actor = LocalBuildingGroup[Index];
		if (!IsValid(Actor))
		{
			continue;
		}

		const FTransform& Original = LocalBuildingOriginalTransforms[Index];
		const FVector RelativeLocation =
			Original.GetLocation() - LocalBuildingOriginalPivot;
		FTransform PreviewTransform = Original;
		PreviewTransform.SetLocation(
			LocalBuildingPivot + DeltaRotation.RotateVector(RelativeLocation));
		PreviewTransform.SetRotation(
			DeltaRotation * Original.GetRotation());
		Actor->SetActorTransform(
			PreviewTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	BuildingPreviewUpdateAccumulator += DeltaTime;
	const float UpdateInterval =
		1.0f / FMath::Max(BuildingPreviewUpdatesPerSecond, 1.0f);
	if (BuildingPreviewUpdateAccumulator >= UpdateInterval)
	{
		BuildingPreviewUpdateAccumulator = 0.0f;
		ServerUpdateBuildingGroupMove(LocalBuildingPivot, LocalBuildingYaw);
	}
}

void ABotanicusPlayerController::SetBuildingGroupHighlighted(bool bHighlighted)
{
	for (AActor* Actor : LocalBuildingGroup)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
		for (UPrimitiveComponent* Primitive : PrimitiveComponents)
		{
			if (Primitive)
			{
				Primitive->SetRenderCustomDepth(bHighlighted);
				Primitive->SetCustomDepthStencilValue(bHighlighted ? 1 : 0);
			}
		}
	}
}

bool ABotanicusPlayerController::TraceTopDownCursor(FHitResult& OutHit) const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	FVector WorldOrigin;
	FVector WorldDirection;
	if (!GetMousePosition(MouseX, MouseY) ||
		!DeprojectScreenPositionToWorld(
			MouseX,
			MouseY,
			WorldOrigin,
			WorldDirection))
	{
		return false;
	}

	// While a whole building is being moved, the cursor ray is projected onto
	// the Landscape itself rather than using the first visibility collision.
	// Trees, roofs and foliage can otherwise pull the building into the air.
	if (LocalBuildingGroup.Num() > 0 &&
		!FMath::IsNearlyZero(WorldDirection.Z))
	{
		float SurfaceZ =
			LocalBuildingOriginalPivot.Z - LocalBuildingGroundOffset;
		FVector SurfacePoint = LocalBuildingOriginalPivot;
		bool bFoundLandscape = false;

		// Perspective rays change X/Y with height. A few iterations converge
		// the cursor position onto sloped Landscape terrain.
		for (int32 Iteration = 0; Iteration < 4; ++Iteration)
		{
			const float DistanceAlongRay =
				(SurfaceZ - WorldOrigin.Z) / WorldDirection.Z;
			if (DistanceAlongRay < 0.0f)
			{
				return false;
			}

			SurfacePoint =
				WorldOrigin + WorldDirection * DistanceAlongRay;
			float LandscapeZ = 0.0f;
			if (!FindLandscapeHeight(
				FVector2D(SurfacePoint.X, SurfacePoint.Y),
				LandscapeZ))
			{
				bFoundLandscape = false;
				break;
			}

			SurfaceZ = LandscapeZ;
			bFoundLandscape = true;
		}

		if (bFoundLandscape)
		{
			SurfacePoint.Z = SurfaceZ + LocalBuildingGroundOffset;
			OutHit = FHitResult();
			OutHit.bBlockingHit = true;
			OutHit.Location = SurfacePoint;
			OutHit.ImpactPoint = SurfacePoint;
			return true;
		}
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusTopDownBuildingTrace),
		true);
	QueryParams.AddIgnoredActor(GetPawn());
	for (AActor* Actor : LocalBuildingGroup)
	{
		QueryParams.AddIgnoredActor(Actor);
	}

	return GetWorld() &&
		GetWorld()->LineTraceSingleByChannel(
			OutHit,
			WorldOrigin,
			WorldOrigin + WorldDirection * 1000000.0f,
			ECC_Visibility,
			QueryParams);
}

bool ABotanicusPlayerController::FindLandscapeHeight(
	const FVector2D& WorldXY,
	float& OutHeight) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<ALandscapeProxy> LandscapeIt(World);
		 LandscapeIt;
		 ++LandscapeIt)
	{
		const TOptional<float> Height =
			LandscapeIt->GetHeightAtLocation(
				FVector(WorldXY.X, WorldXY.Y, 0.0f));
		if (Height.IsSet())
		{
			OutHeight = Height.GetValue();
			return true;
		}
	}

	return false;
}

void ABotanicusPlayerController::ServerBeginBuildingGroupMove_Implementation(
	AActor* HitActor)
{
	if (!IsValid(HitActor) || ServerBuildingGroup.Num() > 0)
	{
		return;
	}

	TArray<AActor*> Group = BuildCompleteBuildingGroup(HitActor);
	if (Group.Num() == 0)
	{
		ClientMessage(
			TEXT("Aucun bâtiment structurel n'a été trouvé pour cette sélection."));
		return;
	}

	AActor* OwnershipAnchor = nullptr;
	for (AActor* Actor : Group)
	{
		if (IsStructuralBuildingActor(Actor))
		{
			OwnershipAnchor = Actor;
			break;
		}
	}

	if (!OwnershipAnchor || !IsBuildingOwnedByThisPlayer(OwnershipAnchor))
	{
		ClientMessage(TEXT("Vous ne pouvez déplacer que vos propres bâtiments."));
		return;
	}

	ServerBuildingGroup.Reset(Group.Num());
	ServerBuildingOriginalTransforms.Reset(Group.Num());
	for (AActor* Actor : Group)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		ServerBuildingGroup.Add(Actor);
		ServerBuildingOriginalTransforms.Add(Actor->GetActorTransform());
		Actor->SetReplicates(true);
		Actor->SetReplicateMovement(true);
		Actor->ForceNetUpdate();
	}

	ServerBuildingOriginalPivot =
		CalculateBuildingGroupPivot(Group);
	ServerBuildingInitialYaw = 0.0f;
	float InitialLandscapeHeight = ServerBuildingOriginalPivot.Z;
	ServerBuildingGroundOffset =
		FindLandscapeHeight(
			FVector2D(
				ServerBuildingOriginalPivot.X,
				ServerBuildingOriginalPivot.Y),
			InitialLandscapeHeight)
			? ServerBuildingOriginalPivot.Z - InitialLandscapeHeight
			: 0.0f;

	ClientBeginBuildingGroupMove(
		Group,
		ServerBuildingOriginalPivot,
		ServerBuildingInitialYaw);
}

void ABotanicusPlayerController::ServerUpdateBuildingGroupMove_Implementation(
	FVector_NetQuantize10 NewPivotLocation,
	float NewYaw)
{
	if (ServerBuildingGroup.Num() == 0 ||
		NewPivotLocation.ContainsNaN() ||
		!FMath::IsFinite(NewYaw))
	{
		return;
	}

	if (const APawn* ControlledPawn = GetPawn())
	{
		const FVector PawnLocation = ControlledPawn->GetActorLocation();
		if (FVector::DistSquared2D(PawnLocation, NewPivotLocation) >
			FMath::Square(MaximumBuildingEditDistance))
		{
			return;
		}
	}

	FVector GroundedPivot = NewPivotLocation;
	float LandscapeHeight = 0.0f;
	if (FindLandscapeHeight(
		FVector2D(GroundedPivot.X, GroundedPivot.Y),
		LandscapeHeight))
	{
		GroundedPivot.Z = LandscapeHeight + ServerBuildingGroundOffset;
	}

	ApplyServerBuildingGroupTransform(GroundedPivot, NewYaw);
}

void ABotanicusPlayerController::ServerConfirmBuildingGroupMove_Implementation()
{
	for (AActor* Actor : ServerBuildingGroup)
	{
		if (IsValid(Actor))
		{
			Actor->ForceNetUpdate();
		}
	}

	ClearServerBuildingGroupMove();
	ClientEndBuildingGroupMove(true);
}

void ABotanicusPlayerController::ServerCancelBuildingGroupMove_Implementation()
{
	for (int32 Index = 0;
		 Index < ServerBuildingGroup.Num() &&
		 Index < ServerBuildingOriginalTransforms.Num();
		 ++Index)
	{
		AActor* Actor = ServerBuildingGroup[Index];
		if (IsValid(Actor))
		{
			Actor->SetActorTransform(
				ServerBuildingOriginalTransforms[Index],
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Actor->ForceNetUpdate();
		}
	}

	ClearServerBuildingGroupMove();
	ClientEndBuildingGroupMove(false);
}

void ABotanicusPlayerController::ClientBeginBuildingGroupMove_Implementation(
	const TArray<AActor*>& GroupActors,
	FVector_NetQuantize10 GroupPivot,
	float InitialYaw)
{
	SetBuildingGroupHighlighted(false);
	LocalBuildingGroup.Reset(GroupActors.Num());
	LocalBuildingOriginalTransforms.Reset(GroupActors.Num());
	for (AActor* Actor : GroupActors)
	{
		if (IsValid(Actor))
		{
			LocalBuildingGroup.Add(Actor);
			LocalBuildingOriginalTransforms.Add(Actor->GetActorTransform());
		}
	}

	LocalBuildingOriginalPivot = GroupPivot;
	LocalBuildingPivot = GroupPivot;
	LocalBuildingYaw = InitialYaw;
	float InitialLandscapeHeight = GroupPivot.Z;
	LocalBuildingGroundOffset =
		FindLandscapeHeight(
			FVector2D(GroupPivot.X, GroupPivot.Y),
			InitialLandscapeHeight)
			? GroupPivot.Z - InitialLandscapeHeight
			: 0.0f;
	BuildingPreviewUpdateAccumulator = 0.0f;
	SetBuildingGroupHighlighted(true);

	ClientMessage(
		TEXT("Bâtiment sélectionné : souris pour déplacer, molette pour tourner, clic/E pour confirmer, clic droit/Échap pour annuler."));
}

void ABotanicusPlayerController::ClientEndBuildingGroupMove_Implementation(
	bool bConfirmed)
{
	SetBuildingGroupHighlighted(false);
	LocalBuildingGroup.Reset();
	LocalBuildingOriginalTransforms.Reset();
	LocalBuildingPivot = FVector::ZeroVector;
	LocalBuildingOriginalPivot = FVector::ZeroVector;
	LocalBuildingYaw = 0.0f;
	LocalBuildingGroundOffset = 0.0f;
	BuildingPreviewUpdateAccumulator = 0.0f;

	ClientMessage(
		bConfirmed
			? TEXT("Déplacement du bâtiment confirmé.")
			: TEXT("Déplacement du bâtiment annulé."));
}

TArray<AActor*> ABotanicusPlayerController::BuildCompleteBuildingGroup(
	AActor* HitActor) const
{
	TArray<AActor*> AllBuildingActors;
	TArray<AActor*> StructuralActors;
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!IsValid(Actor) || !IsEbsBuildingActor(Actor))
		{
			continue;
		}

		AllBuildingActors.Add(Actor);
		if (IsStructuralBuildingActor(Actor))
		{
			StructuralActors.Add(Actor);
		}
	}

	AActor* StructuralSeed =
		IsStructuralBuildingActor(HitActor) ? HitActor : nullptr;
	if (!StructuralSeed && IsEbsBuildingActor(HitActor))
	{
		float BestDistanceSquared = FMath::Square(1200.0f);
		for (AActor* Candidate : StructuralActors)
		{
			const float DistanceSquared = FVector::DistSquared(
				HitActor->GetActorLocation(),
				Candidate->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				StructuralSeed = Candidate;
			}
		}
	}

	if (!StructuralSeed)
	{
		return {};
	}

	TArray<AActor*> Result;
	TSet<AActor*> AddedStructures;
	TArray<AActor*> PendingStructures;
	AddedStructures.Add(StructuralSeed);
	PendingStructures.Add(StructuralSeed);

	while (PendingStructures.Num() > 0)
	{
		AActor* Current = PendingStructures.Pop(EAllowShrinking::No);
		Result.Add(Current);

		FVector CurrentOrigin;
		FVector CurrentExtent;
		Current->GetActorBounds(true, CurrentOrigin, CurrentExtent);
		const FBox CurrentBounds(
			CurrentOrigin - CurrentExtent - FVector(100.0f),
			CurrentOrigin + CurrentExtent + FVector(100.0f));

		for (AActor* Candidate : StructuralActors)
		{
			if (AddedStructures.Contains(Candidate))
			{
				continue;
			}

			FVector CandidateOrigin;
			FVector CandidateExtent;
			Candidate->GetActorBounds(
				true,
				CandidateOrigin,
				CandidateExtent);
			const FBox CandidateBounds(
				CandidateOrigin - CandidateExtent,
				CandidateOrigin + CandidateExtent);
			if (CurrentBounds.Intersect(CandidateBounds))
			{
				AddedStructures.Add(Candidate);
				PendingStructures.Add(Candidate);
			}
		}
	}

	FBox BuildingBounds(EForceInit::ForceInit);
	for (AActor* Actor : Result)
	{
		FVector Origin;
		FVector Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		BuildingBounds += FBox(Origin - Extent, Origin + Extent);
	}

	const FBox ContentBounds(
		BuildingBounds.Min - FVector(150.0f, 150.0f, 100.0f),
		BuildingBounds.Max + FVector(150.0f, 150.0f, 500.0f));
	for (AActor* Actor : AllBuildingActors)
	{
		if (!AddedStructures.Contains(Actor) &&
			ContentBounds.IsInsideOrOn(Actor->GetActorLocation()))
		{
			Result.AddUnique(Actor);
		}
	}

	for (int32 Index = 0; Index < Result.Num(); ++Index)
	{
		TArray<AActor*> AttachedActors;
		Result[Index]->GetAttachedActors(
			AttachedActors,
			true,
			true);
		for (AActor* Attached : AttachedActors)
		{
			if (IsValid(Attached) && IsEbsBuildingActor(Attached))
			{
				Result.AddUnique(Attached);
			}
		}
	}

	return Result;
}

bool ABotanicusPlayerController::IsEbsBuildingActor(
	const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	return Actor->GetClass()->GetPathName().Contains(
		TEXT("/Game/EasyBuildingSystem/Blueprints/BuildingObjects/"));
}

bool ABotanicusPlayerController::IsStructuralBuildingActor(
	const AActor* Actor) const
{
	if (!IsEbsBuildingActor(Actor))
	{
		return false;
	}

	const FString ClassName = Actor->GetClass()->GetName();
	static const TCHAR* StructuralTokens[] = {
		TEXT("Foundation"),
		TEXT("Wall"),
		TEXT("Ceiling"),
		TEXT("Roof"),
		TEXT("Ramp"),
		TEXT("Stairs"),
		TEXT("Fence"),
		TEXT("DoorFrame"),
		TEXT("WindowFrame"),
	};

	for (const TCHAR* Token : StructuralTokens)
	{
		if (ClassName.Contains(Token, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool ABotanicusPlayerController::IsBuildingOwnedByThisPlayer(
	AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	UFunction* OwnershipFunction =
		Actor->FindFunction(TEXT("CheckPlayerIsOwner_BPI"));
	if (!OwnershipFunction)
	{
		// Some demonstration assets do not enable EBS ownership. They remain
		// editable so the feature can be tested on the reference map.
		return true;
	}

	FStructOnScope Parameters(OwnershipFunction);
	void* ParameterMemory = Parameters.GetStructMemory();
	bool* OwnershipResult = nullptr;

	for (TFieldIterator<FProperty> PropertyIt(OwnershipFunction);
		 PropertyIt;
		 ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (!Property->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			if (Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				OwnershipResult =
					BoolProperty->ContainerPtrToValuePtr<bool>(
						ParameterMemory);
			}
			continue;
		}

		if (Property->HasAnyPropertyFlags(CPF_OutParm))
		{
			continue;
		}

		if (FObjectPropertyBase* ObjectProperty =
			CastField<FObjectPropertyBase>(Property))
		{
			UObject* Value = nullptr;
			if (ObjectProperty->PropertyClass->IsChildOf(
				APlayerController::StaticClass()))
			{
				Value = const_cast<ABotanicusPlayerController*>(this);
			}
			else if (ObjectProperty->PropertyClass->IsChildOf(
				APawn::StaticClass()))
			{
				Value = GetPawn();
			}
			else if (ObjectProperty->PropertyClass->IsChildOf(
				APlayerState::StaticClass()))
			{
				Value = PlayerState;
			}

			ObjectProperty->SetObjectPropertyValue_InContainer(
				ParameterMemory,
				Value);
		}
		else if (FStrProperty* StringProperty =
			CastField<FStrProperty>(Property))
		{
			const FString PlayerName =
				PlayerState ? PlayerState->GetPlayerName() : FString();
			StringProperty->SetPropertyValue_InContainer(
				ParameterMemory,
				PlayerName);
		}
		else if (FNameProperty* NameProperty =
			CastField<FNameProperty>(Property))
		{
			const FName PlayerName =
				PlayerState
					? FName(*PlayerState->GetPlayerName())
					: NAME_None;
			NameProperty->SetPropertyValue_InContainer(
				ParameterMemory,
				PlayerName);
		}
	}

	Actor->ProcessEvent(OwnershipFunction, ParameterMemory);
	return !OwnershipResult || *OwnershipResult;
}

FVector ABotanicusPlayerController::CalculateBuildingGroupPivot(
	const TArray<AActor*>& GroupActors) const
{
	FBox Bounds(EForceInit::ForceInit);
	for (AActor* Actor : GroupActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		FVector Origin;
		FVector Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		Bounds += FBox(Origin - Extent, Origin + Extent);
	}

	if (!Bounds.IsValid)
	{
		return FVector::ZeroVector;
	}

	const FVector Center = Bounds.GetCenter();
	return FVector(Center.X, Center.Y, Bounds.Min.Z);
}

void ABotanicusPlayerController::ApplyServerBuildingGroupTransform(
	const FVector& NewPivot,
	float NewYaw)
{
	const FQuat DeltaRotation =
		FRotator(0.0f, NewYaw - ServerBuildingInitialYaw, 0.0f)
			.Quaternion();
	for (int32 Index = 0;
		 Index < ServerBuildingGroup.Num() &&
		 Index < ServerBuildingOriginalTransforms.Num();
		 ++Index)
	{
		AActor* Actor = ServerBuildingGroup[Index];
		if (!IsValid(Actor))
		{
			continue;
		}

		const FTransform& Original = ServerBuildingOriginalTransforms[Index];
		const FVector RelativeLocation =
			Original.GetLocation() - ServerBuildingOriginalPivot;
		FTransform NewTransform = Original;
		NewTransform.SetLocation(
			NewPivot + DeltaRotation.RotateVector(RelativeLocation));
		NewTransform.SetRotation(
			DeltaRotation * Original.GetRotation());
		Actor->SetActorTransform(
			NewTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Actor->ForceNetUpdate();
	}
}

void ABotanicusPlayerController::ClearServerBuildingGroupMove()
{
	ServerBuildingGroup.Reset();
	ServerBuildingOriginalTransforms.Reset();
	ServerBuildingOriginalPivot = FVector::ZeroVector;
	ServerBuildingInitialYaw = 0.0f;
	ServerBuildingGroundOffset = 0.0f;
}

bool ABotanicusPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

UBotanicusMultiplayerSubsystem*
ABotanicusPlayerController::GetBotanicusMultiplayerSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UBotanicusMultiplayerSubsystem>();
	}

	UE_LOG(LogBotanicus, Error, TEXT("Botanicus multiplayer subsystem is unavailable."));
	return nullptr;
}
