// Copyright Epic Games, Inc. All Rights Reserved.


#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "BotanicusCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Botanicus.h"
#include "BotanicusGameMode.h"
#include "Building/BotanicusCommunicationDoorActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusDeliveryZoneActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "DrawDebugHelpers.h"
#include "Online/BotanicusMultiplayerSubsystem.h"
#include "Path/BotanicusPathActor.h"
#include "Ping/BotanicusPingMarker.h"
#include "UI/BotanicusQuickBarWidget.h"
#include "UI/BotanicusTopDownToolbarWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	const FName PurchasedBuildingTag(TEXT("BotanicusPurchasedBuilding"));

	void ConfigurePurchasedActorForNetworking(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		// Marketplace building actors were not authored as runtime network
		// spawns. Override their default ownership/relevancy so every client
		// receives both the spawn and subsequent placement transforms.
		Actor->SetOwner(nullptr);
		Actor->bOnlyRelevantToOwner = false;
		Actor->bAlwaysRelevant = true;
		Actor->SetNetUpdateFrequency(30.0f);
		Actor->SetMinNetUpdateFrequency(10.0f);
		Actor->SetReplicates(true);
		Actor->SetReplicateMovement(true);
		Actor->SetNetDormancy(DORM_Awake);
		Actor->FlushNetDormancy();
		Actor->ForceNetUpdate();
	}

	TMap<
		TWeakObjectPtr<AActor>,
		TWeakObjectPtr<ABotanicusPlayerController>>
		ActiveBuildingEditLocks;
}

ABotanicusPlayerController::ABotanicusPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABotanicusCameraManager::StaticClass();
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		ValidPlacementMaterialFinder(
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_Can_Build.MI_Can_Build"));
	if (ValidPlacementMaterialFinder.Succeeded())
	{
		ValidBuildingPlacementMaterial =
			ValidPlacementMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		InvalidPlacementMaterialFinder(
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_CanNot_Build.MI_CanNot_Build"));
	if (InvalidPlacementMaterialFinder.Succeeded())
	{
		InvalidBuildingPlacementMaterial =
			InvalidPlacementMaterialFinder.Object;
	}
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
	UActorComponent* BuildingComponent = nullptr;
	const bool bBuildModeActive =
		IsEbsConstructionModeActive(BuildingComponent);
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("EBS left-click binding test: component=%s GetBuildingMode=%s TryBuild=%s activeBuildMode=%s"),
		BuildingComponent
			? *BuildingComponent->GetClass()->GetName()
			: TEXT("<missing>"),
		BuildingComponent &&
			BuildingComponent->FindFunction(TEXT("GetBuildingMode"))
				? TEXT("found")
				: TEXT("missing"),
		BuildingComponent &&
			BuildingComponent->FindFunction(TEXT("TryBuild"))
				? TEXT("found")
				: TEXT("missing"),
		bBuildModeActive ? TEXT("true") : TEXT("false"));

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!IsStructuralBuildingActor(Actor))
		{
			continue;
		}

		const TArray<AActor*> Group = BuildCompleteBuildingGroup(Actor);
		int32 StructuralCount = 0;
		int32 SaveableCount = 0;
		for (AActor* BuildingActor : Group)
		{
			StructuralCount += IsStructuralBuildingActor(BuildingActor) ? 1 : 0;
			SaveableCount +=
				BuildingActor &&
				BuildingActor->FindFunction(
					TEXT("SaveData_BPI"))
					? 1
					: 0;
		}

		FString SaveFunctions;
		for (TFieldIterator<UFunction> FunctionIt(
			 Actor->GetClass(),
			 EFieldIterationFlags::IncludeSuper);
			 FunctionIt;
			 ++FunctionIt)
		{
			const FString FunctionName = FunctionIt->GetName();
			if (FunctionName.Contains(
				TEXT("Save"),
				ESearchCase::IgnoreCase))
			{
				if (!SaveFunctions.IsEmpty())
				{
					SaveFunctions += TEXT(",");
				}
				SaveFunctions += FunctionName;
			}
		}

		const FVector Pivot = CalculateBuildingGroupPivot(Group);
		float LandscapeHeight = 0.0f;
		const bool bLandscapeFound = FindLandscapeHeight(
			FVector2D(Pivot.X, Pivot.Y),
			LandscapeHeight);

		ServerBuildingGroup.Reset(Group.Num());
		for (AActor* BuildingActor : Group)
		{
			ServerBuildingGroup.Add(BuildingActor);
		}
		const bool bPlacementValid =
			bLandscapeFound && IsServerBuildingGroupPlacementValid();
		ServerBuildingGroup.Reset();

		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Whole-building grouping test: seed=%s total=%d structural=%d contents=%d saveable=%d landscape=%s groundOffset=%.1f placement=%s"),
			*Actor->GetName(),
			Group.Num(),
			StructuralCount,
			Group.Num() - StructuralCount,
			SaveableCount,
			bLandscapeFound ? TEXT("found") : TEXT("missing"),
			bLandscapeFound ? Pivot.Z - LandscapeHeight : 0.0f,
			bPlacementValid ? TEXT("valid") : TEXT("blocked"));
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("EBS save functions on %s: %s"),
			*Actor->GetClass()->GetName(),
			SaveFunctions.IsEmpty() ? TEXT("<none>") : *SaveFunctions);
		return;
	}

	UE_LOG(
		LogBotanicus,
		Warning,
		TEXT("Whole-building grouping test found no EBS structural actor."));
}

void ABotanicusPlayerController::BotanicusTestPing()
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(
			LogBotanicus,
			Warning,
			TEXT("Ping test has no controlled pawn."));
		return;
	}

	ServerPlacePing(
		ControlledPawn->GetActorLocation() +
		ControlledPawn->GetActorForwardVector() * 300.0f);
}

void ABotanicusPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ForceFirstPersonView();
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::ForceFirstPersonView);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeQuickBarWidget);
	
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
	CancelLargeEquipmentPlacement();
	CancelPathPlacement();
	CancelPathDeletion();
	if (IsValid(CommunicationDoorPreviewActor))
	{
		CommunicationDoorPreviewActor->Destroy();
		CommunicationDoorPreviewActor = nullptr;
	}

	if (TopDownToolbarWidget)
	{
		TopDownToolbarWidget->RemoveFromParent();
		TopDownToolbarWidget = nullptr;
	}

	if (QuickBarWidget)
	{
		QuickBarWidget->RemoveFromParent();
		QuickBarWidget = nullptr;
	}

	SetBuildingGroupHighlighted(false);
	RestoreTopDownRoofVisibility();
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

	if (IsLocalPlayerController() && !QuickBarWidget)
	{
		InitializeQuickBarWidget();
	}

	if (IsLocalPlayerController())
	{
		HideEbsDemoHud();
		if (bBuildingTopDownViewActive)
		{
			TopDownRoofRefreshAccumulator += DeltaTime;
			if (TopDownRoofRefreshAccumulator >= 0.25f)
			{
				TopDownRoofRefreshAccumulator = 0.0f;
				RefreshTopDownRoofVisibility();
			}
		}

		const float AzertyForward =
			(bAzertyForwardPressed ? 1.0f : 0.0f) -
			(bAzertyBackwardPressed ? 1.0f : 0.0f);
		const float AzertyRight =
			(bAzertyRightPressed ? 1.0f : 0.0f) -
			(bAzertyLeftPressed ? 1.0f : 0.0f);
		if (bBuildingTopDownViewActive)
		{
			MoveBuildingCameraForward(AzertyForward);
			MoveBuildingCameraRight(AzertyRight);
		}
		else if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->AddMovementInput(
				ControlledPawn->GetActorForwardVector(),
				AzertyForward);
			ControlledPawn->AddMovementInput(
				ControlledPawn->GetActorRightVector(),
				AzertyRight);
		}
	}

	UpdateBuildingGroupPreview(DeltaTime);
	UpdateCommunicationDoorPreview();
	UpdatePathPreview();
	UpdateLargeEquipmentPlacement(DeltaTime);
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
	if (Params.Key == EKeys::Z ||
		Params.Key == EKeys::S ||
		Params.Key == EKeys::Q ||
		Params.Key == EKeys::D)
	{
		const bool bPressed =
			Params.Event == IE_Pressed ||
			Params.Event == IE_Repeat;
		const bool bReleased = Params.Event == IE_Released;
		if (bPressed || bReleased)
		{
			const bool bNewState = bPressed;
			if (Params.Key == EKeys::Z)
			{
				bAzertyForwardPressed = bNewState;
			}
			else if (Params.Key == EKeys::S)
			{
				bAzertyBackwardPressed = bNewState;
			}
			else if (Params.Key == EKeys::Q)
			{
				bAzertyLeftPressed = bNewState;
			}
			else
			{
				bAzertyRightPressed = bNewState;
			}
		}

		// Consume these physical keys before EBS or the QWERTY Enhanced Input
		// context can reinterpret Q/Z as gameplay shortcuts.
		return true;
	}

	// EBS binds V directly and cycles first person -> top down -> third person.
	// Botanicus never exposes that three-state cycle.
	if (Params.Key == EKeys::V)
	{
		return true;
	}

	// Botanicus uses complete purchased buildings. EBS' modular survival
	// construction, snapping, grid, debug and demo save/load shortcuts are
	// deliberately unavailable to players.
	if (Params.Key == EKeys::C ||
		Params.Key == EKeys::G ||
		Params.Key == EKeys::Tab ||
		Params.Key == EKeys::PageUp ||
		Params.Key == EKeys::PageDown ||
		Params.Key == EKeys::NumPadOne ||
		Params.Key == EKeys::NumPadThree)
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
			if (IsValid(LocalLargeEquipmentPlacement))
			{
				CancelLargeEquipmentPlacement();
			}
			if (bBuildingTopDownViewActive && LocalBuildingGroup.Num() > 0)
			{
				CancelBuildingGroupMove();
			}
			ToggleBuildingTopDownView();
		}

		return true;
	}

	if (Params.Key == EKeys::E &&
		Params.Event == IE_Pressed &&
		!bBuildingTopDownViewActive &&
		(TryHandleNearbyLargeEquipment() ||
		 TryCollectNearbyDeliveryParcel()))
	{
		return true;
	}

	if (IsValid(LocalLargeEquipmentPlacement) &&
		!bBuildingTopDownViewActive)
	{
		if (Params.Event == IE_Pressed &&
			(Params.Key == EKeys::MouseScrollUp ||
			 Params.Key == EKeys::MouseScrollDown))
		{
			RotateLargeEquipmentPlacement(
				Params.Key == EKeys::MouseScrollUp ? 1.0f : -1.0f);
			return true;
		}

		if (Params.Key == EKeys::LeftMouseButton &&
			Params.Event == IE_Pressed)
		{
			ConfirmLargeEquipmentPlacement();
			return true;
		}

		if ((Params.Key == EKeys::RightMouseButton ||
			 Params.Key == EKeys::Escape) &&
			Params.Event == IE_Pressed)
		{
			CancelLargeEquipmentPlacement();
			return true;
		}
	}

	if (Params.Key == EKeys::MiddleMouseButton &&
		Params.Event == IE_Pressed)
	{
		TryPlacePing();
		return true;
	}

	if (bBuildingTopDownViewActive)
	{
		if (bCommunicationDoorPlacementActive)
		{
			if (Params.Key == EKeys::LeftMouseButton &&
				Params.Event == IE_Pressed)
			{
				if (!IsCursorOverTopDownToolbar() &&
					LocalCommunicationDoorCandidateIndex != INDEX_NONE)
				{
					ServerConfirmCommunicationDoor(
						LocalCommunicationDoorCandidateIndex);
				}
				return true;
			}

			if ((Params.Key == EKeys::RightMouseButton ||
				 Params.Key == EKeys::Escape) &&
				Params.Event == IE_Pressed)
			{
				ServerCancelCommunicationDoor();
				return true;
			}
		}

		if (bPathPlacementActive)
		{
			if (Params.Key == EKeys::LeftMouseButton &&
				Params.Event == IE_Pressed)
			{
				if (!IsCursorOverTopDownToolbar())
				{
					AddPathPointAtCursor();
				}
				return true;
			}

			if (Params.Key == EKeys::RightMouseButton &&
				Params.Event == IE_Pressed)
			{
				RemoveLastPathPoint();
				return true;
			}

			if (Params.Key == EKeys::Escape &&
				Params.Event == IE_Pressed)
			{
				CancelPathPlacement();
				return true;
			}
		}

		if (bPathDeletionActive)
		{
			if (Params.Key == EKeys::LeftMouseButton &&
				Params.Event == IE_Pressed)
			{
				if (!IsCursorOverTopDownToolbar())
				{
					TryDeletePathSegmentAtCursor();
				}
				return true;
			}

			if ((Params.Key == EKeys::RightMouseButton ||
				 Params.Key == EKeys::Escape) &&
				Params.Event == IE_Pressed)
			{
				CancelPathDeletion();
				return true;
			}
		}

		if (Params.Event == IE_Pressed &&
			(Params.Key == EKeys::MouseScrollUp ||
			 Params.Key == EKeys::MouseScrollDown))
		{
			const float Direction =
				Params.Key == EKeys::MouseScrollUp ? 1.0f : -1.0f;

			// While a building is selected, the wheel keeps its existing
			// rotation role. Holding Shift temporarily restores camera zoom.
			const bool bZoomRequested =
				LocalBuildingGroup.Num() == 0 ||
				IsInputKeyDown(EKeys::LeftShift) ||
				IsInputKeyDown(EKeys::RightShift);
			if (!bZoomRequested)
			{
				RotateBuildingGroup(Direction);
			}
			else
			{
				ZoomBuildingCamera(Direction);
			}
			return true;
		}

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

	}

	// Temporary multiplayer inventory test: a left click consumes one seed
	// only when the test packet is selected.
	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Pressed)
		{
			if (ABotanicusCharacter* BotanicusCharacter =
				Cast<ABotanicusCharacter>(GetPawn()))
			{
				if (UBotanicusQuickBarComponent* QuickBar =
					BotanicusCharacter->GetQuickBarComponent())
				{
					const FBotanicusQuickBarSlot SelectedSlot =
						QuickBar->GetSelectedSlot();
					if (!SelectedSlot.IsEmpty() &&
						SelectedSlot.ItemKey == TEXT("SeedPacket_Test"))
					{
						QuickBar->RequestConsumeSelectedItem(1);
					}
				}
			}
		}

		// Never forward left click to EBS' damage/destruction trace.
		return true;
	}

	// The EBS mallet interaction is not part of Botanicus.
	if (Params.Key == EKeys::RightMouseButton)
	{
		return true;
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

bool ABotanicusPlayerController::IsQuickBarInputBlocked() const
{
	return bBuildingTopDownViewActive;
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
	TopDownRoofRefreshAccumulator = 0.0f;
	RefreshTopDownRoofVisibility();
	InitializeTopDownToolbarWidget();
	if (TopDownToolbarWidget)
	{
		TopDownToolbarWidget->SetVisibility(ESlateVisibility::Visible);
	}

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

	CancelPathPlacement();
	CancelPathDeletion();
	if (bCommunicationDoorPlacementActive)
	{
		ServerCancelCommunicationDoor();
	}
	if (TopDownToolbarWidget)
	{
		TopDownToolbarWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}

	// EBS cycles top down -> third person -> first person. Advancing twice
	// returns its internal state directly to first person.
	AdvanceEbsViewMode();
	AdvanceEbsViewMode();
	bBuildingTopDownViewActive = false;
	RestoreTopDownRoofVisibility();

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

void ABotanicusPlayerController::InitializeQuickBarWidget()
{
	if (!IsLocalPlayerController() || QuickBarWidget)
	{
		return;
	}

	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!BotanicusCharacter || !BotanicusCharacter->GetQuickBarComponent())
	{
		return;
	}

	QuickBarWidget =
		CreateWidget<UBotanicusQuickBarWidget>(
			this,
			UBotanicusQuickBarWidget::StaticClass());
	if (!QuickBarWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the prototype inventory hotbar."));
		return;
	}

	QuickBarWidget->InitializeWithQuickBar(
		BotanicusCharacter->GetQuickBarComponent());
	QuickBarWidget->AddToPlayerScreen(10);
}

void ABotanicusPlayerController::InitializeTopDownToolbarWidget()
{
	if (!IsLocalPlayerController() || TopDownToolbarWidget)
	{
		return;
	}

	TopDownToolbarWidget =
		CreateWidget<UBotanicusTopDownToolbarWidget>(
			this,
			UBotanicusTopDownToolbarWidget::StaticClass());
	if (!TopDownToolbarWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the top-down planning toolbar."));
		return;
	}

	TopDownToolbarWidget->InitializeWithController(this);
	TopDownToolbarWidget->AddToPlayerScreen(20);
}

void ABotanicusPlayerController::HideEbsDemoHud()
{
	if (bEbsDemoHudHidden)
	{
		return;
	}

	for (TObjectIterator<UUserWidget> WidgetIt; WidgetIt; ++WidgetIt)
	{
		UUserWidget* Widget = *WidgetIt;
		if (!IsValid(Widget) ||
			Widget->IsTemplate() ||
			Widget->GetWorld() != GetWorld() ||
			Widget->GetOwningPlayer() != this ||
			!Widget->GetClass()->GetPathName().Contains(
				TEXT("/UI_EBS_HUD")))
		{
			continue;
		}

		Widget->SetVisibility(ESlateVisibility::Collapsed);
		bEbsDemoHudHidden = true;
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("EBS demonstration HUD hidden; Botanicus UI is authoritative."));
		return;
	}
}

void ABotanicusPlayerController::RefreshTopDownRoofVisibility()
{
	if (!IsLocalPlayerController() || !bBuildingTopDownViewActive)
	{
		return;
	}

	for (auto ComponentIt = TopDownHiddenRoofComponents.CreateIterator();
		 ComponentIt;
		 ++ComponentIt)
	{
		if (!ComponentIt->IsValid())
		{
			ComponentIt.RemoveCurrent();
		}
	}

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!IsEbsBuildingActor(Actor) ||
			!Actor->GetClass()->GetName().Contains(
				TEXT("Roof"),
				ESearchCase::IgnoreCase))
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> RoofComponents(Actor);
		for (UPrimitiveComponent* RoofComponent : RoofComponents)
		{
			if (!IsValid(RoofComponent) ||
				TopDownHiddenRoofComponents.Contains(RoofComponent) ||
				!RoofComponent->IsVisible())
			{
				continue;
			}

			// Component visibility is local and is not part of the saved or
			// replicated actor state. First-person players therefore keep
			// their roofs while this client gets an open-building view.
			RoofComponent->SetVisibility(false, false);
			TopDownHiddenRoofComponents.Add(RoofComponent);
		}
	}
}

void ABotanicusPlayerController::RestoreTopDownRoofVisibility()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Component :
		 TopDownHiddenRoofComponents)
	{
		if (Component.IsValid())
		{
			Component->SetVisibility(true, false);
		}
	}
	TopDownHiddenRoofComponents.Reset();
	TopDownRoofRefreshAccumulator = 0.0f;
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

void ABotanicusPlayerController::ZoomBuildingCamera(float Direction)
{
	if (!bBuildingTopDownViewActive ||
		!IsValid(BuildingCameraActor) ||
		FMath::IsNearlyZero(Direction))
	{
		return;
	}

	FVector CameraLocation = BuildingCameraActor->GetActorLocation();
	float GroundHeight = 0.0f;
	if (!FindLandscapeHeight(
		FVector2D(CameraLocation.X, CameraLocation.Y),
		GroundHeight))
	{
		if (const APawn* ControlledPawn = GetPawn())
		{
			GroundHeight = ControlledPawn->GetActorLocation().Z;
		}
	}

	const float MinimumHeight =
		FMath::Min(MinimumBuildingCameraHeight, MaximumBuildingCameraHeight);
	const float MaximumHeight =
		FMath::Max(MinimumBuildingCameraHeight, MaximumBuildingCameraHeight);
	const float CurrentHeight = CameraLocation.Z - GroundHeight;
	const float NewHeight = FMath::Clamp(
		CurrentHeight - Direction * BuildingCameraZoomStep,
		MinimumHeight,
		MaximumHeight);

	CameraLocation.Z = GroundHeight + NewHeight;
	BuildingCameraActor->SetActorLocation(CameraLocation);
}

void ABotanicusPlayerController::BeginPathPlacement()
{
	if (!bBuildingTopDownViewActive || !IsLocalPlayerController())
	{
		return;
	}

	CancelPathDeletion();
	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}

	if (bPathPlacementActive)
	{
		PendingPathPoints.Reset();
	}
	bPathPlacementActive = true;

	if (!IsValid(PathPreviewActor))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.ObjectFlags |= RF_Transient;
		PathPreviewActor = GetWorld()->SpawnActor<ABotanicusPathActor>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (PathPreviewActor)
		{
			PathPreviewActor->SetReplicates(false);
		}
	}

	if (PathPreviewActor)
	{
		PathPreviewActor->SetPreviewPath(PendingPathPoints);
	}
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::ConfirmPathPlacement()
{
	if (!bPathPlacementActive || PendingPathPoints.Num() < 2)
	{
		return;
	}

	TArray<FVector_NetQuantize10> RequestedPoints;
	RequestedPoints.Reserve(PendingPathPoints.Num());
	for (const FVector& Point : PendingPathPoints)
	{
		RequestedPoints.Add(FVector_NetQuantize10(Point));
	}

	ServerCreatePath(RequestedPoints);
	CancelPathPlacement();
}

void ABotanicusPlayerController::CancelPathPlacement()
{
	bPathPlacementActive = false;
	PendingPathPoints.Reset();
	if (IsValid(PathPreviewActor))
	{
		PathPreviewActor->Destroy();
		PathPreviewActor = nullptr;
	}
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::BeginPathDeletion()
{
	if (!bBuildingTopDownViewActive || !IsLocalPlayerController())
	{
		return;
	}

	if (bPathDeletionActive)
	{
		CancelPathDeletion();
		return;
	}

	CancelPathPlacement();
	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}
	bPathDeletionActive = true;
	RefreshTopDownToolbar();
	ClientMessage(
		TEXT("Suppression de route : cliquez sur la portion a retirer."));
}

void ABotanicusPlayerController::CancelPathDeletion()
{
	if (!bPathDeletionActive)
	{
		return;
	}

	bPathDeletionActive = false;
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::PurchaseTestBuilding()
{
	if (!bBuildingTopDownViewActive || !IsLocalPlayerController())
	{
		return;
	}
	if (bCommunicationDoorPlacementActive)
	{
		ClientMessage(
			TEXT("Terminez d'abord le placement de la porte de communication."));
		return;
	}

	CancelPathPlacement();
	CancelPathDeletion();
	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}
	ServerPurchaseTestBuilding();
}

void ABotanicusPlayerController::OrderTestDelivery()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ServerOrderTestDelivery();
}

void ABotanicusPlayerController::OrderTestLargeEquipment()
{
	if (IsLocalPlayerController())
	{
		ServerOrderTestLargeEquipment();
	}
}

bool ABotanicusPlayerController::TryHandleNearbyLargeEquipment()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	ABotanicusLargeEquipmentActor* NearestEquipment = nullptr;
	float BestDistanceSquared = FMath::Square(450.0f);
	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(
			 GetWorld());
		 EquipmentIt;
		 ++EquipmentIt)
	{
		if (EquipmentIt->GetCarrier() == GetPawn())
		{
			if (!EquipmentIt->IsInPlacementMode())
			{
				ServerBeginLargeEquipmentPlacement(*EquipmentIt);
				BeginLargeEquipmentPlacement(*EquipmentIt);
			}
			return true;
		}

		if (IsValid(EquipmentIt->GetCarrier()))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			EquipmentIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(*EquipmentIt, 450.0f))
		{
			BestDistanceSquared = DistanceSquared;
			NearestEquipment = *EquipmentIt;
		}
	}

	if (!NearestEquipment)
	{
		return false;
	}

	ServerToggleCarryLargeEquipment(NearestEquipment);
	return true;
}

void ABotanicusPlayerController::BeginLargeEquipmentPlacement(
	ABotanicusLargeEquipmentActor* Equipment)
{
	if (!IsLocalPlayerController() ||
		!IsValid(Equipment))
	{
		return;
	}

	LocalLargeEquipmentPlacement = Equipment;
	LargeEquipmentPlacementYaw =
		GetPawn() ? GetPawn()->GetActorRotation().Yaw : 0.0f;
	LargeEquipmentPreviewUpdateAccumulator = 0.0f;
	bLocalLargeEquipmentPlacementValid = false;
	ClientMessage(
		TEXT(
			"Placement : molette 5 degres, Maj + molette 1 degre, clic gauche pour poser, clic droit pour annuler."));
}

void ABotanicusPlayerController::UpdateLargeEquipmentPlacement(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		bBuildingTopDownViewActive ||
		!IsValid(LocalLargeEquipmentPlacement))
	{
		return;
	}

	if (IsValid(LocalLargeEquipmentPlacement->GetCarrier()) &&
		LocalLargeEquipmentPlacement->GetCarrier() != GetPawn())
	{
		LocalLargeEquipmentPlacement = nullptr;
		bLocalLargeEquipmentPlacementValid = false;
		return;
	}

	LargeEquipmentPreviewUpdateAccumulator += DeltaTime;
	if (LargeEquipmentPreviewUpdateAccumulator < (1.0f / 30.0f))
	{
		return;
	}
	LargeEquipmentPreviewUpdateAccumulator = 0.0f;

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	FVector FlatForward = ViewRotation.Vector();
	FlatForward.Z = 0.0f;
	if (!FlatForward.Normalize())
	{
		FlatForward = GetPawn()->GetActorForwardVector();
		FlatForward.Z = 0.0f;
		FlatForward.Normalize();
	}

	const FVector RequestedLocation =
		GetPawn()->GetActorLocation() +
		FlatForward * EquipmentPlacementDistance;
	FTransform PlacementTransform;
	bLocalLargeEquipmentPlacementValid =
		ResolveLargeEquipmentPlacement(
			LocalLargeEquipmentPlacement,
			RequestedLocation,
			LargeEquipmentPlacementYaw,
			PlacementTransform);

	LocalLargeEquipmentPlacement->SetLocalPlacementPreview(
		PlacementTransform,
		bLocalLargeEquipmentPlacementValid);
	DrawLargeEquipmentAlignmentGuides(PlacementTransform);
	ServerUpdateLargeEquipmentPlacement(
		LocalLargeEquipmentPlacement,
		RequestedLocation,
		LargeEquipmentPlacementYaw);
}

void ABotanicusPlayerController::RotateLargeEquipmentPlacement(
	float Direction)
{
	if (IsValid(LocalLargeEquipmentPlacement))
	{
		const float RotationStep =
			IsInputKeyDown(EKeys::LeftShift) ||
			IsInputKeyDown(EKeys::RightShift)
				? FineEquipmentRotationStep
				: EquipmentRotationStep;
		LargeEquipmentPlacementYaw =
			FMath::UnwindDegrees(
				LargeEquipmentPlacementYaw +
				Direction * RotationStep);
		LargeEquipmentPreviewUpdateAccumulator = 1.0f;
	}
}

void ABotanicusPlayerController::DrawLargeEquipmentAlignmentGuides(
	const FTransform& PlacementTransform) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(LocalLargeEquipmentPlacement))
	{
		return;
	}

	const FVector PreviewLocation = PlacementTransform.GetLocation();
	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(World);
		 EquipmentIt;
		 ++EquipmentIt)
	{
		const ABotanicusLargeEquipmentActor* OtherEquipment = *EquipmentIt;
		if (!IsValid(OtherEquipment) ||
			OtherEquipment == LocalLargeEquipmentPlacement ||
			IsValid(OtherEquipment->GetCarrier()))
		{
			continue;
		}

		const FVector OtherLocation = OtherEquipment->GetActorLocation();
		const FVector Delta = PreviewLocation - OtherLocation;
		if (Delta.SizeSquared2D() >
			FMath::Square(EquipmentAlignmentGuideDistance))
		{
			continue;
		}

		const FRotator OtherYaw(
			0.0f,
			OtherEquipment->GetActorRotation().Yaw,
			0.0f);
		const FVector OtherForward = OtherYaw.Vector();
		const FVector OtherRight =
			FRotationMatrix(OtherYaw).GetUnitAxis(EAxis::Y);
		const float ForwardOffset =
			FVector::DotProduct(Delta, OtherForward);
		const float RightOffset =
			FVector::DotProduct(Delta, OtherRight);
		const float GuideZ =
			FMath::Min(PreviewLocation.Z, OtherLocation.Z) -
			LocalLargeEquipmentPlacement->GetPlacementBoxExtent().Z +
			4.0f;

		if (FMath::Abs(RightOffset) <=
			EquipmentAlignmentGuideTolerance)
		{
			const FVector GuideEnd =
				OtherLocation + OtherForward * ForwardOffset;
			DrawDebugLine(
				World,
				FVector(OtherLocation.X, OtherLocation.Y, GuideZ),
				FVector(GuideEnd.X, GuideEnd.Y, GuideZ),
				FColor::Green,
				false,
				0.08f,
				0,
				4.0f);
		}

		if (FMath::Abs(ForwardOffset) <=
			EquipmentAlignmentGuideTolerance)
		{
			const FVector GuideEnd =
				OtherLocation + OtherRight * RightOffset;
			DrawDebugLine(
				World,
				FVector(OtherLocation.X, OtherLocation.Y, GuideZ),
				FVector(GuideEnd.X, GuideEnd.Y, GuideZ),
				FColor::Green,
				false,
				0.08f,
				0,
				4.0f);
		}
	}
}

void ABotanicusPlayerController::ConfirmLargeEquipmentPlacement()
{
	if (!IsValid(LocalLargeEquipmentPlacement))
	{
		return;
	}

	if (!bLocalLargeEquipmentPlacementValid)
	{
		ClientMessage(
			TEXT("Position invalide : l'objet ne peut pas etre pose ici."));
		return;
	}

	ServerConfirmLargeEquipmentPlacement(
		LocalLargeEquipmentPlacement);
	LocalLargeEquipmentPlacement = nullptr;
	bLocalLargeEquipmentPlacementValid = false;
}

void ABotanicusPlayerController::CancelLargeEquipmentPlacement()
{
	if (!IsValid(LocalLargeEquipmentPlacement))
	{
		return;
	}

	ServerCancelLargeEquipmentPlacement(
		LocalLargeEquipmentPlacement);
	LocalLargeEquipmentPlacement = nullptr;
	bLocalLargeEquipmentPlacementValid = false;
}

bool ABotanicusPlayerController::ResolveLargeEquipmentPlacement(
	ABotanicusLargeEquipmentActor* Equipment,
	const FVector& RequestedLocation,
	float RequestedYaw,
	FTransform& OutTransform) const
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !IsValid(Equipment) || !ControlledPawn ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			RequestedLocation) >
			FMath::Square(MaximumEquipmentPlacementDistance))
	{
		OutTransform = FTransform(
			FRotator(0.0f, RequestedYaw, 0.0f),
			RequestedLocation);
		return false;
	}

	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusEquipmentPlacementFloor),
		false);
	FloorQuery.AddIgnoredActor(Equipment);
	FloorQuery.AddIgnoredActor(ControlledPawn);
	const float PawnBaseZ = ControlledPawn->GetActorLocation().Z;
	const FVector TraceStart(
		RequestedLocation.X,
		RequestedLocation.Y,
		PawnBaseZ + 140.0f);
	const FVector TraceEnd(
		RequestedLocation.X,
		RequestedLocation.Y,
		PawnBaseZ - 450.0f);
	FHitResult FloorHit;
	const bool bFoundFloor = World->LineTraceSingleByChannel(
		FloorHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		FloorQuery);

	const FVector BoxExtent =
		Equipment->GetPlacementBoxExtent().GetAbs();
	const FVector PlacementLocation(
		RequestedLocation.X,
		RequestedLocation.Y,
		bFoundFloor
			? FloorHit.ImpactPoint.Z + BoxExtent.Z + 3.0f
			: RequestedLocation.Z);
	const FQuat PlacementRotation =
		FRotator(0.0f, RequestedYaw, 0.0f).Quaternion();
	OutTransform = FTransform(
		PlacementRotation,
		PlacementLocation,
		Equipment->GetActorScale3D());

	if (!bFoundFloor || FloorHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams OverlapQuery(
		SCENE_QUERY_STAT(BotanicusEquipmentPlacementOverlap),
		false);
	OverlapQuery.AddIgnoredActor(Equipment);
	OverlapQuery.AddIgnoredActor(ControlledPawn);

	const FVector TestExtent(
		FMath::Max(5.0f, BoxExtent.X - 4.0f),
		FMath::Max(5.0f, BoxExtent.Y - 4.0f),
		FMath::Max(5.0f, BoxExtent.Z - 5.0f));
	TArray<FOverlapResult> Overlaps;
	return !World->OverlapMultiByObjectType(
		Overlaps,
		PlacementLocation,
		PlacementRotation,
		ObjectQuery,
		FCollisionShape::MakeBox(TestExtent),
		OverlapQuery);
}

bool ABotanicusPlayerController::TryCollectNearbyDeliveryParcel()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	ABotanicusDeliveryParcelActor* NearestParcel = nullptr;
	float BestDistanceSquared = FMath::Square(400.0f);
	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(
			 GetWorld());
		 ParcelIt;
		 ++ParcelIt)
	{
		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			ParcelIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(*ParcelIt, 400.0f))
		{
			BestDistanceSquared = DistanceSquared;
			NearestParcel = *ParcelIt;
		}
	}

	if (!NearestParcel)
	{
		return false;
	}

	ServerCollectDeliveryParcel(NearestParcel);
	return true;
}

bool ABotanicusPlayerController::IsLookingAtWorldItem(
	const AActor* Item,
	float MaximumDistance) const
{
	if (!IsValid(Item) || !GetWorld())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector TargetOrigin;
	FVector TargetExtent;
	Item->GetActorBounds(true, TargetOrigin, TargetExtent);
	const FVector ToTarget = TargetOrigin - ViewLocation;
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER ||
		Distance > MaximumDistance ||
		FVector::DotProduct(
			ViewRotation.Vector(),
			ToTarget / Distance) <
			FMath::Cos(FMath::DegreesToRadians(WorldItemLookAngle)))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusWorldItemLook),
		false);
	QueryParams.AddIgnoredActor(GetPawn());
	FHitResult VisibilityHit;
	return GetWorld()->LineTraceSingleByChannel(
			VisibilityHit,
			ViewLocation,
			TargetOrigin,
			ECC_Visibility,
			QueryParams) &&
		VisibilityHit.GetActor() == Item;
}

void ABotanicusPlayerController::TryDeletePathSegmentAtCursor()
{
	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit))
	{
		return;
	}

	ABotanicusPathActor* Path =
		Cast<ABotanicusPathActor>(CursorHit.GetActor());
	if (!IsValid(Path) || Path->IsPreviewPath())
	{
		ClientMessage(TEXT("Cliquez directement sur une portion de route."));
		return;
	}

	int32 SegmentIndex = INDEX_NONE;
	FVector ClosestPoint = FVector::ZeroVector;
	float Distance = 0.0f;
	if (!Path->FindClosestSegment(
			CursorHit.ImpactPoint,
			SegmentIndex,
			ClosestPoint,
			Distance))
	{
		return;
	}

	ServerDeletePathSegment(
		Path,
		SegmentIndex,
		FVector_NetQuantize10(ClosestPoint));
}

void ABotanicusPlayerController::AddPathPointAtCursor()
{
	FVector Point;
	if (!GetPathCursorPoint(Point))
	{
		return;
	}

	if (PendingPathPoints.Num() > 0 &&
		FVector::Dist2D(PendingPathPoints.Last(), Point) < 50.0f)
	{
		return;
	}

	PendingPathPoints.Add(Point);
	if (PathPreviewActor)
	{
		PathPreviewActor->SetPreviewPath(PendingPathPoints);
	}
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::RemoveLastPathPoint()
{
	if (PendingPathPoints.Num() == 0)
	{
		CancelPathPlacement();
		return;
	}

	PendingPathPoints.Pop();
	if (PathPreviewActor)
	{
		PathPreviewActor->SetPreviewPath(PendingPathPoints);
	}
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::UpdatePathPreview()
{
	if (!bPathPlacementActive ||
		!IsValid(PathPreviewActor) ||
		PendingPathPoints.Num() == 0)
	{
		return;
	}

	FVector CursorPoint;
	if (!GetPathCursorPoint(CursorPoint))
	{
		return;
	}

	TArray<FVector> PreviewPoints = PendingPathPoints;
	if (FVector::Dist2D(PreviewPoints.Last(), CursorPoint) >= 25.0f)
	{
		PreviewPoints.Add(CursorPoint);
	}
	PathPreviewActor->SetPreviewPath(PreviewPoints);
}

void ABotanicusPlayerController::RefreshTopDownToolbar()
{
	if (TopDownToolbarWidget)
	{
		TopDownToolbarWidget->RefreshPathState(
			bPathPlacementActive,
			PendingPathPoints.Num() >= 2,
			bPathDeletionActive);
	}
}

bool ABotanicusPlayerController::GetPathCursorPoint(
	FVector& OutPoint) const
{
	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit))
	{
		return false;
	}

	float LandscapeHeight = 0.0f;
	if (!FindLandscapeHeight(
		FVector2D(CursorHit.ImpactPoint.X, CursorHit.ImpactPoint.Y),
		LandscapeHeight))
	{
		return false;
	}

	const FVector RawPoint(
		CursorHit.ImpactPoint.X,
		CursorHit.ImpactPoint.Y,
		LandscapeHeight + 8.0f);
	ABotanicusPathActor* ConnectedPath = nullptr;
	if (!SnapPathPoint(RawPoint, OutPoint, ConnectedPath))
	{
		OutPoint = RawPoint;
	}
	return true;
}

bool ABotanicusPlayerController::SnapPathPoint(
	const FVector& RawPoint,
	FVector& OutSnappedPoint,
	ABotanicusPathActor*& OutConnectedPath) const
{
	OutConnectedPath = nullptr;
	if (FindNearestBuildingEntrance(RawPoint, OutSnappedPoint))
	{
		return true;
	}

	OutConnectedPath =
		FindNearestExistingPath(RawPoint, OutSnappedPoint);
	return OutConnectedPath != nullptr;
}

bool ABotanicusPlayerController::FindNearestBuildingEntrance(
	const FVector& RawPoint,
	FVector& OutEntrancePoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	float BestDistanceSquared =
		FMath::Square(BuildingEntranceSnapDistance);
	bool bFoundEntrance = false;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!IsValid(Actor))
		{
			continue;
		}

		const FString ClassPath = Actor->GetClass()->GetPathName();
		if (!ClassPath.Contains(TEXT("BP_EBS_Building_Door")))
		{
			continue;
		}

		const FVector ActorLocation = Actor->GetActorLocation();
		const float DistanceSquared =
			FVector::DistSquared2D(RawPoint, ActorLocation);
		if (DistanceSquared > BestDistanceSquared)
		{
			continue;
		}

		float LandscapeHeight = 0.0f;
		if (!FindLandscapeHeight(
			FVector2D(ActorLocation.X, ActorLocation.Y),
			LandscapeHeight))
		{
			continue;
		}

		BestDistanceSquared = DistanceSquared;
		OutEntrancePoint = FVector(
			ActorLocation.X,
			ActorLocation.Y,
			LandscapeHeight + 8.0f);
		bFoundEntrance = true;
	}

	return bFoundEntrance;
}

ABotanicusPathActor*
	ABotanicusPlayerController::FindNearestExistingPath(
		const FVector& RawPoint,
		FVector& OutPathPoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ABotanicusPathActor* BestPath = nullptr;
	float BestDistance = ExistingPathSnapDistance;
	for (TActorIterator<ABotanicusPathActor> PathIt(World);
		 PathIt;
		 ++PathIt)
	{
		ABotanicusPathActor* Path = *PathIt;
		if (!IsValid(Path) || Path->IsPreviewPath())
		{
			continue;
		}

		FVector ClosestPoint;
		float Distance = 0.0f;
		if (Path->FindClosestPoint(RawPoint, ClosestPoint, Distance) &&
			Distance <= BestDistance)
		{
			BestDistance = Distance;
			BestPath = Path;
			OutPathPoint = ClosestPoint;
		}
	}

	return BestPath;
}

bool ABotanicusPlayerController::IsCursorOverTopDownToolbar() const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	GetViewportSize(ViewportX, ViewportY);
	return MouseY <= 125.0f &&
		FMath::Abs(MouseX - ViewportX * 0.5f) <= 620.0f;
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
		if (!bLocalBuildingPlacementValid)
		{
			ClientMessage(
				TEXT("Placement impossible : déplacez le bâtiment vers une zone verte."));
			return;
		}

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

bool ABotanicusPlayerController::TryPlacePing()
{
	if (!IsLocalPlayerController())
	{
		return false;
	}

	FHitResult Hit;
	if (bBuildingTopDownViewActive)
	{
		if (!TraceTopDownCursor(Hit))
		{
			return false;
		}
	}
	else
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetPlayerViewPoint(ViewLocation, ViewRotation);

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(BotanicusPingTrace),
			true);
		QueryParams.AddIgnoredActor(GetPawn());

		if (!GetWorld() ||
			!GetWorld()->LineTraceSingleByChannel(
				Hit,
				ViewLocation,
				ViewLocation +
					ViewRotation.Vector() * MaximumPingDistance,
				ECC_Visibility,
				QueryParams))
		{
			return false;
		}
	}

	ServerPlacePing(Hit.ImpactPoint);
	return true;
}

bool ABotanicusPlayerController::IsEbsConstructionModeActive(
	UActorComponent*& OutBuildingComponent) const
{
	OutBuildingComponent = nullptr;

	TInlineComponentArray<UActorComponent*> Components(
		const_cast<ABotanicusPlayerController*>(this));
	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component) ||
			!Component->GetClass()->GetPathName().Contains(
				TEXT("/Game/EasyBuildingSystem/Blueprints/Components/BP_EBS_BuildingComponent")))
		{
			continue;
		}

		OutBuildingComponent = Component;
		UFunction* GetModeFunction =
			Component->FindFunction(TEXT("GetBuildingMode"));
		if (!GetModeFunction)
		{
			return false;
		}

		FStructOnScope Parameters(GetModeFunction);
		void* ParameterMemory = Parameters.GetStructMemory();
		Component->ProcessEvent(GetModeFunction, ParameterMemory);

		for (TFieldIterator<FProperty> PropertyIt(GetModeFunction);
			 PropertyIt;
			 ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(
				CPF_OutParm | CPF_ReturnParm))
			{
				continue;
			}

			if (FEnumProperty* EnumProperty =
				CastField<FEnumProperty>(Property))
			{
				const void* ValueAddress =
					EnumProperty->ContainerPtrToValuePtr<void>(
						ParameterMemory);
				const int64 Value =
					EnumProperty->GetUnderlyingProperty()
						->GetSignedIntPropertyValue(ValueAddress);
				const FString ModeName =
					EnumProperty->GetEnum()
						->GetDisplayNameTextByValue(Value)
						.ToString();
				return ModeName.Equals(
					TEXT("Build"),
					ESearchCase::IgnoreCase);
			}

			if (FByteProperty* ByteProperty =
				CastField<FByteProperty>(Property))
			{
				const uint8 Value =
					ByteProperty->GetPropertyValue_InContainer(
						ParameterMemory);
				const FString ModeName = ByteProperty->Enum
					? ByteProperty->Enum
						->GetDisplayNameTextByValue(Value)
						.ToString()
					: FString();
				return ModeName.Equals(
					TEXT("Build"),
					ESearchCase::IgnoreCase);
			}
		}

		return false;
	}

	return false;
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
	bool bCursorOnLandscape = false;
	if (!TraceTopDownCursor(CursorHit, &bCursorOnLandscape))
	{
		return;
	}

	LocalBuildingPivot = CursorHit.ImpactPoint;
	if (!bCursorOnLandscape)
	{
		bLocalBuildingPlacementValid = false;
		UpdateBuildingGroupPlacementVisual(false);
	}

	const FQuat DeltaRotation =
		FRotator(0.0f, LocalBuildingYaw, 0.0f).Quaternion();
	const auto ApplyLocalPreviewTransform = [this, &DeltaRotation]()
	{
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

			const FTransform& Original =
				LocalBuildingOriginalTransforms[Index];
			const FVector RelativeLocation =
				Original.GetLocation() - LocalBuildingOriginalPivot;
			FTransform PreviewTransform = Original;
			PreviewTransform.SetLocation(
				LocalBuildingPivot +
				DeltaRotation.RotateVector(RelativeLocation));
			PreviewTransform.SetRotation(
				DeltaRotation * Original.GetRotation());
			Actor->SetActorTransform(
				PreviewTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
	};

	ApplyLocalPreviewTransform();

	FVector SnapCorrection = FVector::ZeroVector;
	AActor* SnappedMovingWall = nullptr;
	AActor* SnappedExistingWall = nullptr;
	if (FindBuildingConnectionSnap(
			LocalBuildingGroup,
			SnapCorrection,
			SnappedMovingWall,
			SnappedExistingWall))
	{
		LocalBuildingPivot += SnapCorrection;
		ApplyLocalPreviewTransform();
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

void ABotanicusPlayerController::UpdateCommunicationDoorPreview()
{
	if (!bCommunicationDoorPlacementActive ||
		!IsValid(CommunicationDoorPreviewActor) ||
		LocalDoorCandidateLocations.Num() == 0)
	{
		return;
	}

	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit))
	{
		return;
	}

	int32 BestIndex = INDEX_NONE;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (int32 Index = 0;
		 Index < LocalDoorCandidateLocations.Num();
		 ++Index)
	{
		const float DistanceSquared = FVector::DistSquared2D(
			CursorHit.ImpactPoint,
			FVector(LocalDoorCandidateLocations[Index]));
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestIndex = Index;
		}
	}

	if (BestIndex == INDEX_NONE ||
		!LocalDoorCandidateYaws.IsValidIndex(BestIndex))
	{
		return;
	}

	LocalCommunicationDoorCandidateIndex = BestIndex;
	CommunicationDoorPreviewActor->SetActorLocationAndRotation(
		FVector(LocalDoorCandidateLocations[BestIndex]),
		FRotator(0.0f, LocalDoorCandidateYaws[BestIndex], 0.0f));
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
				if (!bHighlighted)
				{
					if (UMeshComponent* Mesh =
						Cast<UMeshComponent>(Primitive))
					{
						Mesh->SetOverlayMaterial(nullptr);
					}
				}
			}
		}
	}
}

void ABotanicusPlayerController::UpdateBuildingGroupPlacementVisual(
	bool bPlacementValid)
{
	UMaterialInterface* OverlayMaterial =
		bPlacementValid
			? ValidBuildingPlacementMaterial
			: InvalidBuildingPlacementMaterial;

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
				Primitive->SetRenderCustomDepth(true);
				Primitive->SetCustomDepthStencilValue(
					bPlacementValid ? 1 : 2);
				if (UMeshComponent* Mesh =
					Cast<UMeshComponent>(Primitive))
				{
					Mesh->SetOverlayMaterial(OverlayMaterial);
				}
			}
		}
	}
}

bool ABotanicusPlayerController::TraceTopDownCursor(
	FHitResult& OutHit,
	bool* bOutOnLandscape) const
{
	if (bOutOnLandscape)
	{
		*bOutOnLandscape = false;
	}

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
			if (bOutOnLandscape)
			{
				*bOutOnLandscape = true;
			}
			return true;
		}

		// Keep X/Y controllable beyond the Landscape edge so the preview can
		// turn red instead of freezing at the last valid point.
		const float FallbackPlaneZ = LocalBuildingPivot.Z;
		const float DistanceAlongRay =
			(FallbackPlaneZ - WorldOrigin.Z) / WorldDirection.Z;
		if (DistanceAlongRay >= 0.0f)
		{
			const FVector FallbackPoint =
				WorldOrigin + WorldDirection * DistanceAlongRay;
			OutHit = FHitResult();
			OutHit.bBlockingHit = true;
			OutHit.Location = FallbackPoint;
			OutHit.ImpactPoint = FallbackPoint;
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
	if (bBuildingTopDownViewActive)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& RoofComponent :
			 TopDownHiddenRoofComponents)
		{
			if (RoofComponent.IsValid())
			{
				QueryParams.AddIgnoredActor(
					RoofComponent->GetOwner());
			}
		}
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

	if (!TryAcquireBuildingGroupLock(Group))
	{
		ClientMessage(
			TEXT("Ce bâtiment est déjà en cours de modification par un autre joueur."));
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
	bServerBuildingPlacementValid = true;

	ClientBeginBuildingGroupMove(
		Group,
		ServerBuildingOriginalPivot,
		ServerBuildingInitialYaw);
}

void ABotanicusPlayerController::ServerPurchaseTestBuilding_Implementation()
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn || ServerBuildingGroup.Num() > 0)
	{
		return;
	}

	AActor* TemplateSeed = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Candidate = *ActorIt;
		if (!IsStructuralBuildingActor(Candidate) ||
			Candidate->ActorHasTag(PurchasedBuildingTag))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			TemplateSeed = Candidate;
		}
	}

	const TArray<AActor*> TemplateGroup =
		BuildCompleteBuildingGroup(TemplateSeed);
	if (!TemplateSeed || TemplateGroup.Num() == 0)
	{
		ClientMessage(
			TEXT("Aucun bâtiment de référence disponible pour cet achat test."));
		return;
	}

	const FVector TemplatePivot =
		CalculateBuildingGroupPivot(TemplateGroup);
	const FVector InitialOffset(3500.0f, 0.0f, 0.0f);
	TArray<AActor*> PurchasedGroup;
	PurchasedGroup.Reserve(TemplateGroup.Num());
	for (AActor* TemplateActor : TemplateGroup)
	{
		if (!IsValid(TemplateActor))
		{
			continue;
		}

		FTransform SpawnTransform = TemplateActor->GetActorTransform();
		SpawnTransform.AddToTranslation(InitialOffset);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* PurchasedActor = World->SpawnActor<AActor>(
			TemplateActor->GetClass(),
			SpawnTransform,
			SpawnParameters);
		if (!PurchasedActor)
		{
			for (AActor* SpawnedActor : PurchasedGroup)
			{
				if (IsValid(SpawnedActor))
				{
					SpawnedActor->Destroy();
				}
			}
			ClientMessage(TEXT("Impossible de créer le bâtiment test."));
			return;
		}

		PurchasedActor->Tags = TemplateActor->Tags;
		PurchasedActor->Tags.AddUnique(PurchasedBuildingTag);
		ConfigurePurchasedActorForNetworking(PurchasedActor);
		PurchasedGroup.Add(PurchasedActor);
	}

	if (PurchasedGroup.Num() == 0 ||
		!TryAcquireBuildingGroupLock(PurchasedGroup))
	{
		for (AActor* SpawnedActor : PurchasedGroup)
		{
			if (IsValid(SpawnedActor))
			{
				SpawnedActor->Destroy();
			}
		}
		return;
	}

	ServerBuildingGroup.Reset(PurchasedGroup.Num());
	ServerBuildingOriginalTransforms.Reset(PurchasedGroup.Num());
	for (AActor* PurchasedActor : PurchasedGroup)
	{
		ServerBuildingGroup.Add(PurchasedActor);
		ServerBuildingOriginalTransforms.Add(
			PurchasedActor->GetActorTransform());
	}

	ServerBuildingOriginalPivot = TemplatePivot + InitialOffset;
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
	bServerBuildingPlacementValid = false;
	bServerBuildingPurchasePlacement = true;

	ClientBeginBuildingGroupMove(
		PurchasedGroup,
		ServerBuildingOriginalPivot,
		ServerBuildingInitialYaw);
	ClientMessage(
		TEXT(
			"Bâtiment test acheté gratuitement : choisissez son emplacement."));
	BroadcastPurchasedBuildingSnapshot(true);
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Server spawned purchased building group with %d replicated actors for %s."),
		PurchasedGroup.Num(),
		PlayerState ? *PlayerState->GetPlayerName() : TEXT("UnknownPlayer"));
}

void ABotanicusPlayerController::ServerOrderTestDelivery_Implementation()
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn)
	{
		return;
	}

	ABotanicusDeliveryZoneActor* DeliveryZone = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ABotanicusDeliveryZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		const float DistanceSquared = FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			ZoneIt->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			DeliveryZone = *ZoneIt;
		}
	}

	if (!DeliveryZone)
	{
		FVector ZoneLocation =
			ControlledPawn->GetActorLocation() +
			ControlledPawn->GetActorForwardVector() * 600.0f;
		float GroundHeight = ZoneLocation.Z;
		if (FindLandscapeHeight(
				FVector2D(ZoneLocation.X, ZoneLocation.Y),
				GroundHeight))
		{
			ZoneLocation.Z = GroundHeight;
		}

		FActorSpawnParameters ZoneSpawnParameters;
		ZoneSpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		DeliveryZone =
			World->SpawnActor<ABotanicusDeliveryZoneActor>(
				ZoneLocation,
				FRotator::ZeroRotator,
				ZoneSpawnParameters);
	}

	if (!DeliveryZone)
	{
		ClientMessage(TEXT("Impossible de créer la zone de livraison."));
		return;
	}

	int32 ParcelIndex = 0;
	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(World);
		 ParcelIt;
		 ++ParcelIt)
	{
		if (FVector::DistSquared2D(
				ParcelIt->GetActorLocation(),
				DeliveryZone->GetActorLocation()) <
			FMath::Square(600.0f))
		{
			++ParcelIndex;
		}
	}

	FActorSpawnParameters ParcelSpawnParameters;
	ParcelSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABotanicusDeliveryParcelActor* Parcel =
		World->SpawnActor<ABotanicusDeliveryParcelActor>(
			DeliveryZone->GetParcelSpawnLocation(ParcelIndex),
			FRotator::ZeroRotator,
			ParcelSpawnParameters);
	if (!Parcel)
	{
		ClientMessage(TEXT("La commande n'a pas pu être livrée."));
		return;
	}

	Parcel->InitializeParcel(TEXT("SeedPacket_Test"), 1);
	ClientMessage(
		TEXT(
			"Commande livrée : quittez la vue top-down et récupérez le colis avec E."));
}

void ABotanicusPlayerController::
	ServerOrderTestLargeEquipment_Implementation()
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn)
	{
		return;
	}

	ABotanicusDeliveryZoneActor* DeliveryZone = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ABotanicusDeliveryZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		const float DistanceSquared = FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			ZoneIt->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			DeliveryZone = *ZoneIt;
		}
	}

	if (!DeliveryZone)
	{
		FVector ZoneLocation =
			ControlledPawn->GetActorLocation() +
			ControlledPawn->GetActorForwardVector() * 600.0f;
		float GroundHeight = ZoneLocation.Z;
		if (FindLandscapeHeight(
				FVector2D(ZoneLocation.X, ZoneLocation.Y),
				GroundHeight))
		{
			ZoneLocation.Z = GroundHeight;
		}

		FActorSpawnParameters ZoneSpawnParameters;
		ZoneSpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		DeliveryZone =
			World->SpawnActor<ABotanicusDeliveryZoneActor>(
				ZoneLocation,
				FRotator::ZeroRotator,
				ZoneSpawnParameters);
	}

	if (!DeliveryZone)
	{
		ClientMessage(TEXT("Impossible de creer la zone de livraison."));
		return;
	}

	int32 EquipmentIndex = 0;
	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(
			 World);
		 EquipmentIt;
		 ++EquipmentIt)
	{
		if (!IsValid(EquipmentIt->GetCarrier()) &&
			FVector::DistSquared2D(
				EquipmentIt->GetActorLocation(),
				DeliveryZone->GetActorLocation()) <
				FMath::Square(700.0f))
		{
			++EquipmentIndex;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABotanicusLargeEquipmentActor* Equipment =
		World->SpawnActor<ABotanicusLargeEquipmentActor>(
			DeliveryZone->GetParcelSpawnLocation(EquipmentIndex) +
				FVector(0.0f, 0.0f, 35.0f),
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!Equipment)
	{
		ClientMessage(TEXT("Le gros equipement n'a pas pu etre livre."));
		return;
	}

	ClientMessage(
		TEXT(
			"Gros equipement livre : regardez-le et appuyez sur E pour le placer."));
}

void ABotanicusPlayerController::
	ServerToggleCarryLargeEquipment_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(Equipment) || !ControlledPawn)
	{
		return;
	}

	const bool bAlreadyCarried =
		Equipment->GetCarrier() == ControlledPawn;
	if (bAlreadyCarried)
	{
		return;
	}

	if (!IsLookingAtWorldItem(Equipment, 500.0f))
	{
		ClientMessage(
			TEXT(
				"Regardez l'equipement et rapprochez-vous pour le porter."));
		return;
	}

	IBotanicusInteractable::Execute_Interact(
		Equipment,
		ControlledPawn);
	if (ABotanicusCharacter* BotanicusCharacter =
			Cast<ABotanicusCharacter>(ControlledPawn);
		Equipment->GetCarrier() == BotanicusCharacter)
	{
		Equipment->BeginPlacement(BotanicusCharacter);
		ClientBeginLargeEquipmentPlacement(Equipment);
	}
}

void ABotanicusPlayerController::
	ServerBeginLargeEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!IsValid(Equipment) ||
		!BotanicusCharacter ||
		Equipment->GetCarrier() != BotanicusCharacter ||
		Equipment->IsInPlacementMode())
	{
		return;
	}

	Equipment->BeginPlacement(BotanicusCharacter);
}

void ABotanicusPlayerController::
	ServerUpdateLargeEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw)
{
	if (!IsValid(Equipment) ||
		Equipment->GetCarrier() != GetPawn() ||
		!Equipment->IsInPlacementMode())
	{
		return;
	}

	FTransform PlacementTransform;
	const bool bIsValid = ResolveLargeEquipmentPlacement(
		Equipment,
		FVector(RequestedLocation),
		RequestedYaw,
		PlacementTransform);
	Equipment->UpdatePlacement(PlacementTransform, bIsValid);
}

void ABotanicusPlayerController::
	ServerConfirmLargeEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	if (!IsValid(Equipment) ||
		Equipment->GetCarrier() != GetPawn() ||
		!Equipment->IsInPlacementMode())
	{
		return;
	}

	FTransform PlacementTransform;
	const bool bIsValid = ResolveLargeEquipmentPlacement(
		Equipment,
		Equipment->GetActorLocation(),
		Equipment->GetActorRotation().Yaw,
		PlacementTransform);
	Equipment->UpdatePlacement(PlacementTransform, bIsValid);
	if (bIsValid)
	{
		Equipment->ConfirmPlacement();
	}
	else
	{
		ClientMessage(
			TEXT("Le serveur a refuse cette position."));
	}
}

void ABotanicusPlayerController::
	ServerCancelLargeEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	if (IsValid(Equipment) &&
		Equipment->GetCarrier() == GetPawn() &&
		Equipment->IsInPlacementMode())
	{
		Equipment->CancelPlacement();
	}
}

void ABotanicusPlayerController::
	ClientBeginLargeEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	BeginLargeEquipmentPlacement(Equipment);
}

void ABotanicusPlayerController::
	ServerCollectDeliveryParcel_Implementation(
		ABotanicusDeliveryParcelActor* Parcel)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(Parcel) || !ControlledPawn ||
		!IsLookingAtWorldItem(Parcel, 450.0f))
	{
		ClientMessage(
			TEXT(
				"Regardez le colis et rapprochez-vous pour le prendre."));
		return;
	}

	IBotanicusInteractable::Execute_Interact(
		Parcel,
		ControlledPawn);
}

void ABotanicusPlayerController::
	ServerConfirmCommunicationDoor_Implementation(int32 CandidateIndex)
{
	UWorld* World = GetWorld();
	if (!World ||
		!ServerDoorCandidateLocations.IsValidIndex(CandidateIndex) ||
		!ServerDoorCandidateYaws.IsValidIndex(CandidateIndex) ||
		!ServerDoorCandidatePurchasedWalls.IsValidIndex(CandidateIndex) ||
		!ServerDoorCandidateExistingWalls.IsValidIndex(CandidateIndex))
	{
		return;
	}

	AActor* PurchasedWall =
		ServerDoorCandidatePurchasedWalls[CandidateIndex];
	AActor* ExistingWall =
		ServerDoorCandidateExistingWalls[CandidateIndex];
	if (!IsValid(PurchasedWall) || !IsValid(ExistingWall))
	{
		ServerCancelCommunicationDoor();
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusCommunicationDoorActor* Door =
		World->SpawnActor<ABotanicusCommunicationDoorActor>(
			FVector(ServerDoorCandidateLocations[CandidateIndex]),
			FRotator(
				0.0f,
				ServerDoorCandidateYaws[CandidateIndex],
				0.0f),
			SpawnParameters);
	if (!Door)
	{
		return;
	}
	ConfigurePurchasedActorForNetworking(Door);

	const TArray<FName> RemovedWallNames = {
		PurchasedWall->GetFName(),
		ExistingWall->GetFName()};
	if (ABotanicusGameMode* BotanicusGameMode =
			Cast<ABotanicusGameMode>(World->GetAuthGameMode()))
	{
		for (const FName WallName : RemovedWallNames)
		{
			BotanicusGameMode->RegisterRemovedBuildingActor(WallName);
		}
	}

	for (TActorIterator<ABotanicusPlayerController> ControllerIt(World);
		 ControllerIt;
		 ++ControllerIt)
	{
		ControllerIt->ClientHideRemovedBuildingActors(RemovedWallNames);
	}
	PurchasedWall->Destroy();
	ExistingWall->Destroy();

	ServerDoorCandidatePurchasedWalls.Reset();
	ServerDoorCandidateExistingWalls.Reset();
	ServerDoorCandidateLocations.Reset();
	ServerDoorCandidateYaws.Reset();
	ClientEndCommunicationDoorPlacement(true);

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Created a replicated communication door at %s."),
		*Door->GetActorLocation().ToString());
}

void ABotanicusPlayerController::
	ServerCancelCommunicationDoor_Implementation()
{
	ServerDoorCandidatePurchasedWalls.Reset();
	ServerDoorCandidateExistingWalls.Reset();
	ServerDoorCandidateLocations.Reset();
	ServerDoorCandidateYaws.Reset();
	ClientEndCommunicationDoorPlacement(false);
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
	const bool bLandscapeFound = FindLandscapeHeight(
		FVector2D(GroundedPivot.X, GroundedPivot.Y),
		LandscapeHeight);
	if (bLandscapeFound)
	{
		GroundedPivot.Z = LandscapeHeight + ServerBuildingGroundOffset;
	}

	ApplyServerBuildingGroupTransform(GroundedPivot, NewYaw);

	FVector SnapCorrection = FVector::ZeroVector;
	AActor* SnappedMovingWall = nullptr;
	AActor* SnappedExistingWall = nullptr;
	if (FindBuildingConnectionSnap(
			ServerBuildingGroup,
			SnapCorrection,
			SnappedMovingWall,
			SnappedExistingWall))
	{
		GroundedPivot += SnapCorrection;
		ApplyServerBuildingGroupTransform(GroundedPivot, NewYaw);
		ServerSnappedMovingWall = SnappedMovingWall;
		ServerSnappedExistingWall = SnappedExistingWall;
	}
	else
	{
		ServerSnappedMovingWall = nullptr;
		ServerSnappedExistingWall = nullptr;
	}

	bServerBuildingPlacementValid =
		bLandscapeFound && IsServerBuildingGroupPlacementValid();
	ClientUpdateBuildingPlacementValidity(
		bServerBuildingPlacementValid);
	if (bServerBuildingPurchasePlacement)
	{
		BroadcastPurchasedBuildingSnapshot(false);
	}
}

void ABotanicusPlayerController::ServerConfirmBuildingGroupMove_Implementation()
{
	if (!bServerBuildingPlacementValid ||
		!IsServerBuildingGroupPlacementValid())
	{
		bServerBuildingPlacementValid = false;
		ClientUpdateBuildingPlacementValidity(false);
		ClientMessage(
			TEXT("Le serveur refuse ce placement : collision ou terrain invalide."));
		return;
	}

	for (AActor* Actor : ServerBuildingGroup)
	{
		if (IsValid(Actor))
		{
			Actor->ForceNetUpdate();
		}
	}

	const bool bBeginDoorPlacement =
		bServerBuildingPurchasePlacement &&
		BuildCommunicationDoorCandidates(ServerBuildingGroup);
	if (bServerBuildingPurchasePlacement)
	{
		BroadcastPurchasedBuildingSnapshot(true);
	}
	ClearServerBuildingGroupMove();
	ClientEndBuildingGroupMove(true);
	if (bBeginDoorPlacement)
	{
		ClientBeginCommunicationDoorPlacement(
			ServerDoorCandidateLocations,
			ServerDoorCandidateYaws);
	}
}

void ABotanicusPlayerController::ServerCancelBuildingGroupMove_Implementation()
{
	if (bServerBuildingPurchasePlacement)
	{
		for (TActorIterator<ABotanicusPlayerController> ControllerIt(
				 GetWorld());
			 ControllerIt;
			 ++ControllerIt)
		{
			ControllerIt->ClientCancelPurchasedBuildingSnapshot();
		}
		for (AActor* Actor : ServerBuildingGroup)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}
	else
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
	bLocalBuildingPlacementValid = true;
	SetBuildingGroupHighlighted(true);
	UpdateBuildingGroupPlacementVisual(true);

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
	bLocalBuildingPlacementValid = true;

	ClientMessage(
		bConfirmed
			? TEXT("Déplacement du bâtiment confirmé.")
			: TEXT("Déplacement du bâtiment annulé."));
}

void ABotanicusPlayerController::
	ClientUpdateBuildingPlacementValidity_Implementation(
		bool bPlacementValid)
{
	bLocalBuildingPlacementValid = bPlacementValid;
	UpdateBuildingGroupPlacementVisual(bPlacementValid);
}

void ABotanicusPlayerController::
	ClientApplyPurchasedBuildingSnapshot_Implementation(
		const TArray<FName>& ActorNames,
		const TArray<FTransform>& ActorTransforms)
{
	QueuePurchasedBuildingSnapshot(ActorNames, ActorTransforms);
}

void ABotanicusPlayerController::
	ClientApplyPurchasedBuildingPreviewSnapshot_Implementation(
		const TArray<FName>& ActorNames,
		const TArray<FTransform>& ActorTransforms)
{
	QueuePurchasedBuildingSnapshot(ActorNames, ActorTransforms);
}

void ABotanicusPlayerController::
	ClientCancelPurchasedBuildingSnapshot_Implementation()
{
	GetWorldTimerManager().ClearTimer(
		PurchasedBuildingSnapshotRetryTimer);
	PendingPurchasedBuildingActorNames.Reset();
	PendingPurchasedBuildingTransforms.Reset();
	PurchasedBuildingSnapshotRetryCount = 0;
}

void ABotanicusPlayerController::
	ClientBeginCommunicationDoorPlacement_Implementation(
		const TArray<FVector_NetQuantize10>& CandidateLocations,
		const TArray<float>& CandidateYaws)
{
	if (!bBuildingTopDownViewActive ||
		CandidateLocations.Num() == 0 ||
		CandidateLocations.Num() != CandidateYaws.Num())
	{
		return;
	}

	LocalDoorCandidateLocations = CandidateLocations;
	LocalDoorCandidateYaws = CandidateYaws;
	LocalCommunicationDoorCandidateIndex = 0;
	bCommunicationDoorPlacementActive = true;

	if (!IsValid(CommunicationDoorPreviewActor))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CommunicationDoorPreviewActor =
			GetWorld()->SpawnActor<ABotanicusCommunicationDoorActor>(
				FVector(LocalDoorCandidateLocations[0]),
				FRotator(0.0f, LocalDoorCandidateYaws[0], 0.0f),
				SpawnParameters);
		if (CommunicationDoorPreviewActor)
		{
			CommunicationDoorPreviewActor->SetPreviewMode(true);
		}
	}

	ClientMessage(
		TEXT(
			"Choisissez la porte de communication avec la souris, puis clic gauche pour confirmer. Clic droit/Echap pour annuler."));
}

void ABotanicusPlayerController::
	ClientEndCommunicationDoorPlacement_Implementation(bool bCreated)
{
	bCommunicationDoorPlacementActive = false;
	LocalDoorCandidateLocations.Reset();
	LocalDoorCandidateYaws.Reset();
	LocalCommunicationDoorCandidateIndex = INDEX_NONE;
	if (IsValid(CommunicationDoorPreviewActor))
	{
		CommunicationDoorPreviewActor->Destroy();
		CommunicationDoorPreviewActor = nullptr;
	}

	ClientMessage(
		bCreated
			? TEXT("Porte de communication créée.")
			: TEXT("Création de la porte de communication annulée."));
}

void ABotanicusPlayerController::
	ClientHideRemovedBuildingActors_Implementation(
		const TArray<FName>& ActorNames)
{
	TSet<FName> NamesToHide;
	for (const FName ActorName : ActorNames)
	{
		NamesToHide.Add(ActorName);
	}
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (IsValid(Actor) && NamesToHide.Contains(Actor->GetFName()))
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}
}

void ABotanicusPlayerController::ServerPlacePing_Implementation(
	FVector_NetQuantize10 RequestedLocation)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World ||
		!ControlledPawn ||
		RequestedLocation.ContainsNaN())
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastServerPingTime < PingCooldown)
	{
		return;
	}

	if (FVector::DistSquared(
			ControlledPawn->GetActorLocation(),
			RequestedLocation) >
		FMath::Square(MaximumPingDistance))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = ControlledPawn;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABotanicusPingMarker* Ping = World->SpawnActor<ABotanicusPingMarker>(
		RequestedLocation + FVector(0.0f, 0.0f, 80.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!Ping)
	{
		return;
	}

	const FString PlayerName =
		PlayerState ? PlayerState->GetPlayerName() : TEXT("Player");
	Ping->InitializePing(PlayerName, PingLifeTime);
	LastServerPingTime = CurrentTime;

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Ping placed by %s at %s; lifetime %.1fs."),
		*PlayerName,
		*RequestedLocation.ToString(),
		PingLifeTime);
}

void ABotanicusPlayerController::ServerCreatePath_Implementation(
	const TArray<FVector_NetQuantize10>& RequestedPoints)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World ||
		!ControlledPawn ||
		RequestedPoints.Num() < 2 ||
		RequestedPoints.Num() > 64)
	{
		return;
	}

	TArray<FVector> ValidatedPoints;
	ValidatedPoints.Reserve(RequestedPoints.Num());
	TArray<TObjectPtr<ABotanicusPathActor>> ConnectedPaths;
	ConnectedPaths.SetNumZeroed(2);
	float TotalLength = 0.0f;

	for (int32 PointIndex = 0;
		 PointIndex < RequestedPoints.Num();
		 ++PointIndex)
	{
		const FVector_NetQuantize10& RequestedPoint =
			RequestedPoints[PointIndex];
		if (RequestedPoint.ContainsNaN())
		{
			return;
		}

		float LandscapeHeight = 0.0f;
		if (!FindLandscapeHeight(
			FVector2D(RequestedPoint.X, RequestedPoint.Y),
			LandscapeHeight))
		{
			return;
		}

		FVector GroundedPoint(
			RequestedPoint.X,
			RequestedPoint.Y,
			LandscapeHeight + 8.0f);
		if (PointIndex == 0 ||
			PointIndex == RequestedPoints.Num() - 1)
		{
			FVector SnappedPoint;
			ABotanicusPathActor* ConnectedPath = nullptr;
			if (SnapPathPoint(
				GroundedPoint,
				SnappedPoint,
				ConnectedPath))
			{
				GroundedPoint = SnappedPoint;
				ConnectedPaths[
					PointIndex == 0 ? 0 : 1] = ConnectedPath;
			}
		}

		if (ValidatedPoints.Num() > 0)
		{
			const float SegmentLength =
				FVector::Dist2D(ValidatedPoints.Last(), GroundedPoint);
			if (SegmentLength < 25.0f || SegmentLength > 5000.0f)
			{
				return;
			}
			TotalLength += SegmentLength;
			if (TotalLength > 50000.0f)
			{
				return;
			}
		}
		ValidatedPoints.Add(GroundedPoint);
	}

	if (FVector::DistSquared2D(
		ControlledPawn->GetActorLocation(),
		ValidatedPoints[0]) >
		FMath::Square(MaximumBuildingEditDistance))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusPathActor* Path =
		World->SpawnActor<ABotanicusPathActor>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!Path)
	{
		return;
	}

	Path->InitializeConfirmedPath(ValidatedPoints);
	if (ConnectedPaths[0])
	{
		ConnectedPaths[0]->AddJunctionPoint(ValidatedPoints[0]);
	}
	if (ConnectedPaths[1])
	{
		ConnectedPaths[1]->AddJunctionPoint(ValidatedPoints.Last());
	}
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Created a replicated path with %d points and length %.0f cm."),
		ValidatedPoints.Num(),
		TotalLength);
}

void ABotanicusPlayerController::ServerDeletePathSegment_Implementation(
	ABotanicusPathActor* Path,
	int32 SegmentIndex,
	FVector_NetQuantize10 RequestedHitLocation)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World ||
		!ControlledPawn ||
		!IsValid(Path) ||
		Path->IsPreviewPath() ||
		RequestedHitLocation.ContainsNaN())
	{
		return;
	}

	const TArray<FVector> OriginalPoints = Path->GetPathWorldPoints();
	if (!OriginalPoints.IsValidIndex(SegmentIndex) ||
		!OriginalPoints.IsValidIndex(SegmentIndex + 1))
	{
		return;
	}

	int32 VerifiedSegmentIndex = INDEX_NONE;
	FVector VerifiedClosestPoint = FVector::ZeroVector;
	float VerifiedDistance = 0.0f;
	if (!Path->FindClosestSegment(
			FVector(RequestedHitLocation),
			VerifiedSegmentIndex,
			VerifiedClosestPoint,
			VerifiedDistance) ||
		VerifiedSegmentIndex != SegmentIndex ||
		VerifiedDistance > ExistingPathSnapDistance ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			VerifiedClosestPoint) >
			FMath::Square(MaximumBuildingEditDistance))
	{
		return;
	}

	TArray<FVector> LeftPoints;
	for (int32 PointIndex = 0;
		 PointIndex <= SegmentIndex;
		 ++PointIndex)
	{
		LeftPoints.Add(OriginalPoints[PointIndex]);
	}

	TArray<FVector> RightPoints;
	for (int32 PointIndex = SegmentIndex + 1;
		 PointIndex < OriginalPoints.Num();
		 ++PointIndex)
	{
		RightPoints.Add(OriginalPoints[PointIndex]);
	}

	const TArray<FVector> OriginalJunctions =
		Path->GetJunctionWorldPoints();
	int32 RemainingPieceCount = 0;
	auto SpawnRemainingPiece =
		[this, World, &OriginalJunctions, &RemainingPieceCount](
			const TArray<FVector>& PiecePoints)
		{
			if (PiecePoints.Num() < 2)
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABotanicusPathActor* Piece =
				World->SpawnActor<ABotanicusPathActor>(
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					SpawnParameters);
			if (!Piece)
			{
				return;
			}

			Piece->InitializeConfirmedPath(PiecePoints);
			TArray<FVector> RetainedJunctions;
			for (const FVector& JunctionPoint : OriginalJunctions)
			{
				FVector ClosestPoint;
				float Distance = 0.0f;
				if (Piece->FindClosestPoint(
						JunctionPoint,
						ClosestPoint,
						Distance) &&
					Distance <= ExistingPathSnapDistance)
				{
					RetainedJunctions.Add(JunctionPoint);
				}
			}
			Piece->RestoreJunctionPoints(RetainedJunctions);
			++RemainingPieceCount;
		};

	SpawnRemainingPiece(LeftPoints);
	SpawnRemainingPiece(RightPoints);
	Path->Destroy();

	// Junction markers live on the path that existed first. Removing a
	// connected segment can therefore orphan a marker on another actor.
	// Retain only markers that still touch at least one different path.
	TArray<ABotanicusPathActor*> RemainingPaths;
	for (TActorIterator<ABotanicusPathActor> PathIt(World);
		 PathIt;
		 ++PathIt)
	{
		ABotanicusPathActor* RemainingPath = *PathIt;
		if (IsValid(RemainingPath) &&
			RemainingPath != Path &&
			!RemainingPath->IsPreviewPath())
		{
			RemainingPaths.Add(RemainingPath);
		}
	}
	for (ABotanicusPathActor* RemainingPath : RemainingPaths)
	{
		TArray<FVector> RetainedJunctions;
		for (const FVector& JunctionPoint :
			 RemainingPath->GetJunctionWorldPoints())
		{
			bool bStillConnected = false;
			for (ABotanicusPathActor* OtherPath : RemainingPaths)
			{
				if (OtherPath == RemainingPath)
				{
					continue;
				}

				FVector ClosestPoint;
				float Distance = 0.0f;
				if (OtherPath->FindClosestPoint(
						JunctionPoint,
						ClosestPoint,
						Distance) &&
					Distance <= 50.0f)
				{
					bStillConnected = true;
					break;
				}
			}
			if (bStillConnected)
			{
				RetainedJunctions.Add(JunctionPoint);
			}
		}
		RemainingPath->RestoreJunctionPoints(RetainedJunctions);
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Deleted path segment %d; generated %d remaining path piece(s)."),
		SegmentIndex,
		RemainingPieceCount);
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

bool ABotanicusPlayerController::IsPlainBuildingWall(
	const AActor* Actor) const
{
	if (!IsEbsBuildingActor(Actor))
	{
		return false;
	}

	const FString ClassName = Actor->GetClass()->GetName();
	return ClassName.Contains(TEXT("Wall"), ESearchCase::IgnoreCase) &&
		!ClassName.Contains(TEXT("Roof"), ESearchCase::IgnoreCase) &&
		!ClassName.Contains(TEXT("Tri"), ESearchCase::IgnoreCase) &&
		!ClassName.Contains(TEXT("Slope"), ESearchCase::IgnoreCase) &&
		!ClassName.Contains(TEXT("Door"), ESearchCase::IgnoreCase) &&
		!ClassName.Contains(TEXT("Window"), ESearchCase::IgnoreCase);
}

bool ABotanicusPlayerController::FindBuildingConnectionSnap(
	const TArray<TObjectPtr<AActor>>& MovingGroup,
	FVector& OutCorrection,
	AActor*& OutMovingWall,
	AActor*& OutExistingWall) const
{
	OutCorrection = FVector::ZeroVector;
	OutMovingWall = nullptr;
	OutExistingWall = nullptr;

	UWorld* World = GetWorld();
	if (!World || MovingGroup.Num() == 0)
	{
		return false;
	}

	TSet<const AActor*> MovingActors;
	for (AActor* Actor : MovingGroup)
	{
		if (IsValid(Actor))
		{
			MovingActors.Add(Actor);
		}
	}

	float BestCorrectionSize = TNumericLimits<float>::Max();
	for (AActor* MovingWall : MovingGroup)
	{
		if (!IsPlainBuildingWall(MovingWall))
		{
			continue;
		}

		FVector MovingOrigin;
		FVector MovingExtent;
		MovingWall->GetActorBounds(
			true,
			MovingOrigin,
			MovingExtent);
		const bool bMovingNormalIsX =
			MovingExtent.X <= MovingExtent.Y;

		for (TActorIterator<AActor> ExistingIt(World);
			 ExistingIt;
			 ++ExistingIt)
		{
			AActor* ExistingWall = *ExistingIt;
			if (!IsValid(ExistingWall) ||
				MovingActors.Contains(ExistingWall) ||
				!IsPlainBuildingWall(ExistingWall))
			{
				continue;
			}

			FVector ExistingOrigin;
			FVector ExistingExtent;
			ExistingWall->GetActorBounds(
				true,
				ExistingOrigin,
				ExistingExtent);
			const bool bExistingNormalIsX =
				ExistingExtent.X <= ExistingExtent.Y;
			if (bMovingNormalIsX != bExistingNormalIsX)
			{
				continue;
			}

			const float NormalCorrection =
				bMovingNormalIsX
					? ExistingOrigin.X - MovingOrigin.X
					: ExistingOrigin.Y - MovingOrigin.Y;
			const float CorrectionSize =
				FMath::Abs(NormalCorrection);
			if (CorrectionSize > BuildingConnectionSnapDistance ||
				CorrectionSize >= BestCorrectionSize)
			{
				continue;
			}

			const float MovingTangentMin =
				bMovingNormalIsX
					? MovingOrigin.Y - MovingExtent.Y
					: MovingOrigin.X - MovingExtent.X;
			const float MovingTangentMax =
				bMovingNormalIsX
					? MovingOrigin.Y + MovingExtent.Y
					: MovingOrigin.X + MovingExtent.X;
			const float ExistingTangentMin =
				bMovingNormalIsX
					? ExistingOrigin.Y - ExistingExtent.Y
					: ExistingOrigin.X - ExistingExtent.X;
			const float ExistingTangentMax =
				bMovingNormalIsX
					? ExistingOrigin.Y + ExistingExtent.Y
					: ExistingOrigin.X + ExistingExtent.X;
			const float TangentOverlap =
				FMath::Min(MovingTangentMax, ExistingTangentMax) -
				FMath::Max(MovingTangentMin, ExistingTangentMin);
			if (TangentOverlap < 180.0f)
			{
				continue;
			}

			const float VerticalOverlap =
				FMath::Min(
					MovingOrigin.Z + MovingExtent.Z,
					ExistingOrigin.Z + ExistingExtent.Z) -
				FMath::Max(
					MovingOrigin.Z - MovingExtent.Z,
					ExistingOrigin.Z - ExistingExtent.Z);
			if (VerticalOverlap < 200.0f)
			{
				continue;
			}

			BestCorrectionSize = CorrectionSize;
			OutCorrection = bMovingNormalIsX
				? FVector(NormalCorrection, 0.0f, 0.0f)
				: FVector(0.0f, NormalCorrection, 0.0f);
			OutMovingWall = MovingWall;
			OutExistingWall = ExistingWall;
		}
	}

	return IsValid(OutMovingWall) && IsValid(OutExistingWall);
}

bool ABotanicusPlayerController::BuildCommunicationDoorCandidates(
	const TArray<AActor*>& PurchasedGroup)
{
	check(HasAuthority());

	ServerDoorCandidatePurchasedWalls.Reset();
	ServerDoorCandidateExistingWalls.Reset();
	ServerDoorCandidateLocations.Reset();
	ServerDoorCandidateYaws.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TSet<const AActor*> PurchasedActors;
	for (AActor* Actor : PurchasedGroup)
	{
		if (IsValid(Actor))
		{
			PurchasedActors.Add(Actor);
		}
	}

	TArray<AActor*> ExistingWalls;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (IsValid(Actor) &&
			!PurchasedActors.Contains(Actor) &&
			IsPlainBuildingWall(Actor))
		{
			ExistingWalls.Add(Actor);
		}
	}

	for (AActor* PurchasedWall : PurchasedGroup)
	{
		if (!IsPlainBuildingWall(PurchasedWall))
		{
			continue;
		}

		FVector PurchasedOrigin;
		FVector PurchasedExtent;
		PurchasedWall->GetActorBounds(
			true,
			PurchasedOrigin,
			PurchasedExtent);
		const bool bPurchasedNormalIsX =
			PurchasedExtent.X <= PurchasedExtent.Y;

		for (AActor* ExistingWall : ExistingWalls)
		{
			FVector ExistingOrigin;
			FVector ExistingExtent;
			ExistingWall->GetActorBounds(
				true,
				ExistingOrigin,
				ExistingExtent);
			const bool bExistingNormalIsX =
				ExistingExtent.X <= ExistingExtent.Y;
			if (bPurchasedNormalIsX != bExistingNormalIsX)
			{
				continue;
			}

			const float NormalDistance =
				bPurchasedNormalIsX
					? FMath::Abs(
						PurchasedOrigin.X - ExistingOrigin.X)
					: FMath::Abs(
						PurchasedOrigin.Y - ExistingOrigin.Y);
			const float MaximumNormalDistance =
				(bPurchasedNormalIsX
					 ? PurchasedExtent.X + ExistingExtent.X
					 : PurchasedExtent.Y + ExistingExtent.Y) +
				80.0f;
			if (NormalDistance > MaximumNormalDistance)
			{
				continue;
			}

			const float PurchasedTangentMin =
				bPurchasedNormalIsX
					? PurchasedOrigin.Y - PurchasedExtent.Y
					: PurchasedOrigin.X - PurchasedExtent.X;
			const float PurchasedTangentMax =
				bPurchasedNormalIsX
					? PurchasedOrigin.Y + PurchasedExtent.Y
					: PurchasedOrigin.X + PurchasedExtent.X;
			const float ExistingTangentMin =
				bPurchasedNormalIsX
					? ExistingOrigin.Y - ExistingExtent.Y
					: ExistingOrigin.X - ExistingExtent.X;
			const float ExistingTangentMax =
				bPurchasedNormalIsX
					? ExistingOrigin.Y + ExistingExtent.Y
					: ExistingOrigin.X + ExistingExtent.X;
			const float OverlapMin =
				FMath::Max(PurchasedTangentMin, ExistingTangentMin);
			const float OverlapMax =
				FMath::Min(PurchasedTangentMax, ExistingTangentMax);
			if (OverlapMax - OverlapMin < 180.0f)
			{
				continue;
			}

			const float BottomZ = FMath::Max(
				PurchasedOrigin.Z - PurchasedExtent.Z,
				ExistingOrigin.Z - ExistingExtent.Z);
			const float TopZ = FMath::Min(
				PurchasedOrigin.Z + PurchasedExtent.Z,
				ExistingOrigin.Z + ExistingExtent.Z);
			if (TopZ - BottomZ < 200.0f)
			{
				continue;
			}

			FVector CandidateLocation =
				(PurchasedOrigin + ExistingOrigin) * 0.5f;
			if (bPurchasedNormalIsX)
			{
				CandidateLocation.Y = (OverlapMin + OverlapMax) * 0.5f;
			}
			else
			{
				CandidateLocation.X = (OverlapMin + OverlapMax) * 0.5f;
			}
			CandidateLocation.Z = BottomZ;

			bool bDuplicate = false;
			for (const FVector_NetQuantize10& ExistingCandidate :
				 ServerDoorCandidateLocations)
			{
				if (FVector::DistSquared(
						FVector(ExistingCandidate),
						CandidateLocation) <
					FMath::Square(100.0f))
				{
					bDuplicate = true;
					break;
				}
			}
			if (bDuplicate)
			{
				continue;
			}

			ServerDoorCandidatePurchasedWalls.Add(PurchasedWall);
			ServerDoorCandidateExistingWalls.Add(ExistingWall);
			ServerDoorCandidateLocations.Add(
				FVector_NetQuantize10(CandidateLocation));
			ServerDoorCandidateYaws.Add(
				bPurchasedNormalIsX ? 0.0f : 90.0f);
		}
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Detected %d communication-door candidate wall pair(s)."),
		ServerDoorCandidateLocations.Num());
	return ServerDoorCandidateLocations.Num() > 0;
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

bool ABotanicusPlayerController::TryAcquireBuildingGroupLock(
	const TArray<AActor*>& GroupActors)
{
	check(HasAuthority());

	for (auto LockIt = ActiveBuildingEditLocks.CreateIterator();
		 LockIt;
		 ++LockIt)
	{
		if (!LockIt.Key().IsValid() || !LockIt.Value().IsValid())
		{
			LockIt.RemoveCurrent();
		}
	}

	for (AActor* BuildingActor : GroupActors)
	{
		if (!IsValid(BuildingActor))
		{
			continue;
		}

		if (const TWeakObjectPtr<ABotanicusPlayerController>* LockOwner =
			ActiveBuildingEditLocks.Find(BuildingActor))
		{
			if (LockOwner->IsValid() && LockOwner->Get() != this)
			{
				return false;
			}
		}
	}

	for (AActor* BuildingActor : GroupActors)
	{
		if (IsValid(BuildingActor))
		{
			ActiveBuildingEditLocks.Add(BuildingActor, this);
		}
	}

	return true;
}

void ABotanicusPlayerController::ReleaseBuildingGroupLock()
{
	if (!HasAuthority())
	{
		return;
	}

	for (auto LockIt = ActiveBuildingEditLocks.CreateIterator();
		 LockIt;
		 ++LockIt)
	{
		if (!LockIt.Key().IsValid() ||
			!LockIt.Value().IsValid() ||
			LockIt.Value().Get() == this)
		{
			LockIt.RemoveCurrent();
		}
	}
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

void ABotanicusPlayerController::BroadcastPurchasedBuildingSnapshot(
	bool bReliable)
{
	check(HasAuthority());

	UWorld* World = GetWorld();
	if (!World || ServerBuildingGroup.Num() == 0)
	{
		return;
	}

	TArray<FName> ActorNames;
	TArray<FTransform> ActorTransforms;
	ActorNames.Reserve(ServerBuildingGroup.Num());
	ActorTransforms.Reserve(ServerBuildingGroup.Num());
	for (AActor* Actor : ServerBuildingGroup)
	{
		if (IsValid(Actor))
		{
			ActorNames.Add(Actor->GetFName());
			ActorTransforms.Add(Actor->GetActorTransform());
		}
	}

	for (TActorIterator<ABotanicusPlayerController> ControllerIt(World);
		 ControllerIt;
		 ++ControllerIt)
	{
		if (bReliable)
		{
			ControllerIt->ClientApplyPurchasedBuildingSnapshot(
				ActorNames,
				ActorTransforms);
		}
		else
		{
			ControllerIt->ClientApplyPurchasedBuildingPreviewSnapshot(
				ActorNames,
				ActorTransforms);
		}
	}
}

void ABotanicusPlayerController::QueuePurchasedBuildingSnapshot(
	const TArray<FName>& ActorNames,
	const TArray<FTransform>& ActorTransforms)
{
	if (ActorNames.Num() == 0 ||
		ActorNames.Num() != ActorTransforms.Num())
	{
		return;
	}

	if (PendingPurchasedBuildingActorNames != ActorNames)
	{
		PurchasedBuildingSnapshotRetryCount = 0;
	}
	PendingPurchasedBuildingActorNames = ActorNames;
	PendingPurchasedBuildingTransforms = ActorTransforms;
	TryApplyPurchasedBuildingSnapshot();
}

void ABotanicusPlayerController::TryApplyPurchasedBuildingSnapshot()
{
	UWorld* World = GetWorld();
	if (!World ||
		PendingPurchasedBuildingActorNames.Num() == 0 ||
		PendingPurchasedBuildingActorNames.Num() !=
			PendingPurchasedBuildingTransforms.Num())
	{
		return;
	}

	TMap<FName, AActor*> ActorsByName;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (IsValid(Actor))
		{
			ActorsByName.Add(Actor->GetFName(), Actor);
		}
	}

	int32 AppliedActorCount = 0;
	for (int32 Index = 0;
		 Index < PendingPurchasedBuildingActorNames.Num();
		 ++Index)
	{
		AActor* const* FoundActor =
			ActorsByName.Find(PendingPurchasedBuildingActorNames[Index]);
		if (!FoundActor || !IsValid(*FoundActor))
		{
			continue;
		}

		(*FoundActor)->SetActorTransform(
			PendingPurchasedBuildingTransforms[Index],
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		(*FoundActor)->MarkComponentsRenderStateDirty();
		++AppliedActorCount;
	}

	if (AppliedActorCount <
			PendingPurchasedBuildingActorNames.Num() &&
		PurchasedBuildingSnapshotRetryCount < 100)
	{
		++PurchasedBuildingSnapshotRetryCount;
		if (!GetWorldTimerManager().IsTimerActive(
				PurchasedBuildingSnapshotRetryTimer))
		{
			GetWorldTimerManager().SetTimer(
				PurchasedBuildingSnapshotRetryTimer,
				this,
				&ABotanicusPlayerController::
					TryApplyPurchasedBuildingSnapshot,
				0.1f,
				false);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(
		PurchasedBuildingSnapshotRetryTimer);
	const int32 ExpectedActorCount =
		PendingPurchasedBuildingActorNames.Num();
	PendingPurchasedBuildingActorNames.Reset();
	PendingPurchasedBuildingTransforms.Reset();
	PurchasedBuildingSnapshotRetryCount = 0;

	// Only refresh EBS once all replicated actors exist locally. This is the
	// refresh that entering top-down previously happened to trigger.
	AdvanceEbsViewMode();
	AdvanceEbsViewMode();
	AdvanceEbsViewMode();
	if (!bBuildingTopDownViewActive)
	{
		ForceFirstPersonView();
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Client applied purchased building snapshot to %d/%d actors and refreshed EBS."),
		AppliedActorCount,
		ExpectedActorCount);
}

bool ABotanicusPlayerController::
	IsServerBuildingGroupPlacementValid() const
{
	UWorld* World = GetWorld();
	if (!World || ServerBuildingGroup.Num() == 0)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusWholeBuildingPlacement),
		false);
	QueryParams.AddIgnoredActor(GetPawn());
	for (AActor* GroupMember : ServerBuildingGroup)
	{
		QueryParams.AddIgnoredActor(GroupMember);
	}

	for (AActor* GroupMember : ServerBuildingGroup)
	{
		if (!IsValid(GroupMember))
		{
			continue;
		}

		const FBox LocalBounds =
			GroupMember->CalculateComponentsBoundingBoxInLocalSpace(
				false,
				true);
		if (!LocalBounds.IsValid)
		{
			continue;
		}

		const FTransform ActorTransform =
			GroupMember->GetActorTransform();
		const FVector WorldCenter =
			ActorTransform.TransformPosition(LocalBounds.GetCenter());
		FVector WorldExtent =
			LocalBounds.GetExtent() * ActorTransform.GetScale3D().GetAbs();

		// A tiny inset allows snapped pieces and neighbouring buildings to
		// touch exactly at their faces without being treated as penetrations.
		WorldExtent.X = FMath::Max(1.0f, WorldExtent.X - 5.0f);
		WorldExtent.Y = FMath::Max(1.0f, WorldExtent.Y - 5.0f);
		WorldExtent.Z = FMath::Max(1.0f, WorldExtent.Z - 5.0f);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			WorldCenter,
			ActorTransform.GetRotation(),
			ObjectQuery,
			FCollisionShape::MakeBox(WorldExtent),
			QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OverlappedActor = Overlap.GetActor();
			if (!IsValid(OverlappedActor) ||
				OverlappedActor->IsA<ALandscapeProxy>() ||
				ServerBuildingGroup.Contains(OverlappedActor))
			{
				continue;
			}

			// The two wall modules deliberately share the same plane when a
			// purchased building is magnetised to an existing one. They will
			// be replaced by the communication doorway after confirmation.
			if (GroupMember == ServerSnappedMovingWall &&
				OverlappedActor == ServerSnappedExistingWall)
			{
				continue;
			}

			// A complete modular facade normally contains several wall,
			// foundation and roof actors. Once one wall pair has established
			// the magnetic connection plane, allow the other EBS pieces to
			// meet only inside a shallow band around that same plane. A real
			// building-on-building penetration extends beyond this band and
			// therefore remains invalid.
			if (IsValid(ServerSnappedMovingWall) &&
				IsValid(ServerSnappedExistingWall) &&
				IsEbsBuildingActor(GroupMember) &&
				IsEbsBuildingActor(OverlappedActor))
			{
				FVector SnapMovingOrigin;
				FVector SnapMovingExtent;
				ServerSnappedMovingWall->GetActorBounds(
					true,
					SnapMovingOrigin,
					SnapMovingExtent);
				const bool bConnectionNormalIsX =
					SnapMovingExtent.X <= SnapMovingExtent.Y;
				const float ConnectionPlane =
					bConnectionNormalIsX
						? SnapMovingOrigin.X
						: SnapMovingOrigin.Y;

				FVector MovingOrigin;
				FVector MovingExtent;
				GroupMember->GetActorBounds(
					true,
					MovingOrigin,
					MovingExtent);
				FVector ExistingOrigin;
				FVector ExistingExtent;
				OverlappedActor->GetActorBounds(
					true,
					ExistingOrigin,
					ExistingExtent);

				const float MovingMin =
					bConnectionNormalIsX
						? MovingOrigin.X - MovingExtent.X
						: MovingOrigin.Y - MovingExtent.Y;
				const float MovingMax =
					bConnectionNormalIsX
						? MovingOrigin.X + MovingExtent.X
						: MovingOrigin.Y + MovingExtent.Y;
				const float ExistingMin =
					bConnectionNormalIsX
						? ExistingOrigin.X - ExistingExtent.X
						: ExistingOrigin.Y - ExistingExtent.Y;
				const float ExistingMax =
					bConnectionNormalIsX
						? ExistingOrigin.X + ExistingExtent.X
						: ExistingOrigin.Y + ExistingExtent.Y;
				const float IntersectionMin =
					FMath::Max(MovingMin, ExistingMin);
				const float IntersectionMax =
					FMath::Min(MovingMax, ExistingMax);

				if (IntersectionMax >= IntersectionMin &&
					IntersectionMin >=
						ConnectionPlane -
							BuildingConnectionOverlapDepth &&
					IntersectionMax <=
						ConnectionPlane +
							BuildingConnectionOverlapDepth)
				{
					continue;
				}
			}

			return false;
		}
	}

	return true;
}

void ABotanicusPlayerController::ClearServerBuildingGroupMove()
{
	ReleaseBuildingGroupLock();
	ServerBuildingGroup.Reset();
	ServerBuildingOriginalTransforms.Reset();
	ServerBuildingOriginalPivot = FVector::ZeroVector;
	ServerBuildingInitialYaw = 0.0f;
	ServerBuildingGroundOffset = 0.0f;
	ServerSnappedMovingWall = nullptr;
	ServerSnappedExistingWall = nullptr;
	bServerBuildingPlacementValid = true;
	bServerBuildingPurchasePlacement = false;
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
