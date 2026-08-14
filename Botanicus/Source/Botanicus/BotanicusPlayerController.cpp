// Copyright Epic Games, Inc. All Rights Reserved.


#include "BotanicusPlayerController.h"
#include "BotanicusCharacter.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "Sales/BotanicusSalesDisplayActor.h"
#include "Sales/BotanicusSalePotActor.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/BotanicusSelfCheckoutActor.h"
#include "Storage/BotanicusStorageShelfActor.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"
#include "Preparation/BotanicusWorkSurfaceActor.h"
#include "Preparation/BotanicusComputerActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "BotanicusCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Botanicus.h"
#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Building/BotanicusCatalogBuildingActor.h"
#include "Building/BotanicusCommunicationDoorActor.h"
#include "Catalog/BotanicusBuildingCatalogSubsystem.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusDeliveryZoneActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "DrawDebugHelpers.h"
#include "Economy/BotanicusRefundZoneActor.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Growing/BotanicusMultiPlantPotActor.h"
#include "Growing/BotanicusWateringCanActor.h"
#include "Growing/BotanicusWaterReserveActor.h"
#include "Interaction/BotanicusInteractable.h"
#include "Online/BotanicusMultiplayerSubsystem.h"
#include "Path/BotanicusPathActor.h"
#include "Ping/BotanicusPingMarker.h"
#include "UI/BotanicusQuickBarWidget.h"
#include "UI/BotanicusCarryProgressWidget.h"
#include "UI/BotanicusBuildingCatalogWidget.h"
#include "UI/BotanicusOrderCatalogWidget.h"
#include "UI/BotanicusWorkbenchUpgradeWidget.h"
#include "UI/BotanicusDevelopmentPanelWidget.h"
#include "UI/BotanicusSharedFundsWidget.h"
#include "UI/BotanicusShopObjectivesWidget.h"
#include "UI/BotanicusDaySummaryWidget.h"
#include "UI/BotanicusClockWidget.h"
#include "UI/BotanicusStorageQuantityWidget.h"
#include "UI/BotanicusInteractionTargetWidget.h"
#include "UI/BotanicusHudMessageWidget.h"
#include "UI/BotanicusHudLayoutWidget.h"
#include "UI/BotanicusThrowPowerWidget.h"
#include "UI/BotanicusTopDownToolbarWidget.h"
#include "Visitors/BotanicusVisitorZoneActor.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	const FName PlayerPurchasedBuildingTag(
		TEXT("BotanicusPurchasedBuilding"));
	constexpr int32 PottingSoilGroundStackLimit = 5;

	bool IsSalePotItemKey(FName ItemKey)
	{
		return ItemKey == TEXT("SalePot") ||
			ItemKey == TEXT("SalePotSquare");
	}

	bool CanCharacterUseWaterReserve(
		const ABotanicusCharacter* Character,
		const ABotanicusWaterReserveActor* WaterReserve,
		float MaximumDistance)
	{
		if (!IsValid(Character) ||
			!IsValid(WaterReserve) ||
			MaximumDistance <= 0.0f)
		{
			return false;
		}

		FVector TargetOrigin;
		FVector TargetExtent;
		WaterReserve->GetActorBounds(
			true,
			TargetOrigin,
			TargetExtent);
		const FVector ViewLocation =
			Character->GetPawnViewLocation();
		const FVector ToReserve =
			TargetOrigin - ViewLocation;
		const float Distance = ToReserve.Size();
		if (Distance <= KINDA_SMALL_NUMBER ||
			Distance > MaximumDistance)
		{
			return false;
		}

		const AController* CharacterController =
			Character->GetController();
		const FVector AimDirection =
			CharacterController
				? CharacterController->GetControlRotation().Vector()
				: Character->GetActorForwardVector();
		return FVector::DotProduct(
				AimDirection,
				ToReserve / Distance) >=
			FMath::Cos(FMath::DegreesToRadians(45.0f));
	}

	ABotanicusWateringCanActor* FindCarriedWateringCan(
		UWorld* World,
		const ABotanicusCharacter* Character)
	{
		if (!World || !IsValid(Character))
		{
			return nullptr;
		}
		if (ABotanicusWateringCanActor* LinkedCan =
			Character->GetHeldWateringCan())
		{
			return LinkedCan;
		}
		for (TActorIterator<ABotanicusWateringCanActor> CanIt(World);
			 CanIt;
			 ++CanIt)
		{
			if (CanIt->GetCarrier() == Character)
			{
				return *CanIt;
			}
		}
		return nullptr;
	}

	ABotanicusGameState* GetSharedGameState(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World
			? World->GetGameState<ABotanicusGameState>()
			: nullptr;
	}

	bool TrySpendSharedFunds(const UObject* Context, int32 Amount)
	{
		ABotanicusGameState* GameState = GetSharedGameState(Context);
		return GameState && GameState->TrySpendSharedFunds(Amount);
	}

	void AddSharedFunds(const UObject* Context, int32 Amount)
	{
		if (ABotanicusGameState* GameState =
			GetSharedGameState(Context))
		{
			GameState->AddSharedFunds(Amount);
		}
	}

	ABotanicusPreparationWorkbenchActor*
	FindPrimaryPreparationWorkbench(const UObject* Context)
	{
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ABotanicusPreparationWorkbenchActor> It(
				 World);
			 It;
			 ++It)
		{
			if (!It->ActorHasTag(TEXT("BotanicusPlacementPreview")))
			{
				return *It;
			}
		}
		return nullptr;
	}

	void ScheduleSharedStateAutosave(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		if (ABotanicusGameMode* GameMode =
			World
				? World->GetAuthGameMode<ABotanicusGameMode>()
				: nullptr)
		{
			GameMode->ScheduleInventoryAutosave();
		}
	}

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

	const FBotanicusItemDefinition* FindItemDefinition(
		const UObject* Context,
		FName ItemKey)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		UGameInstance* GameInstance =
			World ? World->GetGameInstance() : nullptr;
		const UBotanicusItemCatalogSubsystem* Catalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusItemCatalogSubsystem>()
				: nullptr;
		return Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	}

	const FBotanicusBuildingDefinition* FindBuildingDefinition(
		const UObject* Context,
		FName BuildingKey)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		UGameInstance* GameInstance =
			World ? World->GetGameInstance() : nullptr;
		const UBotanicusBuildingCatalogSubsystem* Catalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusBuildingCatalogSubsystem>()
				: nullptr;
		return Catalog ? Catalog->FindBuilding(BuildingKey) : nullptr;
	}

	FVector GetItemAlignmentExtent(const AActor* Actor)
	{
		if (const ABotanicusLargeEquipmentActor* Equipment =
			Cast<ABotanicusLargeEquipmentActor>(Actor))
		{
			return Equipment->GetPlacementBoxExtent().GetAbs();
		}
		if (const ABotanicusPlaceableItemActor* Item =
			Cast<ABotanicusPlaceableItemActor>(Actor))
		{
			return Item->GetPlacementBoxExtent().GetAbs();
		}
		return FVector::ZeroVector;
	}

	void DrawOrientedEdgeAlignmentGuides(
		UWorld* World,
		const FTransform& PreviewTransform,
		const FVector& PreviewExtent,
		const AActor* OtherActor,
		const FVector& OtherExtent,
		float MaximumDistance,
		float EdgeTolerance,
		float AngleTolerance)
	{
		if (!World ||
			!IsValid(OtherActor) ||
			PreviewExtent.IsNearlyZero() ||
			OtherExtent.IsNearlyZero())
		{
			return;
		}

		const FVector PreviewLocation = PreviewTransform.GetLocation();
		const FVector OtherLocation = OtherActor->GetActorLocation();
		const FVector Delta = PreviewLocation - OtherLocation;
		if (Delta.SizeSquared2D() > FMath::Square(MaximumDistance))
		{
			return;
		}

		const float PreviewYaw =
			PreviewTransform.Rotator().Yaw;
		const float OtherYaw =
			OtherActor->GetActorRotation().Yaw;
		const float AbsoluteYawDelta = FMath::Abs(
			FMath::FindDeltaAngleDegrees(PreviewYaw, OtherYaw));
		const bool bSameOrientation =
			AbsoluteYawDelta <= AngleTolerance ||
			FMath::Abs(180.0f - AbsoluteYawDelta) <=
				AngleTolerance;
		if (!bSameOrientation)
		{
			return;
		}

		const FRotator ReferenceYaw(0.0f, OtherYaw, 0.0f);
		const FVector Forward = ReferenceYaw.Vector();
		const FVector Right =
			FRotationMatrix(ReferenceYaw).GetUnitAxis(EAxis::Y);
		const float ForwardOffset =
			FVector::DotProduct(Delta, Forward);
		const float RightOffset =
			FVector::DotProduct(Delta, Right);
		const float GuideZ =
			FMath::Min(
				PreviewLocation.Z - PreviewExtent.Z,
				OtherLocation.Z - OtherExtent.Z) +
			4.0f;

		const float OtherRightEdges[2] =
			{-OtherExtent.Y, OtherExtent.Y};
		const float PreviewRightEdges[2] =
			{
				RightOffset - PreviewExtent.Y,
				RightOffset + PreviewExtent.Y
			};
		for (const float OtherEdge : OtherRightEdges)
		{
			for (const float PreviewEdge : PreviewRightEdges)
			{
				if (FMath::Abs(OtherEdge - PreviewEdge) >
					EdgeTolerance)
				{
					continue;
				}

				const float AlignedEdge =
					(OtherEdge + PreviewEdge) * 0.5f;
				const float SegmentStart =
					FMath::Min(
						-OtherExtent.X,
						ForwardOffset - PreviewExtent.X);
				const float SegmentEnd =
					FMath::Max(
						OtherExtent.X,
						ForwardOffset + PreviewExtent.X);
				DrawDebugLine(
					World,
					OtherLocation +
						Forward * SegmentStart +
						Right * AlignedEdge +
						FVector(0.0f, 0.0f, GuideZ - OtherLocation.Z),
					OtherLocation +
						Forward * SegmentEnd +
						Right * AlignedEdge +
						FVector(0.0f, 0.0f, GuideZ - OtherLocation.Z),
					FColor::Green,
					false,
					0.08f,
					0,
					4.0f);
			}
		}

		const float OtherForwardEdges[2] =
			{-OtherExtent.X, OtherExtent.X};
		const float PreviewForwardEdges[2] =
			{
				ForwardOffset - PreviewExtent.X,
				ForwardOffset + PreviewExtent.X
			};
		for (const float OtherEdge : OtherForwardEdges)
		{
			for (const float PreviewEdge : PreviewForwardEdges)
			{
				if (FMath::Abs(OtherEdge - PreviewEdge) >
					EdgeTolerance)
				{
					continue;
				}

				const float AlignedEdge =
					(OtherEdge + PreviewEdge) * 0.5f;
				const float SegmentStart =
					FMath::Min(
						-OtherExtent.Y,
						RightOffset - PreviewExtent.Y);
				const float SegmentEnd =
					FMath::Max(
						OtherExtent.Y,
						RightOffset + PreviewExtent.Y);
				DrawDebugLine(
					World,
					OtherLocation +
						Forward * AlignedEdge +
						Right * SegmentStart +
						FVector(0.0f, 0.0f, GuideZ - OtherLocation.Z),
					OtherLocation +
						Forward * AlignedEdge +
						Right * SegmentEnd +
						FVector(0.0f, 0.0f, GuideZ - OtherLocation.Z),
					FColor::Green,
					false,
					0.08f,
					0,
					4.0f);
			}
		}
	}
}

ABotanicusPlayerController::ABotanicusPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABotanicusCameraManager::StaticClass();
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		ValidPlacementMaterialFinder(
			TEXT("/Game/Botanicus/Materials/Silhouette/M_Silhouette_Hologram_Blue.M_Silhouette_Hologram_Blue"));
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

void ABotanicusPlayerController::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		PendingOrders,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		BuildingProgressionLevel,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		CarriedTransplantPlantKey,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		CarriedTransplantItemKey,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		CarriedTransplantGrowth,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		CarriedTransplantCare,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		CarriedTransplantWateringCount,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		ABotanicusPlayerController,
		bCarriedTransplantElementalDead,
		COND_OwnerOnly);
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

void ABotanicusPlayerController::BotanicusOrder(FName ItemKey)
{
	if (!ItemKey.IsNone())
	{
		if (HasAuthority())
		{
			ServerPlaceCatalogOrder_Implementation(ItemKey);
		}
		else
		{
			ServerPlaceCatalogOrder(ItemKey);
		}
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

	if (IsLocalPlayerController())
	{
		UMaterialInterface* HighlightBase =
			LoadObject<UMaterialInterface>(
				nullptr,
				TEXT(
					"/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent"));
		if (HighlightBase)
		{
			FurnitureHighlightMaterial =
				UMaterialInstanceDynamic::Create(
					HighlightBase,
					this);
			if (FurnitureHighlightMaterial)
			{
				const FLinearColor FurnitureYellow(
					1.0f,
					0.72f,
					0.02f,
					0.35f);
				FurnitureHighlightMaterial->SetVectorParameterValue(
					TEXT("Color"),
					FurnitureYellow);
				FurnitureHighlightMaterial->SetVectorParameterValue(
					TEXT("TintColor"),
					FurnitureYellow);
				FurnitureHighlightMaterial->SetScalarParameterValue(
					TEXT("Opacity"),
					0.35f);
			}

			InteractionHighlightMaterial =
				UMaterialInstanceDynamic::Create(
					HighlightBase,
					this);
			if (InteractionHighlightMaterial)
			{
				const FLinearColor InteractionBlue(
					0.02f,
					0.42f,
					1.0f,
					0.42f);
				InteractionHighlightMaterial->SetVectorParameterValue(
					TEXT("Color"),
					InteractionBlue);
				InteractionHighlightMaterial->SetVectorParameterValue(
					TEXT("TintColor"),
					InteractionBlue);
				InteractionHighlightMaterial->SetScalarParameterValue(
					TEXT("Opacity"),
					0.42f);
			}
		}
	}

	ForceFirstPersonView();
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::ForceFirstPersonView);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeHudLayoutWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeQuickBarWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeSharedFundsWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeShopObjectivesWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeDaySummaryWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeClockWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeStorageQuantityWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeInteractionTargetWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::InitializeHudMessageWidget);
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ABotanicusPlayerController::
			InitializeDevelopmentPanelWidget);
	
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
	if (bFurnitureMoveModeActive || LocalHighlightedFurniture.Num() > 0)
	{
		bFurnitureMoveModeActive = false;
		RefreshFurnitureHighlights();
	}
	SetInteractionTargetHighlighted(
		LocalInteractionHighlightActor.Get(),
		false);
	LocalInteractionHighlightActor.Reset();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->ClearTarget();
	}
	CancelEquipmentCarryCharge(true);
	CancelPlaceableItemMoveCharge();
	CancelDisplayedSalePotPickup();
	CancelStorageCollectionQuantitySelection(true);
	CancelParcelMoveCharge();
	EndWateringCanRefill(true);
	EndPlantPotAction();
	CancelQuickBarItemPlacement();
	DestroyEquippedQuickBarItem();
	if (LocalEquippedQuickBarSource.IsValid())
	{
		LocalEquippedQuickBarSource->OnQuickBarChanged.RemoveDynamic(
			this,
			&ABotanicusPlayerController::HandleEquippedQuickBarChanged);
	}
	LocalEquippedQuickBarSource.Reset();
	CancelDeliveryParcelPlacement();
	CancelLargeEquipmentPlacement();
	CancelPathPlacement();
	CancelPathDeletion();
	CancelVisitorZonePlacement();
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

	if (OrderCatalogWidget)
	{
		OrderCatalogWidget->RemoveFromParent();
		OrderCatalogWidget = nullptr;
	}
	if (WorkbenchUpgradeWidget)
	{
		WorkbenchUpgradeWidget->RemoveFromParent();
		WorkbenchUpgradeWidget = nullptr;
	}
	if (DevelopmentPanelWidget)
	{
		DevelopmentPanelWidget->RemoveFromParent();
		DevelopmentPanelWidget = nullptr;
	}
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->RemoveFromParent();
		BuildingCatalogWidget = nullptr;
	}

	if (HasAuthority())
	{
		for (TPair<FGuid, FTimerHandle>& Timer : PendingOrderTimers)
		{
			GetWorldTimerManager().ClearTimer(Timer.Value);
		}
		PendingOrderTimers.Reset();
	}

	if (HudLayoutWidget)
	{
		HudLayoutWidget->RemoveFromParent();
		HudLayoutWidget = nullptr;
		QuickBarWidget = nullptr;
		SharedFundsWidget = nullptr;
		ShopObjectivesWidget = nullptr;
		ClockWidget = nullptr;
		InteractionTargetWidget = nullptr;
		HudMessageWidget = nullptr;
	}

	if (QuickBarWidget)
	{
		QuickBarWidget->RemoveFromParent();
		QuickBarWidget = nullptr;
	}
	if (SharedFundsWidget)
	{
		SharedFundsWidget->RemoveFromParent();
		SharedFundsWidget = nullptr;
	}
	if (ShopObjectivesWidget)
	{
		ShopObjectivesWidget->RemoveFromParent();
		ShopObjectivesWidget = nullptr;
	}
	if (DaySummaryWidget)
	{
		DaySummaryWidget->RemoveFromParent();
		DaySummaryWidget = nullptr;
	}
	if (ClockWidget)
	{
		ClockWidget->RemoveFromParent();
		ClockWidget = nullptr;
	}
	if (StorageQuantityWidget)
	{
		StorageQuantityWidget->RemoveFromParent();
		StorageQuantityWidget = nullptr;
	}
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->RemoveFromParent();
		InteractionTargetWidget = nullptr;
	}
	if (HudMessageWidget)
	{
		HudMessageWidget->RemoveFromParent();
		HudMessageWidget = nullptr;
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->RemoveFromParent();
		CarryProgressWidget = nullptr;
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
	if (IsLocalPlayerController() && !HudLayoutWidget)
	{
		InitializeHudLayoutWidget();
	}

	if (IsLocalPlayerController() && !QuickBarWidget)
	{
		InitializeQuickBarWidget();
	}
	if (IsLocalPlayerController() && !SharedFundsWidget)
	{
		InitializeSharedFundsWidget();
	}
	if (IsLocalPlayerController() && !ShopObjectivesWidget)
	{
		InitializeShopObjectivesWidget();
	}
	if (IsLocalPlayerController() && !DaySummaryWidget)
	{
		InitializeDaySummaryWidget();
	}
	if (IsLocalPlayerController() && !ClockWidget)
	{
		InitializeClockWidget();
	}
	if (IsLocalPlayerController() && !StorageQuantityWidget)
	{
		InitializeStorageQuantityWidget();
	}
	if (IsLocalPlayerController() && !InteractionTargetWidget)
	{
		InitializeInteractionTargetWidget();
	}
	if (IsLocalPlayerController() && !HudMessageWidget)
	{
		InitializeHudMessageWidget();
	}
	if (LocalStorageCollectionItem &&
		!IsValid(LocalStorageCollectionItem))
	{
		CancelStorageCollectionQuantitySelection(false);
	}

	if (IsLocalPlayerController())
	{
		HideEbsDemoHud();
		LegacyWorldGuidanceRefreshAccumulator += DeltaTime;
		if (LegacyWorldGuidanceRefreshAccumulator >= 0.2f)
		{
			LegacyWorldGuidanceRefreshAccumulator = 0.0f;
			HideLegacyWorldGuidance();
		}
		if (bFurnitureMoveModeActive)
		{
			FurnitureHighlightRefreshAccumulator += DeltaTime;
			if (FurnitureHighlightRefreshAccumulator >= 0.25f)
			{
				FurnitureHighlightRefreshAccumulator = 0.0f;
				RefreshFurnitureHighlights();
			}
		}
		InteractionHighlightRefreshAccumulator += DeltaTime;
		if (InteractionHighlightRefreshAccumulator >= 0.05f)
		{
			InteractionHighlightRefreshAccumulator = 0.0f;
			RefreshInteractionTargetHighlight();
		}
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
			if (bBuildingCameraOrbitActive &&
				LocalBuildingGroup.Num() == 0)
			{
				EndBuildingCameraOrbit();
			}
			UpdateBuildingCameraOrbit();
			UpdateBuildingCameraFreeLook();
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
	UpdateEquipmentCarryCharge(DeltaTime);
	UpdatePlaceableItemMoveCharge(DeltaTime);
	UpdateDisplayedSalePotPickup(DeltaTime);
	UpdateParcelMoveCharge(DeltaTime);
	UpdateWateringCanRefill(DeltaTime);
	UpdateGardenTrowelTransplantHold(DeltaTime);
	if (bPlantPotActionHeld)
	{
		AActor* ActivePot = IsValid(LocalActivePlantPot)
			? static_cast<AActor*>(LocalActivePlantPot.Get())
			: static_cast<AActor*>(LocalActiveSalePot.Get());
		if (!IsValid(ActivePot) ||
			!IsLookingAtWorldItem(
				ActivePot,
				MaximumWorldInteractionDistance))
		{
			EndPlantPotAction();
		}
		else
		{
			PlantPotActionHoldElapsed += DeltaTime;
			if (InteractionTargetWidget)
			{
				InteractionTargetWidget->SetHoldProgress(
					FMath::Clamp(PlantPotActionHoldElapsed, 0.0f, 1.0f));
			}
		}
	}
	UpdateLargeEquipmentPlacement(DeltaTime);
	UpdateQuickBarItemPlacement(DeltaTime);
	UpdateEquippedQuickBarItem();
	UpdateThrowPowerWidget();
	UpdateDeliveryParcelPlacement(DeltaTime);
	if (InteractionTargetWidget && CarryProgressWidget &&
		CarryProgressWidget->GetVisibility() ==
			ESlateVisibility::HitTestInvisible)
	{
		InteractionTargetWidget->SetHoldProgress(
			CarryProgressWidget->GetCarryProgress());
	}
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
	if (Params.Key == EKeys::LeftAlt)
	{
		if (Params.Event == IE_Pressed)
		{
			SetHudCursorMode(true);
		}
		else if (Params.Event == IE_Released)
		{
			SetHudCursorMode(false);
		}
		return true;
	}

	if (Params.Key == EKeys::LeftShift &&
		Params.Event == IE_Released)
	{
		if (ABotanicusCharacter* BotanicusCharacter =
				Cast<ABotanicusCharacter>(GetPawn()))
		{
			BotanicusCharacter->SetSprinting(false);
		}
	}

	// Furniture mode must remain reachable even if another native catalogue
	// widget accidentally retains Visible input state.
	if (Params.Key == EKeys::B &&
		!bQuickBarReorganizationMode)
	{
		if (Params.Event == IE_Pressed)
		{
			ToggleFurnitureMoveMode();
		}
		return true;
	}

	if (Params.Key == EKeys::Tab)
	{
		if (Params.Event == IE_Pressed)
		{
			ToggleQuickBarReorganizationMode();
		}
		return true;
	}

	if (Params.Key == EKeys::N ||
		Params.Key == EKeys::F1)
	{
		if (Params.Event == IE_Pressed)
		{
			if (bQuickBarReorganizationMode)
			{
				ToggleQuickBarReorganizationMode();
			}
			ToggleDevelopmentPanel();
		}
		return true;
	}

	if (bQuickBarReorganizationMode)
	{
		if (Params.Key == EKeys::Escape &&
			Params.Event == IE_Pressed)
		{
			ToggleQuickBarReorganizationMode();
		}
		return true;
	}

	if (DevelopmentPanelWidget &&
		DevelopmentPanelWidget->GetVisibility() ==
			ESlateVisibility::Visible)
	{
		if (Params.Key == EKeys::Escape &&
			Params.Event == IE_Pressed)
		{
			ToggleDevelopmentPanel();
		}
		return true;
	}

	if (BuildingCatalogWidget &&
		BuildingCatalogWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
		{
			ToggleBuildingCatalog();
		}
		return true;
	}

	if (OrderCatalogWidget &&
		OrderCatalogWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		if (Params.Key == EKeys::Escape &&
			Params.Event == IE_Pressed)
		{
			ToggleOrderCatalog();
		}
		return true;
	}

	if (WorkbenchUpgradeWidget &&
		WorkbenchUpgradeWidget->GetVisibility() ==
			ESlateVisibility::Visible)
	{
		if (Params.Key == EKeys::Escape &&
			Params.Event == IE_Pressed)
		{
			ClosePreparationWorkbenchUpgrade();
		}
		return true;
	}

	if (IsValid(LocalStorageCollectionItem))
	{
		if (Params.Event == IE_Pressed &&
			(Params.Key == EKeys::MouseScrollUp ||
			 Params.Key == EKeys::MouseScrollDown))
		{
			AdjustStorageCollectionQuantity(
				Params.Key == EKeys::MouseScrollUp ? 1 : -1);
			return true;
		}
		if (Params.Key == EKeys::LeftMouseButton &&
			Params.Event == IE_Pressed)
		{
			ConfirmStorageCollectionQuantitySelection();
			return true;
		}
		if ((Params.Key == EKeys::RightMouseButton ||
			 Params.Key == EKeys::Escape) &&
			Params.Event == IE_Pressed)
		{
			CancelStorageCollectionQuantitySelection(true);
			return true;
		}
		if (Params.Key == EKeys::E ||
			Params.Key == EKeys::A)
		{
			return true;
		}
	}

	if (Params.Key == EKeys::LeftShift &&
		Params.Event == IE_Pressed &&
		!bBuildingTopDownViewActive &&
		!IsValid(LocalLargeEquipmentPlacement) &&
		!IsValid(LocalQuickBarItemPreview) &&
		!IsValid(LocalParcelMovePreview))
	{
		if (ABotanicusCharacter* BotanicusCharacter =
				Cast<ABotanicusCharacter>(GetPawn()))
		{
			BotanicusCharacter->SetSprinting(true);
		}
	}

	if (Params.Key == EKeys::C &&
		Params.Event == IE_Pressed &&
		!bBuildingTopDownViewActive)
	{
		if (ABotanicusCharacter* BotanicusCharacter =
				Cast<ABotanicusCharacter>(GetPawn()))
		{
			BotanicusCharacter->ToggleCrouching();
		}
		return true;
	}

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
	if (Params.Key == EKeys::G ||
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
			if (IsValid(LocalQuickBarItemPreview))
			{
				CancelQuickBarItemPlacement();
			}
			if (IsValid(LocalParcelMovePreview))
			{
				CancelDeliveryParcelPlacement();
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
		Params.Event == IE_Released)
	{
		bEquipmentCarryKeyHeld = false;
		if (IsValid(LocalHeldHeavyEquipment))
		{
			CancelEquipmentCarryCharge(true);
			return true;
		}
		if (IsValid(LocalPlaceableItemMoveCandidate))
		{
			CancelPlaceableItemMoveCharge();
			return true;
		}
		if (IsValid(LocalDisplayedSalePotPickupCandidate))
		{
			CancelDisplayedSalePotPickup();
			return true;
		}
		if (IsValid(LocalParcelMoveCandidate))
		{
			CancelParcelMoveCharge();
			return true;
		}
	}

	if (Params.Key == EKeys::E &&
		Params.Event == IE_Pressed &&
		!bBuildingTopDownViewActive &&
		(TryOpenNearbyWorkbenchUpgrade() ||
		 TryUseNearbyComputer() ||
		 TryPlacePlantOnNearbySalesDisplay() ||
		 TryBeginDisplayedSalePotPickup() ||
		 TryHandleNearbyLargeEquipment() ||
		 TryCollectNearbyDeliveryParcel() ||
		 TryMoveNearbyPlaceableItem()))
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

	if (IsValid(LocalQuickBarItemPreview) &&
		!bBuildingTopDownViewActive)
	{
		if (Params.Event == IE_Pressed &&
			(Params.Key == EKeys::MouseScrollUp ||
			 Params.Key == EKeys::MouseScrollDown))
		{
			ABotanicusStorageShelfActor* TargetShelf = nullptr;
			int32 TargetSlotIndex = INDEX_NONE;
			const bool bQuantityModifier =
				IsInputKeyDown(EKeys::LeftControl) ||
				IsInputKeyDown(EKeys::RightControl);
			if (bQuantityModifier &&
				!IsValid(LocalMovedPlaceableItem) &&
				(LocalQuickBarItemKey == TEXT("PottingSoil") ||
				 (FindAimedStorageSlot(
					  TargetShelf,
					  TargetSlotIndex) &&
				  ABotanicusStorageShelfActor::
					  IsCatalogItemCompatible(
						  this,
						  LocalQuickBarItemKey))))
			{
				AdjustQuickBarPlacementQuantity(
					Params.Key == EKeys::MouseScrollUp ? 1 : -1);
			}
			else
			{
				RotateQuickBarItemPlacement(
					Params.Key == EKeys::MouseScrollUp ? 1.0f : -1.0f);
			}
			return true;
		}

		if (Params.Key == EKeys::LeftMouseButton &&
			Params.Event == IE_Pressed)
		{
			if (IsValid(LocalMovedPlaceableItem))
			{
				ConfirmQuickBarItemPlacement();
			}
			else
			{
				bQuickBarThrowChargeActive = true;
				QuickBarThrowChargeStartTime =
					FPlatformTime::Seconds();
				InitializeThrowPowerWidget();
				if (ThrowPowerWidget)
				{
					ThrowPowerWidget->SetThrowPower(0.0f);
					ThrowPowerWidget->SetVisibility(
						ESlateVisibility::HitTestInvisible);
				}
			}
			return true;
		}

		if (Params.Key == EKeys::LeftMouseButton &&
			Params.Event == IE_Released &&
			bQuickBarThrowChargeActive)
		{
			ReleaseQuickBarThrowCharge();
			return true;
		}

		if ((Params.Key == EKeys::RightMouseButton ||
			 Params.Key == EKeys::Escape) &&
			Params.Event == IE_Pressed)
		{
			CancelQuickBarItemPlacement();
			return true;
		}
	}

	if (IsValid(LocalParcelMovePreview) &&
		!bBuildingTopDownViewActive)
	{
		if (Params.Event == IE_Pressed &&
			(Params.Key == EKeys::MouseScrollUp ||
			 Params.Key == EKeys::MouseScrollDown))
		{
			RotateDeliveryParcelPlacement(
				Params.Key == EKeys::MouseScrollUp ? 1.0f : -1.0f);
			return true;
		}

		if (Params.Key == EKeys::LeftMouseButton &&
			Params.Event == IE_Pressed)
		{
			ConfirmDeliveryParcelPlacement();
			return true;
		}

		if ((Params.Key == EKeys::RightMouseButton ||
			 Params.Key == EKeys::Escape) &&
			Params.Event == IE_Pressed)
		{
			CancelDeliveryParcelPlacement();
			return true;
		}
	}

	if (Params.Key == EKeys::A)
	{
		if (Params.Event == IE_Pressed &&
			!bBuildingTopDownViewActive &&
			!IsValid(LocalLargeEquipmentPlacement) &&
			!IsValid(LocalQuickBarItemPreview) &&
			!IsValid(LocalParcelMovePreview))
		{
			BeginQuickBarItemPlacement();
		}
		return true;
	}

	if (Params.Key == EKeys::MiddleMouseButton &&
		Params.Event == IE_Pressed)
	{
		TryPlacePing();
		return true;
	}

	if (bBuildingTopDownViewActive)
	{
		if (Params.Key == EKeys::LeftShift)
		{
			if (Params.Event == IE_Pressed &&
				LocalBuildingGroup.Num() > 0)
			{
				BeginBuildingCameraOrbit();
			}
			else if (Params.Event == IE_Released &&
				bBuildingCameraOrbitActive)
			{
				EndBuildingCameraOrbit();
			}
			return true;
		}

		if (bBuildingCameraOrbitActive &&
			(Params.Key == EKeys::LeftMouseButton ||
			 Params.Key == EKeys::RightMouseButton))
		{
			return true;
		}

		if (bDoorEditSelectionActive)
		{
			if (Params.Key == EKeys::LeftMouseButton &&
				Params.Event == IE_Pressed)
			{
				if (!IsCursorOverTopDownToolbar())
				{
					FHitResult DoorHit;
					if (TraceTopDownCursor(DoorHit) &&
						IsValid(DoorHit.GetActor()) &&
						(DoorHit.GetActor()->IsA<
							 ABotanicusCommunicationDoorActor>() ||
						 IsEbsBuildingActor(DoorHit.GetActor())))
					{
						bDoorEditSelectionActive = false;
						RefreshTopDownToolbar();
						ServerBeginManualDoorPlacement(
							DoorHit.GetActor());
					}
					else
					{
						ClientMessage(
							TEXT("Cliquez sur un bâtiment ou une porte existante."));
					}
				}
				return true;
			}
			if ((Params.Key == EKeys::RightMouseButton ||
				 Params.Key == EKeys::Escape) &&
				Params.Event == IE_Pressed)
			{
				CancelDoorEditing();
				return true;
			}
		}

		if (PendingVisitorZoneType != INDEX_NONE)
		{
			if (Params.Key == EKeys::LeftMouseButton &&
				Params.Event == IE_Pressed)
			{
				if (!IsCursorOverTopDownToolbar())
				{
					PlaceVisitorZoneAtCursor();
				}
				return true;
			}

			if ((Params.Key == EKeys::RightMouseButton ||
				 Params.Key == EKeys::Escape) &&
				Params.Event == IE_Pressed)
			{
				CancelVisitorZonePlacement();
				return true;
			}
		}

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
					bPathStrokeActive = true;
					AddPathPointAtCursor();
				}
				return true;
			}
			if (Params.Key == EKeys::LeftMouseButton &&
				Params.Event == IE_Released)
			{
				if (bPathStrokeActive && !IsCursorOverTopDownToolbar())
				{
					AddPathPointAtCursor();
				}
				bPathStrokeActive = false;
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

		if (Params.Key == EKeys::RightMouseButton &&
			LocalBuildingGroup.Num() == 0 &&
			PendingVisitorZoneType == INDEX_NONE &&
			!bCommunicationDoorPlacementActive &&
			!bPathPlacementActive &&
			!bPathDeletionActive)
		{
			if (Params.Event == IE_Pressed)
			{
				BeginBuildingCameraFreeLook();
			}
			else if (Params.Event == IE_Released)
			{
				EndBuildingCameraFreeLook();
			}
			return true;
		}

		if (bBuildingCameraFreeLookActive &&
			Params.Key == EKeys::LeftMouseButton)
		{
			return true;
		}

		if (Params.Event == IE_Pressed &&
			(Params.Key == EKeys::MouseScrollUp ||
			 Params.Key == EKeys::MouseScrollDown))
		{
			const float Direction =
				Params.Key == EKeys::MouseScrollUp ? 1.0f : -1.0f;

			// Wheel rotates the selected building. Ctrl + wheel always zooms.
			const bool bZoomRequested =
				LocalBuildingGroup.Num() == 0 ||
				IsInputKeyDown(EKeys::LeftControl) ||
				IsInputKeyDown(EKeys::RightControl);
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

	if (!bBuildingTopDownViewActive &&
		Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Pressed &&
			(TryUseGardenTrowelForTransplant() ||
			 TryPlaceSelectedSalePotOnWorkbench() ||
			 TryPlacePlantOnNearbySalesDisplay() ||
			 TryCheckoutNearbyRegister() ||
			 TryBeginNearbyParcelCut() ||
			 TryRefillHeldWateringCan() ||
			 TryBeginNearbyPlantPotAction() ||
			 TryBeginNearbySalePotAction()))
		{
			return true;
		}
		if (Params.Event == IE_Released &&
			bWaterRefillActionHeld)
		{
			EndWateringCanRefill(true);
			return true;
		}
		if (Params.Event == IE_Released &&
			bParcelCutActionHeld)
		{
			EndParcelCut();
			return true;
		}
		if (Params.Event == IE_Released &&
			bTrowelTransplantActionHeld)
		{
			CancelGardenTrowelTransplantHold();
			return true;
		}
		if (Params.Event == IE_Released &&
			bPlantPotActionHeld)
		{
			EndPlantPotAction();
			return true;
		}
	}

	// Never forward left click to EBS' damage/destruction trace. Quickbar
	// quantities are consumed only by an authoritative confirmed placement.
	if (Params.Key == EKeys::LeftMouseButton)
	{
		return true;
	}
	if (!bBuildingTopDownViewActive &&
		Params.Key == EKeys::RightMouseButton &&
		Params.Event == IE_Pressed &&
		TryTogglePlantInspection())
	{
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
	return bBuildingTopDownViewActive ||
		bQuickBarReorganizationMode ||
		IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalParcelMovePreview);
}

void ABotanicusPlayerController::ToggleFurnitureMoveMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	bFurnitureMoveModeActive = !bFurnitureMoveModeActive;
	FurnitureHighlightRefreshAccumulator = 0.0f;
	if (bFurnitureMoveModeActive)
	{
		RefreshInteractionTargetHighlight();
		RefreshFurnitureHighlights();
	}
	else
	{
		RefreshFurnitureHighlights();
		RefreshInteractionTargetHighlight();
	}
	ServerSetFurnitureMoveMode(bFurnitureMoveModeActive);
	ClientMessage(
		bFurnitureMoveModeActive
			? TEXT(
				"MODE MEUBLES ACTIF : meubles en jaune, appuyez sur E pour selectionner celui a deplacer avec son contenu.")
			: TEXT("MODE MEUBLES DESACTIVE."));
}

bool ABotanicusPlayerController::IsFurnitureActor(
	const AActor* Actor) const
{
	if (!IsValid(Actor) ||
		Actor->ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		return false;
	}

	FName ItemKey = NAME_None;
	if (const ABotanicusLargeEquipmentActor* Equipment =
			Cast<ABotanicusLargeEquipmentActor>(Actor))
	{
		ItemKey = Equipment->GetItemKey();
	}
	else if (const ABotanicusPlaceableItemActor* Item =
				 Cast<ABotanicusPlaceableItemActor>(Actor))
	{
		ItemKey = Item->GetItemKey();
	}
	return ItemKey == TEXT("PreparationWorkbench") ||
		ItemKey == TEXT("CommandComputer") ||
		ItemKey == TEXT("CashRegister") ||
		ItemKey == TEXT("SelfCheckout") ||
		ItemKey == TEXT("SalesDisplay") ||
		ItemKey == TEXT("WaterReserve") ||
		ItemKey.ToString().StartsWith(TEXT("WorkSurface")) ||
		ItemKey.ToString().StartsWith(TEXT("StorageShelf"));
}

void ABotanicusPlayerController::SetFurnitureActorHighlighted(
	AActor* Actor,
	bool bHighlighted) const
{
	if (!IsValid(Actor))
	{
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (Primitive)
		{
			// Stencil 1 is the project's yellow selection outline.
			Primitive->SetRenderCustomDepth(bHighlighted);
			Primitive->SetCustomDepthStencilValue(
				bHighlighted ? 1 : 0);
			if (UMeshComponent* Mesh =
					Cast<UMeshComponent>(Primitive))
			{
				Mesh->SetOverlayMaterial(
					bHighlighted
						? FurnitureHighlightMaterial
						: nullptr);
			}
		}
	}
}

void ABotanicusPlayerController::RefreshFurnitureHighlights()
{
	TSet<TWeakObjectPtr<AActor>> CurrentFurniture;
	if (bFurnitureMoveModeActive && GetWorld())
	{
		for (TActorIterator<ABotanicusLargeEquipmentActor> It(GetWorld());
			 It;
			 ++It)
		{
			if (IsFurnitureActor(*It))
			{
				CurrentFurniture.Add(*It);
				SetFurnitureActorHighlighted(*It, true);
			}
		}
		for (TActorIterator<ABotanicusPlaceableItemActor> It(GetWorld());
			 It;
			 ++It)
		{
			if (IsFurnitureActor(*It))
			{
				CurrentFurniture.Add(*It);
				SetFurnitureActorHighlighted(*It, true);
			}
		}
	}

	for (const TWeakObjectPtr<AActor>& Previous :
		 LocalHighlightedFurniture)
	{
		if (Previous.IsValid() &&
			!CurrentFurniture.Contains(Previous))
		{
			SetFurnitureActorHighlighted(Previous.Get(), false);
		}
	}
	LocalHighlightedFurniture = MoveTemp(CurrentFurniture);
}

void ABotanicusPlayerController::SetInteractionTargetHighlighted(
	AActor* Actor,
	bool bHighlighted) const
{
	if (!IsValid(Actor))
	{
		return;
	}
	if (ABotanicusSalesDisplayActor* SalesDisplay =
			Cast<ABotanicusSalesDisplayActor>(Actor);
		IsValid(SalesDisplay) && !SalesDisplay->IsEmpty())
	{
		SalesDisplay->SetDisplayedSalePotHighlighted(
			bHighlighted,
			InteractionHighlightMaterial);
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}

		Primitive->SetRenderCustomDepth(bHighlighted);
		Primitive->SetCustomDepthStencilValue(
			bHighlighted ? 2 : 0);
		if (UMeshComponent* Mesh =
				Cast<UMeshComponent>(Primitive))
		{
			Mesh->SetOverlayMaterial(
				bHighlighted
					? InteractionHighlightMaterial
					: nullptr);
		}
	}
}

void ABotanicusPlayerController::RefreshInteractionTargetHighlight()
{
	AActor* NewTarget = nullptr;
	const bool bInteractionViewBlocked =
		bFurnitureMoveModeActive ||
		bBuildingTopDownViewActive ||
		IsValid(LocalLargeEquipmentPlacement) ||
		IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalParcelMovePreview) ||
		(DevelopmentPanelWidget &&
		 DevelopmentPanelWidget->GetVisibility() ==
			 ESlateVisibility::Visible) ||
		(BuildingCatalogWidget &&
		 BuildingCatalogWidget->GetVisibility() ==
			 ESlateVisibility::Visible) ||
		(OrderCatalogWidget &&
		 OrderCatalogWidget->GetVisibility() ==
			 ESlateVisibility::Visible);

	if (!bInteractionViewBlocked && GetWorld() && GetPawn())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetPlayerViewPoint(ViewLocation, ViewRotation);

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(BotanicusInteractionHighlight),
			false);
		QueryParams.AddIgnoredActor(GetPawn());
		if (IsValid(LocalQuickBarItemPreview))
		{
			QueryParams.AddIgnoredActor(LocalQuickBarItemPreview);
		}

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(
				Hit,
				ViewLocation,
				ViewLocation +
					ViewRotation.Vector() *
						(MaximumWorldInteractionDistance + 200.0f),
				ECC_Visibility,
				QueryParams) &&
			FVector::DistSquared2D(
				GetPawn()->GetActorLocation(),
				Hit.ImpactPoint) <=
				FMath::Square(MaximumWorldInteractionDistance))
		{
			AActor* HitActor = Hit.GetActor();
			const bool bIsFurniture =
				IsFurnitureActor(HitActor);
			if (ABotanicusPreparationWorkbenchActor* Workbench =
					Cast<ABotanicusPreparationWorkbenchActor>(HitActor);
				IsValid(Workbench) &&
				Workbench->IsUpgradeTerminalTargeted(GetPawn()))
			{
				// The workbench itself is furniture, but its tablet remains a
				// normal close-range interaction target.
				NewTarget = Workbench;
			}
			else if (ABotanicusComputerActor* Computer =
					Cast<ABotanicusComputerActor>(HitActor);
				IsValid(Computer) &&
				IsLookingAtWorldItem(
					Computer,
					MaximumWorldInteractionDistance))
			{
				// The command computer is furniture for move mode, but remains
				// directly usable when the player looks at it in normal mode.
				NewTarget = Computer;
			}
			else if (ABotanicusSalesDisplayActor* SalesDisplay =
					Cast<ABotanicusSalesDisplayActor>(HitActor);
				IsValid(SalesDisplay) &&
				SalesDisplay->IsDisplayedSalePotTargeted(GetPawn()))
			{
				NewTarget = SalesDisplay;
			}
			else if (IsValid(HitActor) &&
				!HitActor->ActorHasTag(
					TEXT("BotanicusPlacementPreview")) &&
				!bIsFurniture &&
				HitActor->GetClass()->ImplementsInterface(
					UBotanicusInteractable::StaticClass()))
			{
				NewTarget = HitActor;
			}
		}

		// The tablet may be authored as a recessed or offset component of the
		// Blueprint workbench. Do not depend on the generic actor hit (which
		// deliberately filters furniture): use the same terminal targeting test
		// as the E input so the HUD and the action can never disagree.
		if (!NewTarget)
		{
			for (TActorIterator<ABotanicusPreparationWorkbenchActor>
					 WorkbenchIt(GetWorld());
				 WorkbenchIt;
				 ++WorkbenchIt)
			{
				if (WorkbenchIt->IsUpgradeTerminalTargeted(GetPawn()))
				{
					NewTarget = *WorkbenchIt;
					break;
				}
			}
		}

		// Blueprint-authored computers can contain recessed or child meshes.
		// Use the same targeting predicate as the E action as a fallback so the
		// HUD prompt cannot disappear solely because another component received
		// the initial visibility hit.
		if (!NewTarget)
		{
			float BestDistanceSquared =
				FMath::Square(MaximumWorldInteractionDistance);
			for (TActorIterator<ABotanicusComputerActor> ComputerIt(GetWorld());
				 ComputerIt;
				 ++ComputerIt)
			{
				if (ComputerIt->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
					!IsLookingAtWorldItem(
						*ComputerIt,
						MaximumWorldInteractionDistance))
				{
					continue;
				}

				const float DistanceSquared = FVector::DistSquared(
					ViewLocation,
					ComputerIt->GetActorLocation());
				if (DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					NewTarget = *ComputerIt;
				}
			}
		}

		// A preparation-workbench mesh may legitimately stand between the
		// camera and a sale pot snapped into one of its slots.  The generic
		// visibility trace above then selects the bench and the pot loses both
		// its blue feedback and its hold-E pickup path.  Reuse the stricter
		// world-item visibility test, which only tolerates the pot's supporting
		// workbench as an occluder.
		if (!NewTarget)
		{
			float BestDistanceSquared =
				FMath::Square(MaximumWorldInteractionDistance);
			for (TActorIterator<ABotanicusSalePotActor> PotIt(GetWorld());
				 PotIt;
				 ++PotIt)
			{
				if (PotIt->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
					!IsLookingAtWorldItem(
						*PotIt,
						MaximumWorldInteractionDistance))
				{
					continue;
				}

				const float DistanceSquared = FVector::DistSquared(
					ViewLocation,
					PotIt->GetActorLocation());
				if (DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					NewTarget = *PotIt;
				}
			}
		}
	}

	AActor* PreviousTarget =
		LocalInteractionHighlightActor.Get();
	if (PreviousTarget == NewTarget)
	{
		RefreshInteractionTargetName(NewTarget);
		return;
	}

	SetInteractionTargetHighlighted(PreviousTarget, false);
	LocalInteractionHighlightActor = NewTarget;
	SetInteractionTargetHighlighted(NewTarget, true);
	RefreshInteractionTargetName(NewTarget);
}

void ABotanicusPlayerController::RefreshInteractionTargetName(
	AActor* TargetActor)
{
	InitializeInteractionTargetWidget();
	if (!InteractionTargetWidget)
	{
		return;
	}
	if (!IsValid(TargetActor))
	{
		InteractionTargetWidget->ClearTarget();
		return;
	}
	if (const ABotanicusPreparationWorkbenchActor* Workbench =
		Cast<ABotanicusPreparationWorkbenchActor>(TargetActor);
		Workbench && Workbench->IsUpgradeTerminalTargeted(GetPawn()))
	{
		InteractionTargetWidget->SetTargetName(
			NSLOCTEXT(
				"BotanicusInteraction",
				"WorkbenchUpgradeTerminalTarget",
				"Ameliorations de l'atelier"));
		return;
	}
	if (Cast<ABotanicusComputerActor>(TargetActor))
	{
		InteractionTargetWidget->SetKeyboardPrompt(
			NSLOCTEXT(
				"BotanicusInteraction",
				"UseComputerAction",
				"UTILISER"),
			NSLOCTEXT(
				"BotanicusInteraction",
				"ComputerTargetName",
				"Ordinateur"));
		return;
	}

	FName ItemKey = NAME_None;
	bool bIsParcel = false;
	const ABotanicusSalesDisplayActor* DisplayedSalePot =
		Cast<ABotanicusSalesDisplayActor>(TargetActor);
	if (DisplayedSalePot && DisplayedSalePot->IsEmpty())
	{
		// The empty display owns a dedicated world-space prompt. Avoid showing
		// the generic HUD interaction card over the illustrated display UI.
		InteractionTargetWidget->ClearTarget();
		return;
	}
	if (DisplayedSalePot && !DisplayedSalePot->IsEmpty())
	{
		InteractionTargetWidget->SetTargetName(
			NSLOCTEXT(
				"BotanicusInteraction",
				"DisplayedSalePotTargetName",
				"Pot de vente prepare - maintenir E pour reprendre"));
		return;
	}
	if (const ABotanicusPlantPotActor* PlantPot =
		Cast<ABotanicusPlantPotActor>(TargetActor);
		PlantPot)
	{
		const ABotanicusCharacter* BotanicusCharacter =
			Cast<ABotanicusCharacter>(GetPawn());
		const UBotanicusQuickBarComponent* QuickBar = BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
		const FName SelectedItemKey = QuickBar
			? QuickBar->GetSelectedSlot().ItemKey
			: NAME_None;
		const bool bHasPlant = !PlantPot->GetPlantKey().IsNone();
		const UBotanicusPlantSubsystem* Plants = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
		const FBotanicusPlantDefinition* PlantDefinition = Plants
			? Plants->FindPlant(PlantPot->GetPlantKey())
			: nullptr;
		const FText PlantName = PlantDefinition
			? PlantDefinition->DisplayName
			: FText::FromName(PlantPot->GetPlantKey());
		if (SelectedItemKey == TEXT("GardenTrowel") && bHasPlant)
		{
			const bool bMature = PlantPot->IsMature();
			InteractionTargetWidget->SetLeftMousePrompt(
				bMature
					? NSLOCTEXT("BotanicusInteraction", "HarvestPlant", "RECOLTER LA PLANTE")
					: NSLOCTEXT("BotanicusInteraction", "UnpotPlant", "MAINTENIR POUR DEPOTER LA PLANTE"),
				PlantName,
				!bMature);
			if (bTrowelTransplantActionHeld &&
				LocalTrowelTransplantTarget == PlantPot)
			{
				InteractionTargetWidget->SetHoldProgress(
					GardenTrowelTransplantHoldElapsed /
					FMath::Max(0.1f, GardenTrowelTransplantHoldDuration));
			}
			return;
		}
		if (SelectedItemKey == TEXT("GardenTrowel") &&
			!bHasPlant && PlantPot->HasSoil())
		{
			InteractionTargetWidget->SetLeftMousePrompt(
				NSLOCTEXT("BotanicusInteraction", "RemoveSoil", "MAINTENIR POUR RETIRER LE TERREAU"),
				NSLOCTEXT("BotanicusInteraction", "GrowingPot", "Pot de culture"),
				true);
			return;
		}
		if (SelectedItemKey == TEXT("PottingSoil") && !PlantPot->HasSoil())
		{
			InteractionTargetWidget->SetLeftMousePrompt(
				NSLOCTEXT("BotanicusInteraction", "FillSoil", "MAINTENIR POUR VERSER LE TERREAU"),
				NSLOCTEXT("BotanicusInteraction", "GrowingPot", "Pot de culture"),
				true);
			return;
		}
		if (bHasPlant && QuickBar && QuickBar->HasSelectedWateringCan())
		{
			InteractionTargetWidget->SetLeftMousePrompt(
				NSLOCTEXT("BotanicusInteraction", "WaterPlantHold", "MAINTENIR POUR ARROSER LA PLANTE"),
				PlantName,
				true);
			return;
		}
		if (SelectedItemKey.IsNone() && bHasPlant)
		{
			InteractionTargetWidget->SetPlantInspectPrompt(
				PlantName);
			return;
		}
	}
	if (const ABotanicusDeliveryParcelActor* Parcel =
		Cast<ABotanicusDeliveryParcelActor>(TargetActor);
		Parcel && !Parcel->IsOpened())
	{
		const ABotanicusCharacter* BotanicusCharacter =
			Cast<ABotanicusCharacter>(GetPawn());
		const UBotanicusQuickBarComponent* QuickBar = BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
		const bool bCutterSelected =
			QuickBar &&
			QuickBar->GetSelectedSlot().ItemKey == TEXT("BoxCutter");
		if (!bCutterSelected)
		{
			InteractionTargetWidget->ClearTarget();
			return;
		}

		const FBotanicusItemDefinition* Definition =
			FindItemDefinition(this, Parcel->GetItemKey());
		const FText ParcelItemName =
			Definition && !Definition->DisplayName.IsEmpty()
				? Definition->DisplayName
				: FText::FromName(Parcel->GetItemKey());
		InteractionTargetWidget->SetLeftMousePrompt(
			NSLOCTEXT(
				"BotanicusInteraction",
				"CutParcelTapeHold",
				"MAINTENIR POUR COUPER LE SCOTCH"),
			FText::Format(
				NSLOCTEXT(
					"BotanicusInteraction",
					"ClosedParcelTargetName",
					"Colis : {0}"),
				ParcelItemName),
			true);
		if (bParcelCutActionHeld &&
			LocalActiveParcelCut == Parcel)
		{
			InteractionTargetWidget->SetHoldProgress(
				Parcel->GetCutProgress());
		}
		return;
	}
	if (const ABotanicusPlaceableItemActor* PlaceableItem =
		Cast<ABotanicusPlaceableItemActor>(TargetActor))
	{
		ItemKey = PlaceableItem->GetItemKey();
	}
	else if (const ABotanicusLargeEquipmentActor* Equipment =
		Cast<ABotanicusLargeEquipmentActor>(TargetActor))
	{
		ItemKey = Equipment->GetItemKey();
	}
	else if (const ABotanicusDeliveryParcelActor* Parcel =
		Cast<ABotanicusDeliveryParcelActor>(TargetActor))
	{
		ItemKey = Parcel->GetItemKey();
		bIsParcel = true;
	}

	FText TargetName;
	if (!ItemKey.IsNone())
	{
		const FBotanicusItemDefinition* Definition =
			FindItemDefinition(this, ItemKey);
		TargetName =
			Definition && !Definition->DisplayName.IsEmpty()
				? Definition->DisplayName
				: FText::FromName(ItemKey);
	}
	else
	{
		TargetName = FText::FromString(
			TargetActor->GetClass()->GetName().Replace(TEXT("_C"), TEXT("")));
	}

	if (bIsParcel)
	{
		TargetName = FText::Format(
			NSLOCTEXT(
				"BotanicusInteraction",
				"ParcelTargetName",
				"Colis : {0}"),
			TargetName);
	}
	InteractionTargetWidget->SetTargetName(TargetName);
}

void ABotanicusPlayerController::EnterBuildingTopDownView()
{
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return;
	}

	// Older controller Blueprints may have serialized the former 5000-unit
	// limit. Keep the expanded building overview available in those maps.
	MaximumBuildingCameraHeight =
		FMath::Max(MaximumBuildingCameraHeight, 25000.0f);
	// Keep the overview/detail transition consistent even for controller
	// Blueprints that serialized the former 15000-unit value.
	BuildingDetailViewDistance = 10000.0f;

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
	bBuildingCameraOrbitActive = false;
	bBuildingCameraOrbitInitialized = false;
	bBuildingCameraFreeLookActive = false;

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

	if (bBuildingCameraOrbitActive)
	{
		EndBuildingCameraOrbit();
	}
	if (bBuildingCameraFreeLookActive)
	{
		EndBuildingCameraFreeLook();
	}
	CancelPathPlacement();
	CancelPathDeletion();
	CancelVisitorZonePlacement();
	bDoorEditSelectionActive = false;
	if (bCommunicationDoorPlacementActive)
	{
		ServerCancelCommunicationDoor();
	}
	if (TopDownToolbarWidget)
	{
		TopDownToolbarWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (OrderCatalogWidget)
	{
		OrderCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
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
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeHudLayoutWidget();

	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!BotanicusCharacter || !BotanicusCharacter->GetQuickBarComponent())
	{
		return;
	}

	if (!QuickBarWidget)
	{
		QuickBarWidget =
			CreateWidget<UBotanicusQuickBarWidget>(
				this,
				UBotanicusQuickBarWidget::StaticClass());
	}
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
	if (!HudLayoutWidget)
	{
		QuickBarWidget->AddToPlayerScreen(10);
	}
}

void ABotanicusPlayerController::InitializeHudLayoutWidget()
{
	if (!IsLocalPlayerController() || HudLayoutWidget)
	{
		return;
	}

	static const TCHAR* LayoutClassPath =
		TEXT("/Game/Botanicus/UI/HUD/WBP_BotanicusHUD.WBP_BotanicusHUD_C");
	UClass* LayoutClass = LoadClass<UBotanicusHudLayoutWidget>(
		nullptr, LayoutClassPath);
	if (!LayoutClass)
	{
		return;
	}

	HudLayoutWidget = CreateWidget<UBotanicusHudLayoutWidget>(
		this, LayoutClass);
	if (!HudLayoutWidget)
	{
		return;
	}
	HudLayoutWidget->AddToPlayerScreen(38);

	QuickBarWidget = HudLayoutWidget->GetQuickBarWidget();
	SharedFundsWidget = HudLayoutWidget->GetCreditsWidget();
	ShopObjectivesWidget = HudLayoutWidget->GetObjectivesWidget();
	ClockWidget = HudLayoutWidget->GetClockWidget();
	InteractionTargetWidget = HudLayoutWidget->GetInteractionWidget();
	HudMessageWidget = HudLayoutWidget->GetMessageWidget();
}

void ABotanicusPlayerController::
	ToggleQuickBarReorganizationMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	InitializeQuickBarWidget();
	if (!QuickBarWidget)
	{
		return;
	}

	const bool bOpening = !bQuickBarReorganizationMode;
	if (bOpening &&
		((DevelopmentPanelWidget &&
		  DevelopmentPanelWidget->GetVisibility() ==
			  ESlateVisibility::Visible) ||
		 (OrderCatalogWidget &&
		  OrderCatalogWidget->GetVisibility() ==
			  ESlateVisibility::Visible) ||
		 (BuildingCatalogWidget &&
		  BuildingCatalogWidget->GetVisibility() ==
			  ESlateVisibility::Visible)))
	{
		ClientMessage(
			TEXT(
				"Fermez le panneau ouvert avant de reorganiser la hotbar."));
		return;
	}
	if (bOpening && bBuildingTopDownViewActive)
	{
		ClientMessage(
			TEXT(
				"Quittez la vue construction avant de reorganiser la hotbar."));
		return;
	}

	bQuickBarReorganizationMode = bOpening;
	if (bQuickBarReorganizationMode)
	{
		if (IsValid(LocalQuickBarItemPreview))
		{
			CancelQuickBarItemPlacement();
		}
		bAzertyForwardPressed = false;
		bAzertyBackwardPressed = false;
		bAzertyLeftPressed = false;
		bAzertyRightPressed = false;
	}

	QuickBarWidget->SetReorganizationMode(
		bQuickBarReorganizationMode);
	if (bQuickBarReorganizationMode)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(
			EMouseLockMode::DoNotLock);
		InputMode.SetWidgetToFocus(
			QuickBarWidget->TakeWidget());
		SetInputMode(InputMode);
		ClientMessage(
			TEXT(
				"ORGANISATION HOTBAR : glissez un objet vers la case choisie. TAB ou ECHAP pour fermer."));
	}
	else
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
}

void ABotanicusPlayerController::InitializeSharedFundsWidget()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeHudLayoutWidget();
	if (SharedFundsWidget)
	{
		return;
	}

	SharedFundsWidget =
		CreateWidget<UBotanicusSharedFundsWidget>(
			this,
			UBotanicusSharedFundsWidget::StaticClass());
	if (!SharedFundsWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the shared funds widget."));
		return;
	}

	SharedFundsWidget->AddToPlayerScreen(40);
}

void ABotanicusPlayerController::InitializeShopObjectivesWidget()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeHudLayoutWidget();
	if (ShopObjectivesWidget)
	{
		return;
	}

	ShopObjectivesWidget =
		CreateWidget<UBotanicusShopObjectivesWidget>(
			this,
			UBotanicusShopObjectivesWidget::StaticClass());
	if (!ShopObjectivesWidget)
	{
		return;
	}
	ShopObjectivesWidget->AddToPlayerScreen(39);
}

void ABotanicusPlayerController::InitializeDaySummaryWidget()
{
	if (!IsLocalPlayerController() || DaySummaryWidget)
	{
		return;
	}
	DaySummaryWidget =
		CreateWidget<UBotanicusDaySummaryWidget>(
			this,
			UBotanicusDaySummaryWidget::StaticClass());
	if (!DaySummaryWidget)
	{
		return;
	}
	DaySummaryWidget->AddToPlayerScreen(55);
}

void ABotanicusPlayerController::InitializeClockWidget()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeHudLayoutWidget();
	if (ClockWidget)
	{
		return;
	}
	ClockWidget =
		CreateWidget<UBotanicusClockWidget>(
			this,
			UBotanicusClockWidget::StaticClass());
	if (!ClockWidget)
	{
		return;
	}
	ClockWidget->AddToPlayerScreen(38);
}

void ABotanicusPlayerController::InitializeStorageQuantityWidget()
{
	if (!IsLocalPlayerController() || StorageQuantityWidget)
	{
		return;
	}
	StorageQuantityWidget =
		CreateWidget<UBotanicusStorageQuantityWidget>(
			this,
			UBotanicusStorageQuantityWidget::StaticClass());
	if (!StorageQuantityWidget)
	{
		return;
	}
	StorageQuantityWidget->AddToPlayerScreen(52);
	StorageQuantityWidget->SetAlignmentInViewport(
		FVector2D(0.5f, 0.5f));
	StorageQuantityWidget->SetDesiredSizeInViewport(
		FVector2D(310.0f, 108.0f));

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	StorageQuantityWidget->SetPositionInViewport(
		FVector2D(
			static_cast<float>(ViewportWidth) * 0.5f,
			static_cast<float>(ViewportHeight) * 0.5f + 175.0f),
		true);
	StorageQuantityWidget->SetVisibility(
		ESlateVisibility::Collapsed);
}

void ABotanicusPlayerController::InitializeInteractionTargetWidget()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeHudLayoutWidget();
	if (InteractionTargetWidget)
	{
		// Initialization can be requested again while refreshing the current
		// target. Do not clear an already initialized widget here: doing so
		// erases a locally running hold loader and is timing-dependent between
		// the listen-server player and remote clients.
		return;
	}
	InteractionTargetWidget =
		CreateWidget<UBotanicusInteractionTargetWidget>(
			this,
			UBotanicusInteractionTargetWidget::StaticClass());
	if (!InteractionTargetWidget)
	{
		return;
	}
	InteractionTargetWidget->AddToPlayerScreen(48);
	InteractionTargetWidget->SetAnchorsInViewport(
		FAnchors(0.5f, 1.0f));
	InteractionTargetWidget->SetAlignmentInViewport(
		FVector2D(0.5f, 1.0f));
	InteractionTargetWidget->SetDesiredSizeInViewport(
		FVector2D(310.0f, 108.0f));
	InteractionTargetWidget->SetPositionInViewport(
		FVector2D(0.0f, -112.0f),
		true);
	InteractionTargetWidget->ClearTarget();
}

void ABotanicusPlayerController::InitializeHudMessageWidget()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeHudLayoutWidget();
	if (HudMessageWidget)
	{
		return;
	}
	HudMessageWidget =
		CreateWidget<UBotanicusHudMessageWidget>(
			this,
			UBotanicusHudMessageWidget::StaticClass());
	if (!HudMessageWidget)
	{
		return;
	}
	HudMessageWidget->AddToPlayerScreen(49);
	HudMessageWidget->SetAnchorsInViewport(FAnchors(0.5f, 1.0f));
	HudMessageWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
	HudMessageWidget->SetDesiredSizeInViewport(FVector2D(460.0f, 108.0f));
	HudMessageWidget->SetPositionInViewport(FVector2D(0.0f, -226.0f), true);
}

void ABotanicusPlayerController::ClientMessage_Implementation(
	const FString& S,
	FName Type,
	float MsgLifeTime)
{
	if (!IsLocalPlayerController() || S.IsEmpty())
	{
		return;
	}
	InitializeHudMessageWidget();
	if (HudMessageWidget)
	{
		HudMessageWidget->ShowMessage(
			FText::FromString(S),
			MsgLifeTime > 0.0f ? MsgLifeTime : 3.0f);
	}
}

void ABotanicusPlayerController::SetHudCursorMode(bool bEnabled)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bEnabled)
	{
		const bool bAnotherUiOwnsInput =
			bQuickBarReorganizationMode ||
			bBuildingTopDownViewActive ||
			(DevelopmentPanelWidget &&
			 DevelopmentPanelWidget->GetVisibility() == ESlateVisibility::Visible) ||
			(BuildingCatalogWidget &&
			 BuildingCatalogWidget->GetVisibility() == ESlateVisibility::Visible) ||
			(OrderCatalogWidget &&
			 OrderCatalogWidget->GetVisibility() == ESlateVisibility::Visible) ||
			(WorkbenchUpgradeWidget &&
			 WorkbenchUpgradeWidget->GetVisibility() == ESlateVisibility::Visible);
		if (bAnotherUiOwnsInput)
		{
			return;
		}

		bHudCursorModeActive = true;
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}

	if (bHudCursorModeActive)
	{
		bHudCursorModeActive = false;
		const bool bAnotherUiOwnsInput =
			bQuickBarReorganizationMode ||
			bBuildingTopDownViewActive ||
			(DevelopmentPanelWidget &&
			 DevelopmentPanelWidget->GetVisibility() == ESlateVisibility::Visible) ||
			(BuildingCatalogWidget &&
			 BuildingCatalogWidget->GetVisibility() == ESlateVisibility::Visible) ||
			(OrderCatalogWidget &&
			 OrderCatalogWidget->GetVisibility() == ESlateVisibility::Visible) ||
			(WorkbenchUpgradeWidget &&
			 WorkbenchUpgradeWidget->GetVisibility() == ESlateVisibility::Visible);
		if (bAnotherUiOwnsInput)
		{
			return;
		}
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
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

void ABotanicusPlayerController::InitializeOrderCatalogWidget()
{
	if (!IsLocalPlayerController() || OrderCatalogWidget)
	{
		return;
	}

	UClass* OrderCatalogClass =
		LoadClass<UBotanicusOrderCatalogWidget>(
			nullptr,
			TEXT("/Game/Botanicus/UI/Command/WBP_CommandComputer.WBP_CommandComputer_C"));
	if (!OrderCatalogClass)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("WBP_CommandComputer could not be loaded; the command computer will not open."));
		return;
	}
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Creating command computer from %s"),
		*OrderCatalogClass->GetPathName());

	OrderCatalogWidget =
		CreateWidget<UBotanicusOrderCatalogWidget>(
			this,
			OrderCatalogClass);
	if (!OrderCatalogWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the order catalogue screen."));
		return;
	}

	OrderCatalogWidget->InitializeWithController(this);
	OrderCatalogWidget->AddToPlayerScreen(45);
	OrderCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ABotanicusPlayerController::InitializeWorkbenchUpgradeWidget()
{
	if (!IsLocalPlayerController() || WorkbenchUpgradeWidget)
	{
		return;
	}

	WorkbenchUpgradeWidget =
		CreateWidget<UBotanicusWorkbenchUpgradeWidget>(
			this,
			UBotanicusWorkbenchUpgradeWidget::StaticClass());
	if (!WorkbenchUpgradeWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the workbench upgrade panel."));
		return;
	}

	WorkbenchUpgradeWidget->AddToPlayerScreen(75);
	WorkbenchUpgradeWidget->SetVisibility(
		ESlateVisibility::Collapsed);
}

void ABotanicusPlayerController::
	InitializeDevelopmentPanelWidget()
{
	if (!IsLocalPlayerController() || DevelopmentPanelWidget)
	{
		return;
	}
	DevelopmentPanelWidget =
		CreateWidget<UBotanicusDevelopmentPanelWidget>(
			this,
			UBotanicusDevelopmentPanelWidget::StaticClass());
	if (!DevelopmentPanelWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the development command panel."));
		return;
	}
	DevelopmentPanelWidget->InitializeWithController(this);
	DevelopmentPanelWidget->AddToPlayerScreen(70);
	DevelopmentPanelWidget->SetVisibility(
		ESlateVisibility::Collapsed);
}

void ABotanicusPlayerController::InitializeBuildingCatalogWidget()
{
	if (!IsLocalPlayerController() || BuildingCatalogWidget)
	{
		return;
	}

	BuildingCatalogWidget =
		CreateWidget<UBotanicusBuildingCatalogWidget>(
			this,
			UBotanicusBuildingCatalogWidget::StaticClass());
	if (!BuildingCatalogWidget)
	{
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Could not create the building catalogue screen."));
		return;
	}

	BuildingCatalogWidget->InitializeWithController(this);
	BuildingCatalogWidget->AddToPlayerScreen(30);
	BuildingCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
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

void ABotanicusPlayerController::HideLegacyWorldGuidance()
{
	if (!GetWorld() || bBuildingTopDownViewActive)
	{
		return;
	}

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UTextRenderComponent*> TextComponents(*ActorIt);
		for (UTextRenderComponent* TextComponent : TextComponents)
		{
			if (IsValid(TextComponent) && TextComponent->IsVisible())
			{
				TextComponent->SetVisibility(false, true);
			}
		}
	}
}

void ABotanicusPlayerController::RefreshTopDownRoofVisibility()
{
	if (!IsLocalPlayerController() || !bBuildingTopDownViewActive)
	{
		return;
	}

	const bool bShowBuildingOverview =
		GetTopDownBuildingViewDistance() >=
			BuildingDetailViewDistance;
	RefreshTopDownBuildingLabels(bShowBuildingOverview);
	if (bShowBuildingOverview)
	{
		// At long range, complete silhouettes make buildings easier to read.
		for (const TWeakObjectPtr<UPrimitiveComponent>& Component :
			 TopDownHiddenRoofComponents)
		{
			if (Component.IsValid())
			{
				Component->SetVisibility(true, false);
			}
		}
		TopDownHiddenRoofComponents.Reset();
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
		if (!IsEbsBuildingActor(Actor))
		{
			continue;
		}

		const bool bRoofActor =
			Actor->GetClass()->GetName().Contains(
				TEXT("Roof"),
				ESearchCase::IgnoreCase);
		TInlineComponentArray<UPrimitiveComponent*> RoofComponents(Actor);
		for (UPrimitiveComponent* RoofComponent : RoofComponents)
		{
			if (!IsValid(RoofComponent))
			{
				continue;
			}
			const bool bRoofComponent =
				bRoofActor ||
				RoofComponent->GetName().Contains(
					TEXT("Roof"),
					ESearchCase::IgnoreCase);
			if (!bRoofComponent ||
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
	HideTopDownBuildingLabels();
	TopDownRoofRefreshAccumulator = 0.0f;
}

void ABotanicusPlayerController::RefreshTopDownBuildingLabels(
	bool bShowLabels)
{
	for (auto It = TopDownBuildingLabels.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		It.Value()->SetVisibility(bShowLabels);
	}
	if (!bShowLabels || !IsValid(BuildingCameraActor))
	{
		return;
	}

	for (TActorIterator<ABotanicusCatalogBuildingActor> It(GetWorld());
		 It;
		 ++It)
	{
		ABotanicusCatalogBuildingActor* Building = *It;
		if (!IsValid(Building))
		{
			continue;
		}

		UTextRenderComponent* Label = nullptr;
		if (const TWeakObjectPtr<UTextRenderComponent>* Found =
			TopDownBuildingLabels.Find(Building))
		{
			Label = Found->Get();
		}
		if (!IsValid(Label))
		{
			Label = NewObject<UTextRenderComponent>(
				Building,
				MakeUniqueObjectName(
					Building,
					UTextRenderComponent::StaticClass(),
					TEXT("BotanicusTopDownBuildingLabel")));
			if (!Label)
			{
				continue;
			}
			Label->RegisterComponent();
			Label->AttachToComponent(
				Building->GetRootComponent(),
				FAttachmentTransformRules::KeepWorldTransform);
			Label->SetMobility(EComponentMobility::Movable);
			Label->SetUsingAbsoluteRotation(true);
			Label->SetHorizontalAlignment(EHTA_Center);
			Label->SetVerticalAlignment(EVRTA_TextCenter);
			Label->SetWorldSize(180.0f);
			Label->SetTextRenderColor(FColor(255, 225, 80));
			Label->SetCollisionEnabled(
				ECollisionEnabled::NoCollision);
			Label->SetIsReplicated(false);
			Label->SetText(ResolveTopDownBuildingName(Building));
			TopDownBuildingLabels.Add(Building, Label);
		}

		FBox BuildingBounds(EForceInit::ForceInit);
		TInlineComponentArray<UPrimitiveComponent*> Components(Building);
		for (UPrimitiveComponent* Component : Components)
		{
			if (IsValid(Component) &&
				Component != Label &&
				!Component->IsA<UTextRenderComponent>())
			{
				BuildingBounds += Component->Bounds.GetBox();
			}
		}
		const FVector BoundsOrigin =
			BuildingBounds.IsValid
				? BuildingBounds.GetCenter()
				: Building->GetActorLocation();
		const FVector BoundsExtent =
			BuildingBounds.IsValid
				? BuildingBounds.GetExtent()
				: FVector(0.0f, 0.0f, 300.0f);
		const FVector LabelLocation =
			BoundsOrigin +
			FVector(
				0.0f,
				0.0f,
				BoundsExtent.Z + 350.0f);
		Label->SetWorldLocation(LabelLocation);
		Label->SetUsingAbsoluteRotation(true);
		const FVector FacingCamera =
			-BuildingCameraActor->GetActorForwardVector();
		const FVector CameraRight =
			BuildingCameraActor->GetActorRightVector();
		Label->SetWorldRotation(
			FRotationMatrix::MakeFromXY(
				FacingCamera,
				-CameraRight).Rotator());
		Label->SetVisibility(true);
	}
}

void ABotanicusPlayerController::HideTopDownBuildingLabels()
{
	for (const TPair<
			 TWeakObjectPtr<AActor>,
			 TWeakObjectPtr<UTextRenderComponent>>& Pair :
		 TopDownBuildingLabels)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->SetVisibility(false);
		}
	}
}

float ABotanicusPlayerController::
	GetTopDownBuildingViewDistance() const
{
	if (!IsValid(BuildingCameraActor))
	{
		return 0.0f;
	}
	if (LocalBuildingGroup.Num() > 0 &&
		bBuildingCameraOrbitInitialized)
	{
		return BuildingCameraOrbitDistance;
	}

	const FVector CameraLocation =
		BuildingCameraActor->GetActorLocation();
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
	return FMath::Abs(CameraLocation.Z - GroundHeight);
}

FText ABotanicusPlayerController::ResolveTopDownBuildingName(
	AActor* BuildingActor) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusBuildingCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusBuildingCatalogSubsystem>()
			: nullptr;
	if (Catalog && IsValid(BuildingActor))
	{
		for (const FBotanicusBuildingDefinition& Definition :
			 Catalog->GetAllBuildings())
		{
			UClass* BuildingClass =
				Definition.FallbackPrefabClass.LoadSynchronous();
			if (BuildingClass &&
				BuildingActor->IsA(BuildingClass))
			{
				return Definition.DisplayName;
			}
		}
	}
	return IsValid(BuildingActor)
		? FText::FromString(BuildingActor->GetClass()->GetName().Replace(
			TEXT("_C"), TEXT("")))
		: FText::GetEmpty();
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
		bBuildingCameraOrbitActive ||
		FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	FVector CameraForward =
		BuildingCameraActor->GetActorForwardVector();
	CameraForward.Z = 0.0f;
	if (!CameraForward.Normalize())
	{
		// The initial view looks straight down, so its projected forward
		// vector has no length. Preserve the historical world-forward axis.
		CameraForward = FVector::ForwardVector;
	}
	BuildingCameraActor->AddActorWorldOffset(
		CameraForward *
			(AxisValue * BuildingCameraPanSpeed * DeltaSeconds));
}

void ABotanicusPlayerController::MoveBuildingCameraRight(float AxisValue)
{
	if (!bBuildingTopDownViewActive ||
		!IsValid(BuildingCameraActor) ||
		bBuildingCameraOrbitActive ||
		FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	FVector CameraRight =
		BuildingCameraActor->GetActorRightVector();
	CameraRight.Z = 0.0f;
	if (!CameraRight.Normalize())
	{
		CameraRight = FVector::RightVector;
	}
	BuildingCameraActor->AddActorWorldOffset(
		CameraRight *
			(AxisValue * BuildingCameraPanSpeed * DeltaSeconds));
}

void ABotanicusPlayerController::ZoomBuildingCamera(float Direction)
{
	if (!bBuildingTopDownViewActive ||
		!IsValid(BuildingCameraActor) ||
		FMath::IsNearlyZero(Direction))
	{
		return;
	}

	if (LocalBuildingGroup.Num() > 0)
	{
		if (!bBuildingCameraOrbitInitialized)
		{
			InitializeBuildingCameraOrbitFromCurrentView();
		}
		const float MinimumDistance =
			FMath::Min(
				MinimumBuildingCameraHeight,
				MaximumBuildingCameraHeight);
		const float MaximumDistance =
			FMath::Max(
				MinimumBuildingCameraHeight,
				MaximumBuildingCameraHeight);
		BuildingCameraOrbitDistance = FMath::Clamp(
			BuildingCameraOrbitDistance -
				Direction * BuildingCameraZoomStep,
			MinimumDistance,
			MaximumDistance);
		ApplyBuildingCameraOrbit();
		return;
	}

	bBuildingCameraOrbitInitialized = false;
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

void ABotanicusPlayerController::BeginBuildingCameraOrbit()
{
	if (!bBuildingTopDownViewActive ||
		LocalBuildingGroup.Num() == 0 ||
		!IsValid(BuildingCameraActor) ||
		bBuildingCameraOrbitActive)
	{
		return;
	}

	GetMousePosition(
		BuildingOrbitSavedMouseX,
		BuildingOrbitSavedMouseY);
	InitializeBuildingCameraOrbitFromCurrentView();
	bBuildingCameraOrbitActive = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());
}

void ABotanicusPlayerController::EndBuildingCameraOrbit()
{
	if (!bBuildingCameraOrbitActive)
	{
		return;
	}

	bBuildingCameraOrbitActive = false;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetMouseLocation(
		FMath::RoundToInt(BuildingOrbitSavedMouseX),
		FMath::RoundToInt(BuildingOrbitSavedMouseY));
}

void ABotanicusPlayerController::UpdateBuildingCameraOrbit()
{
	if (!bBuildingCameraOrbitActive ||
		!IsValid(BuildingCameraActor) ||
		LocalBuildingGroup.Num() == 0)
	{
		return;
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	if (FMath::IsNearlyZero(MouseDeltaX) &&
		FMath::IsNearlyZero(MouseDeltaY))
	{
		return;
	}

	BuildingCameraOrbitYaw =
		FMath::UnwindDegrees(
			BuildingCameraOrbitYaw +
				MouseDeltaX * BuildingCameraOrbitSensitivity);
	BuildingCameraOrbitPitch = FMath::Clamp(
		BuildingCameraOrbitPitch +
			MouseDeltaY * BuildingCameraOrbitSensitivity,
		MinimumBuildingCameraOrbitPitch,
		MaximumBuildingCameraOrbitPitch);
	ApplyBuildingCameraOrbit();
}

void ABotanicusPlayerController::
	InitializeBuildingCameraOrbitFromCurrentView()
{
	if (!IsValid(BuildingCameraActor) ||
		LocalBuildingGroup.Num() == 0)
	{
		return;
	}

	FVector Offset =
		BuildingCameraActor->GetActorLocation() -
		LocalBuildingPivot;
	const float MinimumDistance =
		FMath::Min(
			MinimumBuildingCameraHeight,
			MaximumBuildingCameraHeight);
	BuildingCameraOrbitDistance =
		FMath::Max(MinimumDistance, Offset.Size());
	if (Offset.IsNearlyZero())
	{
		Offset = FVector(0.0f, 0.0f, BuildingCameraOrbitDistance);
	}
	BuildingCameraOrbitYaw =
		FMath::RadiansToDegrees(
			FMath::Atan2(Offset.Y, Offset.X));
	BuildingCameraOrbitPitch = FMath::Clamp(
		FMath::RadiansToDegrees(
			FMath::Asin(
				FMath::Clamp(
					Offset.Z / BuildingCameraOrbitDistance,
					-1.0f,
					1.0f))),
		MinimumBuildingCameraOrbitPitch,
		MaximumBuildingCameraOrbitPitch);
	bBuildingCameraOrbitInitialized = true;
}

void ABotanicusPlayerController::ApplyBuildingCameraOrbit()
{
	if (!bBuildingCameraOrbitInitialized ||
		!IsValid(BuildingCameraActor) ||
		LocalBuildingGroup.Num() == 0)
	{
		return;
	}

	const float YawRadians =
		FMath::DegreesToRadians(BuildingCameraOrbitYaw);
	const float PitchRadians =
		FMath::DegreesToRadians(BuildingCameraOrbitPitch);
	const float HorizontalDistance =
		BuildingCameraOrbitDistance * FMath::Cos(PitchRadians);
	const FVector Offset(
		HorizontalDistance * FMath::Cos(YawRadians),
		HorizontalDistance * FMath::Sin(YawRadians),
		BuildingCameraOrbitDistance * FMath::Sin(PitchRadians));
	const FVector CameraLocation =
		LocalBuildingPivot + Offset;
	BuildingCameraActor->SetActorLocationAndRotation(
		CameraLocation,
		(LocalBuildingPivot - CameraLocation).Rotation());
}

void ABotanicusPlayerController::BeginBuildingCameraFreeLook()
{
	if (!bBuildingTopDownViewActive ||
		LocalBuildingGroup.Num() > 0 ||
		!IsValid(BuildingCameraActor) ||
		bBuildingCameraFreeLookActive)
	{
		return;
	}

	GetMousePosition(
		BuildingOrbitSavedMouseX,
		BuildingOrbitSavedMouseY);
	bBuildingCameraFreeLookActive = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());
}

void ABotanicusPlayerController::EndBuildingCameraFreeLook()
{
	if (!bBuildingCameraFreeLookActive)
	{
		return;
	}

	bBuildingCameraFreeLookActive = false;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetMouseLocation(
		FMath::RoundToInt(BuildingOrbitSavedMouseX),
		FMath::RoundToInt(BuildingOrbitSavedMouseY));
}

void ABotanicusPlayerController::UpdateBuildingCameraFreeLook()
{
	if (!bBuildingCameraFreeLookActive ||
		!IsValid(BuildingCameraActor) ||
		LocalBuildingGroup.Num() > 0)
	{
		return;
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	if (FMath::IsNearlyZero(MouseDeltaX) &&
		FMath::IsNearlyZero(MouseDeltaY))
	{
		return;
	}

	FRotator CameraRotation =
		BuildingCameraActor->GetActorRotation();
	CameraRotation.Yaw = FMath::UnwindDegrees(
		CameraRotation.Yaw +
			MouseDeltaX * BuildingCameraOrbitSensitivity);
	CameraRotation.Pitch = FMath::Clamp(
		CameraRotation.Pitch +
			MouseDeltaY * BuildingCameraOrbitSensitivity,
		MinimumBuildingCameraFreeLookPitch,
		MaximumBuildingCameraFreeLookPitch);
	CameraRotation.Roll = 0.0f;
	BuildingCameraActor->SetActorRotation(CameraRotation);
}

void ABotanicusPlayerController::BeginPathPlacement()
{
	BeginPathPlacementInternal(EBotanicusPathType::Standard);
}

void ABotanicusPlayerController::BeginDoorEditing()
{
	if (!bBuildingTopDownViewActive ||
		!IsLocalPlayerController())
	{
		return;
	}
	if (bDoorEditSelectionActive ||
		bCommunicationDoorPlacementActive)
	{
		CancelDoorEditing();
		return;
	}

	CancelPathPlacement();
	CancelPathDeletion();
	CancelVisitorZonePlacement();
	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}
	bDoorEditSelectionActive = true;
	RefreshTopDownToolbar();
	ClientMessage(
		TEXT("PORTES : cliquez sur un bâtiment pour ajouter une porte, ou sur une porte existante pour la déplacer."));
}

void ABotanicusPlayerController::CancelDoorEditing()
{
	bDoorEditSelectionActive = false;
	if (bCommunicationDoorPlacementActive)
	{
		ServerCancelCommunicationDoor();
	}
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::BeginVisitorRoutePlacement()
{
	BeginPathPlacementInternal(EBotanicusPathType::VisitorRoute);
}

void ABotanicusPlayerController::BeginPathPlacementInternal(
	EBotanicusPathType PathType)
{
	if (!bBuildingTopDownViewActive || !IsLocalPlayerController())
	{
		return;
	}

	CancelPathDeletion();
	CancelVisitorZonePlacement();
	CancelDoorEditing();
	if (LocalBuildingGroup.Num() > 0)
	{
		CancelBuildingGroupMove();
	}

	if (bPathPlacementActive)
	{
		PendingPathPoints.Reset();
	}
	bPathPlacementActive = true;
	bPathStrokeActive = false;
	PendingPathType = PathType;

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
		PathPreviewActor->SetPreviewPath(
			PendingPathPoints,
			PendingPathType);
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

	ServerCreatePath(
		RequestedPoints,
		static_cast<uint8>(PendingPathType));
	CancelPathPlacement();
}

void ABotanicusPlayerController::CancelPathPlacement()
{
	bPathPlacementActive = false;
	bPathStrokeActive = false;
	PendingPathType = EBotanicusPathType::Standard;
	PendingPathPoints.Reset();
	if (IsValid(PathPreviewActor))
	{
		PathPreviewActor->Destroy();
		PathPreviewActor = nullptr;
	}
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::BeginVisitorParkingPlacement()
{
	BeginVisitorZonePlacement(
		static_cast<int32>(
			EBotanicusVisitorZoneType::Parking));
}

void ABotanicusPlayerController::BeginVisitorSalesAreaPlacement()
{
	BeginVisitorZonePlacement(
		static_cast<int32>(
			EBotanicusVisitorZoneType::SalesArea));
}

void ABotanicusPlayerController::BeginVisitorCheckoutPlacement()
{
	BeginVisitorZonePlacement(
		static_cast<int32>(
			EBotanicusVisitorZoneType::Checkout));
}

void ABotanicusPlayerController::BeginRefundZonePlacement()
{
	BeginVisitorZonePlacement(3);
}

void ABotanicusPlayerController::BeginDeliveryZonePlacement()
{
	BeginVisitorZonePlacement(4);
}

void ABotanicusPlayerController::BeginVisitorZonePlacement(
	int32 ZoneType)
{
	if (!bBuildingTopDownViewActive ||
		!IsLocalPlayerController() ||
		ZoneType < 0 ||
		ZoneType > 4)
	{
		return;
	}

	CancelPathPlacement();
	CancelPathDeletion();
	CancelDoorEditing();
	PendingVisitorZoneType = ZoneType;
	RefreshTopDownToolbar();
	if (ZoneType == 3)
	{
		ClientMessage(
			TEXT(
				"Placement remboursement : cliquez sur un sol, clic droit pour annuler."));
	}
	else if (ZoneType == 4)
	{
		ClientMessage(
			TEXT(
				"Placement livraison : cliquez sur un sol, clic droit pour annuler."));
	}
	else
	{
		ClientMessage(
			TEXT(
				"Placement zone PNJ : cliquez sur un sol, clic droit pour annuler."));
	}
}

void ABotanicusPlayerController::CancelVisitorZonePlacement()
{
	if (PendingVisitorZoneType == INDEX_NONE)
	{
		return;
	}
	PendingVisitorZoneType = INDEX_NONE;
	RefreshTopDownToolbar();
}

void ABotanicusPlayerController::PlaceVisitorZoneAtCursor()
{
	if (PendingVisitorZoneType == INDEX_NONE)
	{
		return;
	}

	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit))
	{
		return;
	}

	const FVector_NetQuantize10 RequestedLocation(
		CursorHit.ImpactPoint);
	if (PendingVisitorZoneType == 3)
	{
		ServerCreateRefundZone(RequestedLocation);
	}
	else if (PendingVisitorZoneType == 4)
	{
		ServerCreateDeliveryZone(RequestedLocation);
	}
	else
	{
		ServerCreateVisitorZone(
			RequestedLocation,
			static_cast<uint8>(PendingVisitorZoneType));
	}
	CancelVisitorZonePlacement();
}

void ABotanicusPlayerController::BeginPathDeletion()
{
	if (!bBuildingTopDownViewActive || !IsLocalPlayerController())
	{
		return;
	}
	CancelDoorEditing();

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

void ABotanicusPlayerController::ToggleBuildingCatalog()
{
	if (!bBuildingTopDownViewActive || !IsLocalPlayerController())
	{
		return;
	}

	InitializeOrderCatalogWidget();
	if (!OrderCatalogWidget)
	{
		return;
	}
	OrderCatalogWidget->ShowBuildingTab();
	OrderCatalogWidget->SetVisibility(ESlateVisibility::Visible);
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	bAzertyForwardPressed = false;
	bAzertyBackwardPressed = false;
	bAzertyLeftPressed = false;
	bAzertyRightPressed = false;
	OrderCatalogWidget->Refresh();
}

void ABotanicusPlayerController::PurchaseCatalogBuilding(
	FName BuildingKey)
{
	if (!IsLocalPlayerController() ||
		BuildingKey.IsNone())
	{
		return;
	}
	if (!bBuildingTopDownViewActive &&
		bOrderCatalogOpenedFromComputer)
	{
		ToggleOrderCatalog();
		EnterBuildingTopDownView();
	}
	if (!bBuildingTopDownViewActive)
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
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (OrderCatalogWidget)
	{
		OrderCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Client requested building purchase %s."),
		*BuildingKey.ToString());
	ServerPurchaseCatalogBuilding(BuildingKey);
}

void ABotanicusPlayerController::ToggleOrderCatalog()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	InitializeOrderCatalogWidget();
	if (!OrderCatalogWidget)
	{
		return;
	}

	const bool bOpening =
		OrderCatalogWidget->GetVisibility() !=
			ESlateVisibility::Visible;
	if (!bOpening && bOrderCatalogOpenedFromComputer)
	{
		CloseOrderCatalogFromComputer();
		return;
	}
	if (bOpening && !bBuildingTopDownViewActive)
	{
		return;
	}
	OrderCatalogWidget->SetVisibility(
		bOpening
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (bOpening)
	{
		if (BuildingCatalogWidget)
		{
			BuildingCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		bAzertyForwardPressed = false;
		bAzertyBackwardPressed = false;
		bAzertyLeftPressed = false;
		bAzertyRightPressed = false;
		OrderCatalogWidget->Refresh();
	}
}

void ABotanicusPlayerController::ToggleDevelopmentPanel()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeDevelopmentPanelWidget();
	if (!DevelopmentPanelWidget)
	{
		return;
	}

	const bool bOpening =
		DevelopmentPanelWidget->GetVisibility() !=
			ESlateVisibility::Visible;
	DevelopmentPanelWidget->SetVisibility(
		bOpening
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (bOpening)
	{
		if (OrderCatalogWidget)
		{
			OrderCatalogWidget->SetVisibility(
				ESlateVisibility::Collapsed);
		}
		if (BuildingCatalogWidget)
		{
			BuildingCatalogWidget->SetVisibility(
				ESlateVisibility::Collapsed);
		}
		bOrderCatalogOpenedFromComputer = false;
		bAzertyForwardPressed = false;
		bAzertyBackwardPressed = false;
		bAzertyLeftPressed = false;
		bAzertyRightPressed = false;
		DevelopmentPanelWidget->Refresh();
		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(
			EMouseLockMode::DoNotLock);
		InputMode.SetWidgetToFocus(
			DevelopmentPanelWidget->TakeWidget());
		SetInputMode(InputMode);
		DevelopmentPanelWidget->SetKeyboardFocus();
	}
	else if (bBuildingTopDownViewActive)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(
			EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
}

void ABotanicusPlayerController::
ClientOpenOrderCatalogFromComputer_Implementation(
	ABotanicusComputerActor* Computer)
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	InitializeOrderCatalogWidget();
	if (!OrderCatalogWidget)
	{
		return;
	}

	bOrderCatalogOpenedFromComputer = true;
	ActiveComputerView = Computer;
	OrderCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	bAzertyForwardPressed = false;
	bAzertyBackwardPressed = false;
	bAzertyLeftPressed = false;
	bAzertyRightPressed = false;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	SetInteractionTargetHighlighted(
		LocalInteractionHighlightActor.Get(), false);
	LocalInteractionHighlightActor.Reset();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->ClearTarget();
	}

	GetWorldTimerManager().ClearTimer(ComputerViewTransitionTimer);
	if (IsValid(Computer) && ComputerCameraEnterBlendTime > KINDA_SMALL_NUMBER)
	{
		SetViewTargetWithBlend(
			Computer,
			ComputerCameraEnterBlendTime,
			EViewTargetBlendFunction::VTBlend_Cubic);
		GetWorldTimerManager().SetTimer(
			ComputerViewTransitionTimer,
			this,
			&ABotanicusPlayerController::FinishOpenOrderCatalogFromComputer,
			ComputerCameraEnterBlendTime,
			false);
	}
	else
	{
		FinishOpenOrderCatalogFromComputer();
	}
}

void ABotanicusPlayerController::FinishOpenOrderCatalogFromComputer()
{
	if (!IsLocalPlayerController() ||
		!bOrderCatalogOpenedFromComputer ||
		!OrderCatalogWidget)
	{
		return;
	}

	OrderCatalogWidget->SetRenderOpacity(1.0f);
	OrderCatalogWidget->SetVisibility(ESlateVisibility::Visible);
	OrderCatalogWidget->Refresh();
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(OrderCatalogWidget->TakeWidget());
	SetInputMode(InputMode);
	OrderCatalogWidget->SetKeyboardFocus();
}

void ABotanicusPlayerController::CloseOrderCatalogFromComputer()
{
	GetWorldTimerManager().ClearTimer(ComputerViewTransitionTimer);
	if (OrderCatalogWidget)
	{
		OrderCatalogWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	bOrderCatalogOpenedFromComputer = false;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ComputerCameraExitBlendTime > KINDA_SMALL_NUMBER)
	{
		SetViewTargetWithBlend(
			ControlledPawn,
			ComputerCameraExitBlendTime,
			EViewTargetBlendFunction::VTBlend_Cubic);
		GetWorldTimerManager().SetTimer(
			ComputerViewTransitionTimer,
			this,
			&ABotanicusPlayerController::FinishCloseOrderCatalogFromComputer,
			ComputerCameraExitBlendTime,
			false);
	}
	else
	{
		FinishCloseOrderCatalogFromComputer();
	}
}

void ABotanicusPlayerController::FinishCloseOrderCatalogFromComputer()
{
	ActiveComputerView.Reset();
	if (bBuildingTopDownViewActive)
	{
		return;
	}
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ABotanicusPlayerController::
	ClientOpenPreparationWorkbenchUpgrade_Implementation(
		ABotanicusPreparationWorkbenchActor* Workbench)
{
	if (!IsLocalPlayerController() || !IsValid(Workbench))
	{
		return;
	}

	InitializeWorkbenchUpgradeWidget();
	if (!WorkbenchUpgradeWidget)
	{
		return;
	}

	if (OrderCatalogWidget)
	{
		OrderCatalogWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	if (DevelopmentPanelWidget)
	{
		DevelopmentPanelWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	bOrderCatalogOpenedFromComputer = false;
	bAzertyForwardPressed = false;
	bAzertyBackwardPressed = false;
	bAzertyLeftPressed = false;
	bAzertyRightPressed = false;

	WorkbenchUpgradeWidget->InitializeWithWorkbench(
		this,
		Workbench);
	WorkbenchUpgradeWidget->SetVisibility(
		ESlateVisibility::Visible);
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(
		WorkbenchUpgradeWidget->TakeWidget());
	SetInputMode(InputMode);
	WorkbenchUpgradeWidget->SetKeyboardFocus();
}

void ABotanicusPlayerController::
ClosePreparationWorkbenchUpgrade()
{
	if (!IsLocalPlayerController() || !WorkbenchUpgradeWidget)
	{
		return;
	}

	WorkbenchUpgradeWidget->SetVisibility(
		ESlateVisibility::Collapsed);
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ABotanicusPlayerController::
RequestPreparationWorkbenchLevel(
	ABotanicusPreparationWorkbenchActor* Workbench,
	int32 TargetLevel)
{
	if (IsLocalPlayerController() &&
		IsValid(Workbench) &&
		TargetLevel >= 1 &&
		TargetLevel <= 5)
	{
		ServerSetPreparationWorkbenchLevel(
			Workbench,
			TargetLevel);
	}
}

void ABotanicusPlayerController::PlaceCatalogOrder(FName ItemKey)
{
	if (IsLocalPlayerController() &&
		(bBuildingTopDownViewActive ||
		 bOrderCatalogOpenedFromComputer) &&
		!ItemKey.IsNone())
	{
		ServerPlaceCatalogOrder(ItemKey);
	}
}

int32 ABotanicusPlayerController::GetAvailableFunds() const
{
	const ABotanicusGameState* GameState =
		GetSharedGameState(this);
	return GameState
		? GameState->GetSharedFunds()
		: FMath::Max(0, StartingFunds);
}

int32 ABotanicusPlayerController::GetMainShopLevel() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetMainShopLevel() : 1;
}

int32 ABotanicusPlayerController::GetSelfCheckoutLimit() const
{
	const int32 ShopLevel = GetMainShopLevel();
	return ShopLevel < 3
		? 0
		: FMath::Min(
			ABotanicusCashRegisterActor::SelfCheckoutSlotCount,
			2 + (ShopLevel - 3));
}

int32 ABotanicusPlayerController::
	GetSelfCheckoutOwnedOrOrderedCount() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ABotanicusSelfCheckoutActor> CheckoutIt(World);
		 CheckoutIt;
		 ++CheckoutIt)
	{
		if (!CheckoutIt->ActorHasTag(
				TEXT("BotanicusPlacementPreview")))
		{
			++Count;
		}
	}
	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(World);
		 ParcelIt;
		 ++ParcelIt)
	{
		if (!ParcelIt->IsOpened() &&
			ParcelIt->GetItemKey() == TEXT("SelfCheckout"))
		{
			Count += FMath::Max(1, ParcelIt->GetQuantity());
		}
	}
	for (TActorIterator<ABotanicusPlayerController> ControllerIt(World);
		 ControllerIt;
		 ++ControllerIt)
	{
		for (const FBotanicusPendingOrder& Order :
			 ControllerIt->GetPendingOrders())
		{
			if (Order.ItemKey == TEXT("SelfCheckout"))
			{
				Count += FMath::Max(1, Order.Quantity);
			}
		}
	}
	return Count;
}

bool ABotanicusPlayerController::CanOrderCatalogItem(
	FName ItemKey) const
{
	if (ItemKey != TEXT("SelfCheckout"))
	{
		return true;
	}
	const int32 Limit = GetSelfCheckoutLimit();
	return Limit > 0 &&
		GetSelfCheckoutOwnedOrOrderedCount() < Limit;
}

bool ABotanicusPlayerController::IsMainShopOpen() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->IsMainShopOpen() : true;
}

void ABotanicusPlayerController::ToggleMainShopOpen()
{
	const bool bNewOpenState = !IsMainShopOpen();
	if (HasAuthority())
	{
		ServerSetMainShopOpen_Implementation(bNewOpenState);
	}
	else
	{
		ServerSetMainShopOpen(bNewOpenState);
	}
}

float ABotanicusPlayerController::GetDevelopmentTimeScale() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetDevelopmentTimeScale() : 1.0f;
}

void ABotanicusPlayerController::CycleDevelopmentTimeScale()
{
	const float CurrentScale = GetDevelopmentTimeScale();
	const float NewScale =
		CurrentScale < 4.0f
			? 5.0f
			: CurrentScale < 10.0f
				? 15.0f
				: 1.0f;
	if (HasAuthority())
	{
		ServerSetDevelopmentTimeScale_Implementation(NewScale);
	}
	else
	{
		ServerSetDevelopmentTimeScale(NewScale);
	}
}

void ABotanicusPlayerController::
	AdjustMainShopLevelForDevelopment(int32 Delta)
{
	const int32 Direction = FMath::Clamp(Delta, -1, 1);
	if (Direction == 0)
	{
		return;
	}
	if (HasAuthority())
	{
		ServerAdjustMainShopLevelForDevelopment_Implementation(
			Direction);
	}
	else
	{
		ServerAdjustMainShopLevelForDevelopment(Direction);
	}
}

int32 ABotanicusPlayerController::GetMainShopVisitorCapacity() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetMainShopVisitorCapacity() : 4;
}

int32 ABotanicusPlayerController::GetTargetVisitorPopulation() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetTargetVisitorPopulation() : 16;
}

int32 ABotanicusPlayerController::GetMainShopUpgradeCost() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetMainShopUpgradeCost() : 1500;
}

int32 ABotanicusPlayerController::GetMainShopRequiredPlantSales() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState
		? GameState->GetRequiredPlantSalesForUpgrade()
		: 3;
}

int32 ABotanicusPlayerController::GetMainShopRequiredCatalogOrders() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState
		? GameState->GetRequiredCatalogOrdersForUpgrade()
		: 5;
}

int32 ABotanicusPlayerController::GetTotalPlantsSold() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetTotalPlantsSold() : 0;
}

int32 ABotanicusPlayerController::GetTotalCatalogOrders() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState ? GameState->GetTotalCatalogOrders() : 0;
}

bool ABotanicusPlayerController::CanUpgradeMainShop() const
{
	const ABotanicusGameState* GameState = GetSharedGameState(this);
	return GameState &&
		GameState->AreMainShopUpgradeTasksComplete() &&
		GameState->GetSharedFunds() >=
			GameState->GetMainShopUpgradeCost();
}

void ABotanicusPlayerController::UpgradeMainShop()
{
	if (IsLocalPlayerController() &&
		bBuildingTopDownViewActive)
	{
		ServerUpgradeMainShop();
	}
}

int32 ABotanicusPlayerController::
	GetPreparationWorkbenchLevel() const
{
	const ABotanicusPreparationWorkbenchActor* Workbench =
		FindPrimaryPreparationWorkbench(this);
	return Workbench ? Workbench->GetWorkbenchLevel() : 1;
}

int32 ABotanicusPlayerController::
	GetPreparationWorkbenchUpgradeCost() const
{
	const ABotanicusPreparationWorkbenchActor* Workbench =
		FindPrimaryPreparationWorkbench(this);
	return Workbench ? Workbench->GetUpgradeCost() : 0;
}

bool ABotanicusPlayerController::
	CanUpgradePreparationWorkbench() const
{
	const ABotanicusPreparationWorkbenchActor* Workbench =
		FindPrimaryPreparationWorkbench(this);
	return Workbench &&
		Workbench->CanUpgrade() &&
		GetAvailableFunds() >= Workbench->GetUpgradeCost();
}

void ABotanicusPlayerController::UpgradePreparationWorkbench()
{
	if (IsLocalPlayerController())
	{
		ServerUpgradePreparationWorkbench();
	}
}

void ABotanicusPlayerController::AddTestCredits()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerAddTestCredits_Implementation();
	}
	else
	{
		ServerAddTestCredits();
	}
}

void ABotanicusPlayerController::RefreshOrderCatalog()
{
	if (OrderCatalogWidget)
	{
		OrderCatalogWidget->Refresh();
	}
}

void ABotanicusPlayerController::OnRep_OrderState()
{
	RefreshOrderCatalog();
	if (BuildingCatalogWidget)
	{
		BuildingCatalogWidget->Refresh();
	}
}

void ABotanicusPlayerController::RestoreCatalogOrderState(
	int32 RestoredFunds,
	const TArray<FBotanicusPendingOrder>& RestoredOrders,
	int32 RestoredBuildingProgressionLevel)
{
	if (!HasAuthority() || bCatalogOrderStateRestored || !GetWorld())
	{
		return;
	}
	bCatalogOrderStateRestored = true;

	for (TPair<FGuid, FTimerHandle>& Timer : PendingOrderTimers)
	{
		GetWorldTimerManager().ClearTimer(Timer.Value);
	}
	PendingOrderTimers.Reset();
	PendingOrders.Reset();
	BuildingProgressionLevel =
		FMath::Max(1, RestoredBuildingProgressionLevel);

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float ServerTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds();
	for (const FBotanicusPendingOrder& SavedOrder : RestoredOrders)
	{
		const FBotanicusItemDefinition* Definition =
			FindItemDefinition(this, SavedOrder.ItemKey);
		if (!Definition || SavedOrder.ItemKey.IsNone())
		{
			AddSharedFunds(
				this,
				FMath::Max(0, SavedOrder.ChargedPrice));
			continue;
		}

		FBotanicusPendingOrder& RestoredOrder =
			PendingOrders.AddDefaulted_GetRef();
		RestoredOrder = SavedOrder;
		if (!RestoredOrder.OrderId.IsValid())
		{
			RestoredOrder.OrderId = FGuid::NewGuid();
		}
		RestoredOrder.DisplayName =
			Definition->DisplayName.IsEmpty()
				? FText::FromName(RestoredOrder.ItemKey)
				: Definition->DisplayName;
		RestoredOrder.Quantity =
			FMath::Max(1, RestoredOrder.Quantity);
		RestoredOrder.ChargedPrice =
			FMath::Max(0, RestoredOrder.ChargedPrice);
		const float RemainingSeconds =
			FMath::Max(0.1f, SavedOrder.DeliveryServerTime);
		RestoredOrder.DeliveryServerTime =
			ServerTime + RemainingSeconds;

		FTimerDelegate DeliveryDelegate;
		DeliveryDelegate.BindUObject(
			this,
			&ABotanicusPlayerController::CompleteCatalogOrder,
			RestoredOrder.OrderId);
		FTimerHandle& Timer =
			PendingOrderTimers.Add(RestoredOrder.OrderId);
		GetWorldTimerManager().SetTimer(
			Timer,
			DeliveryDelegate,
			RemainingSeconds,
			false);
	}

	ForceNetUpdate();
	OnRep_OrderState();
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Restored catalogue economy for %s: %d credits, level %d, %d pending order(s)."),
		PlayerState ? *PlayerState->GetPlayerName() : TEXT("UnknownPlayer"),
		GetAvailableFunds(),
		BuildingProgressionLevel,
		PendingOrders.Num());
}

void ABotanicusPlayerController::CreditPlantSale(
	FName PlantItemKey,
	int32 SalePrice)
{
	if (!HasAuthority() || SalePrice <= 0)
	{
		return;
	}
	AddSharedFunds(this, SalePrice);
	if (ABotanicusGameState* GameState = GetSharedGameState(this))
	{
		GameState->RecordPlantSale(SalePrice);
	}
	OnRep_OrderState();
	ClientMessage(
		FString::Printf(
			TEXT("%s vendu : +%d credits."),
			*PlantItemKey.ToString(),
			SalePrice));
	if (ABotanicusGameMode* GameMode =
		GetWorld()
			? GetWorld()->GetAuthGameMode<ABotanicusGameMode>()
			: nullptr)
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::CancelPendingBuildingPurchaseForLogout()
{
	if (!HasAuthority() || !bServerBuildingPurchasePlacement)
	{
		return;
	}

	for (AActor* Actor : ServerBuildingGroup)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	RefundPendingBuildingPurchase();
	ClearServerBuildingGroupMove();
}

bool ABotanicusPlayerController::TryHandleNearbyLargeEquipment()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	ABotanicusLargeEquipmentActor* NearestEquipment = nullptr;
	const float FurnitureSelectionDistance =
		bFurnitureMoveModeActive ? 650.0f : 450.0f;
	float BestDistanceSquared =
		FMath::Square(FurnitureSelectionDistance);
	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(
			 GetWorld());
		 EquipmentIt;
		 ++EquipmentIt)
	{
		// Furniture can only be selected as a whole while furniture mode
		// is active. Outside that mode, E must remain available for an
		// item stored on or inside the furniture.
		if (!bFurnitureMoveModeActive &&
			IsFurnitureActor(*EquipmentIt))
		{
			continue;
		}

		if (EquipmentIt->GetCarrier() == GetPawn() ||
			EquipmentIt->GetHelper() == GetPawn())
		{
			if (!EquipmentIt->RequiresTwoPlayers() &&
				!EquipmentIt->IsInPlacementMode())
			{
				ServerBeginLargeEquipmentPlacement(*EquipmentIt);
				BeginLargeEquipmentPlacement(*EquipmentIt);
			}
			return true;
		}

		const bool bCanJoinCooperativeCarry =
			EquipmentIt->RequiresTwoPlayers() &&
			EquipmentIt->IsWaitingForHelper();
		const ABotanicusStorageShelfActor* StorageShelf =
			Cast<ABotanicusStorageShelfActor>(*EquipmentIt);
		if (StorageShelf &&
			StorageShelf->HasStoredItems() &&
			!bFurnitureMoveModeActive)
		{
			continue;
		}
		const ABotanicusPreparationWorkbenchActor* Workbench =
			Cast<ABotanicusPreparationWorkbenchActor>(*EquipmentIt);
		if (Workbench &&
			Workbench->HasPreparedPots() &&
			!bFurnitureMoveModeActive)
		{
			continue;
		}
		if (IsValid(EquipmentIt->GetCarrier()) &&
			!bCanJoinCooperativeCarry)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			EquipmentIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(
				*EquipmentIt,
				FurnitureSelectionDistance))
		{
			BestDistanceSquared = DistanceSquared;
			NearestEquipment = *EquipmentIt;
		}
	}

	if (!NearestEquipment)
	{
		return false;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, NearestEquipment->GetItemKey());
	if (Definition &&
		(Definition->WeightClass ==
			 EBotanicusItemWeightClass::OnePlayerCarry ||
		 Definition->WeightClass ==
			 EBotanicusItemWeightClass::TwoPlayerCarry))
	{
		if (bFurnitureMoveModeActive &&
			IsFurnitureActor(NearestEquipment) &&
			Definition->WeightClass ==
				EBotanicusItemWeightClass::OnePlayerCarry)
		{
			if (ABotanicusStorageShelfActor* Shelf =
					Cast<ABotanicusStorageShelfActor>(
						NearestEquipment))
			{
				Shelf->SetMoveContentsWithFurniture(true);
			}
			if (ABotanicusPreparationWorkbenchActor* Workbench =
					Cast<ABotanicusPreparationWorkbenchActor>(
						NearestEquipment))
			{
				Workbench->SetMoveContentsWithFurniture(true);
			}
			if (ABotanicusWorkSurfaceActor* WorkSurface =
					Cast<ABotanicusWorkSurfaceActor>(
						NearestEquipment))
			{
				WorkSurface->SetMoveContentsWithFurniture(true);
			}
			ServerToggleCarryLargeEquipment(NearestEquipment);
		}
		else
		{
			BeginEquipmentCarryCharge(NearestEquipment);
		}
	}
	else
	{
		ClientMessage(
			TEXT("Cet objet ne peut pas etre porte."));
	}
	return true;
}

bool ABotanicusPlayerController::TryOpenNearbyWorkbenchUpgrade()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusWorkbenchUpgradeInput),
		false,
		GetPawn());
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() *
				(MaximumWorldInteractionDistance + 200.0f),
			ECC_Visibility,
			QueryParams))
	{
		return false;
	}

	ABotanicusPreparationWorkbenchActor* Workbench =
		Cast<ABotanicusPreparationWorkbenchActor>(
			Hit.GetActor());
	if (!Workbench ||
		!Workbench->IsUpgradeTerminalTargeted(GetPawn()))
	{
		return false;
	}

	ServerUseWorkbenchUpgradeTerminal(Workbench);
	return true;
}

void ABotanicusPlayerController::BeginEquipmentCarryCharge(
	ABotanicusLargeEquipmentActor* Equipment)
{
	if (!IsLocalPlayerController() ||
		!IsValid(Equipment) ||
		IsValid(LocalHeldHeavyEquipment))
	{
		return;
	}

	if (ABotanicusStorageShelfActor* Shelf =
			Cast<ABotanicusStorageShelfActor>(Equipment))
	{
		Shelf->SetMoveContentsWithFurniture(
			bFurnitureMoveModeActive);
	}
	if (ABotanicusPreparationWorkbenchActor* Workbench =
			Cast<ABotanicusPreparationWorkbenchActor>(Equipment))
	{
		Workbench->SetMoveContentsWithFurniture(
			bFurnitureMoveModeActive);
	}
	if (ABotanicusWorkSurfaceActor* WorkSurface =
			Cast<ABotanicusWorkSurfaceActor>(Equipment))
	{
		WorkSurface->SetMoveContentsWithFurniture(
			bFurnitureMoveModeActive);
	}

	LocalHeldHeavyEquipment = Equipment;
	EquipmentCarryChargeElapsed = 0.0f;
	bEquipmentCarryHoldActivated = false;
	bEquipmentCarryKeyHeld = true;
	InitializeInteractionTargetWidget();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->BeginLocalHoldProgress(
			GetMoveHoldDurationForActor(
				Equipment,
				EquipmentLiftHoldDuration));
	}

	if (!CarryProgressWidget)
	{
		CarryProgressWidget =
			CreateWidget<UBotanicusCarryProgressWidget>(
				this,
				UBotanicusCarryProgressWidget::StaticClass());
		if (CarryProgressWidget)
		{
			CarryProgressWidget->AddToPlayerScreen(50);
			CarryProgressWidget->SetAlignmentInViewport(
				FVector2D(0.5f, 0.5f));
			CarryProgressWidget->SetDesiredSizeInViewport(
				FVector2D(68.0f, 68.0f));
		}
	}

	if (CarryProgressWidget)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		GetViewportSize(ViewportWidth, ViewportHeight);
		CarryProgressWidget->SetPositionInViewport(
			FVector2D(
				static_cast<float>(ViewportWidth) * 0.5f,
				static_cast<float>(ViewportHeight) * 0.5f + 85.0f),
			true);
		CarryProgressWidget->SetCarryProgress(0.0f);
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}
}

void ABotanicusPlayerController::UpdateEquipmentCarryCharge(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		!IsValid(LocalHeldHeavyEquipment) ||
		bEquipmentCarryHoldActivated)
	{
		return;
	}

	if (!bEquipmentCarryKeyHeld ||
		!IsLookingAtWorldItem(LocalHeldHeavyEquipment, 500.0f))
	{
		CancelEquipmentCarryCharge(false);
		return;
	}

	EquipmentCarryChargeElapsed += DeltaTime;
	const float RequiredHoldDuration =
		GetMoveHoldDurationForActor(
			LocalHeldHeavyEquipment,
			EquipmentLiftHoldDuration);
	const float ChargeProgress = FMath::Clamp(
		EquipmentCarryChargeElapsed /
			FMath::Max(RequiredHoldDuration, 0.1f),
		0.0f,
		1.0f);
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetCarryProgress(ChargeProgress);
	}
	// The interaction card belongs to this local controller. Update it
	// directly instead of relying on the legacy carry widget's visibility,
	// which is not guaranteed to match on remote multiplayer clients.
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(ChargeProgress);
	}

	if (ChargeProgress < 1.0f)
	{
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(
			this,
			LocalHeldHeavyEquipment->GetItemKey());
	if (!Definition)
	{
		CancelEquipmentCarryCharge(false);
		return;
	}

	bEquipmentCarryHoldActivated = true;
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}

	if (Definition->WeightClass ==
		EBotanicusItemWeightClass::TwoPlayerCarry)
	{
		ServerSetHeavyEquipmentHold(
			LocalHeldHeavyEquipment,
			true);
	}
	else if (Definition->WeightClass ==
			 EBotanicusItemWeightClass::OnePlayerCarry)
	{
		ServerToggleCarryLargeEquipment(
			LocalHeldHeavyEquipment);
	}
	else
	{
		CancelEquipmentCarryCharge(false);
	}
}

void ABotanicusPlayerController::CancelEquipmentCarryCharge(
	bool bNotifyServer)
{
	ABotanicusLargeEquipmentActor* Equipment =
		LocalHeldHeavyEquipment;
	if (!bEquipmentCarryHoldActivated)
	{
		if (ABotanicusStorageShelfActor* Shelf =
				Cast<ABotanicusStorageShelfActor>(Equipment))
		{
			Shelf->SetMoveContentsWithFurniture(false);
		}
		if (ABotanicusPreparationWorkbenchActor* Workbench =
				Cast<ABotanicusPreparationWorkbenchActor>(Equipment))
		{
			Workbench->SetMoveContentsWithFurniture(false);
		}
		if (ABotanicusWorkSurfaceActor* WorkSurface =
				Cast<ABotanicusWorkSurfaceActor>(Equipment))
		{
			WorkSurface->SetMoveContentsWithFurniture(false);
		}
	}
	if (bNotifyServer &&
		bEquipmentCarryHoldActivated &&
		IsValid(Equipment))
	{
		const FBotanicusItemDefinition* Definition =
			FindItemDefinition(this, Equipment->GetItemKey());
		if (Definition &&
			Definition->WeightClass ==
				EBotanicusItemWeightClass::TwoPlayerCarry)
		{
			ServerSetHeavyEquipmentHold(Equipment, false);
		}
		else
		{
			ServerCancelLargeEquipmentPlacement(Equipment);
		}
	}

	if (LocalLargeEquipmentPlacement == Equipment)
	{
		LocalLargeEquipmentPlacement = nullptr;
		bLocalLargeEquipmentPlacementValid = false;
	}
	LocalHeldHeavyEquipment = nullptr;
	EquipmentCarryChargeElapsed = 0.0f;
	bEquipmentCarryHoldActivated = false;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}
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
	Equipment->SetLocalPlacementPreview(
		Equipment->GetActorTransform(),
		false);
	ClientMessage(
		TEXT(
			"Placement : regardez vers vos pieds pour rapprocher ou devant vous pour eloigner, molette pour tourner, clic gauche pour poser."));
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

	if (!IsValid(LocalLargeEquipmentPlacement->GetCarrier()) &&
		!LocalLargeEquipmentPlacement->IsInPlacementMode())
	{
		LocalLargeEquipmentPlacement = nullptr;
		bLocalLargeEquipmentPlacementValid = false;
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

	const FVector RequestedLocation =
		GetViewDirectedGroundPlacementLocation(
			MinimumEquipmentPlacementDistance,
			MaximumEquipmentPlacementDistance);
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
		const FBotanicusItemDefinition* Definition =
			FindItemDefinition(
				this,
				LocalLargeEquipmentPlacement->GetItemKey());
		const float RotationStep =
			IsInputKeyDown(EKeys::LeftShift) ||
			IsInputKeyDown(EKeys::RightShift)
				? (Definition
					   ? Definition->FineRotationStep
					   : FineEquipmentRotationStep)
				: (Definition
					   ? Definition->RotationStep
					   : EquipmentRotationStep);
		LargeEquipmentPlacementYaw =
			FMath::UnwindDegrees(
				LargeEquipmentPlacementYaw +
				Direction * RotationStep);
		LargeEquipmentPreviewUpdateAccumulator = 1.0f;
	}
}

FVector ABotanicusPlayerController::
	GetViewDirectedGroundPlacementLocation(
		float MinimumDistance,
		float MaximumDistance) const
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return FVector::ZeroVector;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	FVector FlatForward(ViewDirection.X, ViewDirection.Y, 0.0f);
	if (!FlatForward.Normalize())
	{
		FlatForward = ControlledPawn->GetActorForwardVector();
		FlatForward.Z = 0.0f;
		FlatForward.Normalize();
	}

	const float SafeMinimum = FMath::Max(0.0f, MinimumDistance);
	const float SafeMaximum =
		FMath::Max(SafeMinimum, MaximumDistance - 5.0f);
	float GroundDistance = SafeMaximum;
	if (ViewDirection.Z < -KINDA_SMALL_NUMBER)
	{
		const ABotanicusCharacter* BotanicusCharacter =
			Cast<ABotanicusCharacter>(ControlledPawn);
		const UCapsuleComponent* Capsule =
			BotanicusCharacter
				? BotanicusCharacter->GetCapsuleComponent()
				: nullptr;
		const float GroundReferenceZ =
			Capsule
				? BotanicusCharacter->GetActorLocation().Z -
					Capsule->GetScaledCapsuleHalfHeight()
				: ControlledPawn->GetActorLocation().Z;
		const float EyeHeightAboveGround = FMath::Max(
			0.0f,
			ViewLocation.Z - GroundReferenceZ);
		const float RayDistanceToGround =
			EyeHeightAboveGround / -ViewDirection.Z;
		const float HorizontalRayScale = FVector2D(
			ViewDirection.X,
			ViewDirection.Y).Size();
		GroundDistance = FMath::Clamp(
			RayDistanceToGround * HorizontalRayScale,
			SafeMinimum,
			SafeMaximum);
	}

	return ControlledPawn->GetActorLocation() +
		FlatForward * GroundDistance;
}

void ABotanicusPlayerController::DrawLargeEquipmentAlignmentGuides(
	const FTransform& PlacementTransform) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(LocalLargeEquipmentPlacement))
	{
		return;
	}

	const FVector PreviewExtent =
		LocalLargeEquipmentPlacement->GetPlacementBoxExtent().GetAbs();
	const FBotanicusItemDefinition* PreviewDefinition =
		FindItemDefinition(
			this,
			LocalLargeEquipmentPlacement->GetItemKey());
	if (PreviewDefinition &&
		!PreviewDefinition->bShowAlignmentGuides)
	{
		return;
	}
	const float EdgeTolerance =
		PreviewDefinition
			? PreviewDefinition->AlignmentEdgeTolerance
			: EquipmentAlignmentGuideTolerance;
	const float AngleTolerance =
		PreviewDefinition
			? PreviewDefinition->AlignmentAngleTolerance
			: EquipmentAlignmentAngleTolerance;
	auto DrawGuidesToActor =
		[this,
		 World,
		 &PlacementTransform,
		 &PreviewExtent,
		 EdgeTolerance,
		 AngleTolerance](
			const AActor* OtherActor)
		{
			DrawOrientedEdgeAlignmentGuides(
				World,
				PlacementTransform,
				PreviewExtent,
				OtherActor,
				GetItemAlignmentExtent(OtherActor),
				EquipmentAlignmentGuideDistance,
				EdgeTolerance,
				AngleTolerance);
		};

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

		DrawGuidesToActor(OtherEquipment);
	}

	for (TActorIterator<ABotanicusPlaceableItemActor> ItemIt(World);
		 ItemIt;
		 ++ItemIt)
	{
		if (!ItemIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			DrawGuidesToActor(*ItemIt);
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
		LocalLargeEquipmentPlacement,
		LocalLargeEquipmentPlacement->GetActorLocation(),
		LargeEquipmentPlacementYaw);
	if (ABotanicusStorageShelfActor* Shelf =
			Cast<ABotanicusStorageShelfActor>(
				LocalLargeEquipmentPlacement))
	{
		Shelf->SetMoveContentsWithFurniture(false);
	}
	if (ABotanicusPreparationWorkbenchActor* Workbench =
			Cast<ABotanicusPreparationWorkbenchActor>(
				LocalLargeEquipmentPlacement))
	{
		Workbench->SetMoveContentsWithFurniture(false);
	}
	if (ABotanicusWorkSurfaceActor* WorkSurface =
			Cast<ABotanicusWorkSurfaceActor>(
				LocalLargeEquipmentPlacement))
	{
		WorkSurface->SetMoveContentsWithFurniture(false);
	}
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
	if (ABotanicusStorageShelfActor* Shelf =
			Cast<ABotanicusStorageShelfActor>(
				LocalLargeEquipmentPlacement))
	{
		Shelf->SetMoveContentsWithFurniture(false);
	}
	if (ABotanicusPreparationWorkbenchActor* Workbench =
			Cast<ABotanicusPreparationWorkbenchActor>(
				LocalLargeEquipmentPlacement))
	{
		Workbench->SetMoveContentsWithFurniture(false);
	}
	if (ABotanicusWorkSurfaceActor* WorkSurface =
			Cast<ABotanicusWorkSurfaceActor>(
				LocalLargeEquipmentPlacement))
	{
		WorkSurface->SetMoveContentsWithFurniture(false);
	}
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

	const ABotanicusStorageShelfActor* StorageShelf =
		Cast<ABotanicusStorageShelfActor>(Equipment);
	TArray<ABotanicusPlaceableItemActor*> StoredShelfItems;
	if (StorageShelf)
	{
		StorageShelf->GetStoredItems(StoredShelfItems);
	}
	const ABotanicusPreparationWorkbenchActor* MovingWorkbench =
		Cast<ABotanicusPreparationWorkbenchActor>(Equipment);
	TArray<ABotanicusSalePotActor*> PreparedPots;
	if (MovingWorkbench)
	{
		MovingWorkbench->GetPreparedPots(PreparedPots);
	}
	const ABotanicusWorkSurfaceActor* MovingWorkSurface =
		Cast<ABotanicusWorkSurfaceActor>(Equipment);
	TArray<AActor*> WorkSurfaceContents;
	if (MovingWorkSurface)
	{
		MovingWorkSurface->GetSurfaceContents(
			WorkSurfaceContents);
	}
	if (StorageShelf && StorageShelf->IsWallMountedShelf())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetPlayerViewPoint(ViewLocation, ViewRotation);
		FCollisionQueryParams WallQuery(
			SCENE_QUERY_STAT(BotanicusWallShelfPlacement),
			false);
		WallQuery.AddIgnoredActor(Equipment);
		WallQuery.AddIgnoredActor(ControlledPawn);
		FHitResult WallHit;
		const bool bFoundWall = World->LineTraceSingleByChannel(
			WallHit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * 450.0f,
			ECC_Visibility,
			WallQuery);
		const FVector ShelfExtent =
			StorageShelf->GetPlacementBoxExtent().GetAbs();
		if (!bFoundWall || FMath::Abs(WallHit.ImpactNormal.Z) > 0.3f)
		{
			OutTransform = FTransform(
				FRotator(0.0f, RequestedYaw, 0.0f),
				RequestedLocation,
				Equipment->GetActorScale3D());
			return false;
		}

		const FVector WallNormal = WallHit.ImpactNormal.GetSafeNormal();
		const FRotator WallRotation(
			0.0f,
			WallNormal.Rotation().Yaw,
			0.0f);
		const FVector WallLocation =
			WallHit.ImpactPoint +
			WallNormal * (ShelfExtent.X + 3.0f);
		OutTransform = FTransform(
			WallRotation,
			WallLocation,
			Equipment->GetActorScale3D());

		FCollisionObjectQueryParams WallObjectQuery;
		WallObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
		WallObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
		WallObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams WallOverlapQuery(
			SCENE_QUERY_STAT(BotanicusWallShelfPlacementOverlap),
			false);
		WallOverlapQuery.AddIgnoredActor(Equipment);
		WallOverlapQuery.AddIgnoredActor(ControlledPawn);
		for (ABotanicusPlaceableItemActor* StoredItem :
			 StoredShelfItems)
		{
			WallOverlapQuery.AddIgnoredActor(StoredItem);
		}
		const FVector TestExtent(
			FMath::Max(4.0f, ShelfExtent.X - 4.0f),
			FMath::Max(4.0f, ShelfExtent.Y - 4.0f),
			FMath::Max(4.0f, ShelfExtent.Z - 4.0f));
		TArray<FOverlapResult> WallOverlaps;
		return !World->OverlapMultiByObjectType(
			WallOverlaps,
			WallLocation,
			WallRotation.Quaternion(),
			WallObjectQuery,
			FCollisionShape::MakeBox(TestExtent),
			WallOverlapQuery);
	}

	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusEquipmentPlacementFloor),
		false);
	FloorQuery.AddIgnoredActor(Equipment);
	FloorQuery.AddIgnoredActor(ControlledPawn);
	for (ABotanicusPlaceableItemActor* StoredItem :
		 StoredShelfItems)
	{
		FloorQuery.AddIgnoredActor(StoredItem);
	}
	for (ABotanicusSalePotActor* PreparedPot : PreparedPots)
	{
		FloorQuery.AddIgnoredActor(PreparedPot);
	}
	for (AActor* SurfaceContent : WorkSurfaceContents)
	{
		FloorQuery.AddIgnoredActor(SurfaceContent);
	}
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
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, Equipment->GetItemKey());
	const FVector EffectiveBoxExtent =
		Definition &&
			!Definition->CollisionHalfExtentOverride.IsNearlyZero()
			? Definition->CollisionHalfExtentOverride.GetAbs()
			: BoxExtent;
	const FVector PlacementLocation(
		RequestedLocation.X,
		RequestedLocation.Y,
		bFoundFloor
			? FloorHit.ImpactPoint.Z + EffectiveBoxExtent.Z + 3.0f
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
	for (ABotanicusPlaceableItemActor* StoredItem :
		 StoredShelfItems)
	{
		OverlapQuery.AddIgnoredActor(StoredItem);
	}
	for (ABotanicusSalePotActor* PreparedPot : PreparedPots)
	{
		OverlapQuery.AddIgnoredActor(PreparedPot);
	}
	for (AActor* SurfaceContent : WorkSurfaceContents)
	{
		OverlapQuery.AddIgnoredActor(SurfaceContent);
	}

	const FVector TestExtent(
		FMath::Max(5.0f, EffectiveBoxExtent.X - 4.0f),
		FMath::Max(5.0f, EffectiveBoxExtent.Y - 4.0f),
		FMath::Max(5.0f, EffectiveBoxExtent.Z - 5.0f));
	TArray<FOverlapResult> Overlaps;
	return !World->OverlapMultiByObjectType(
		Overlaps,
		PlacementLocation,
		PlacementRotation,
		ObjectQuery,
		FCollisionShape::MakeBox(TestExtent),
		OverlapQuery);
}

void ABotanicusPlayerController::BeginQuickBarItemPlacement()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	UWorld* World = GetWorld();
	if (!IsLocalPlayerController() || !QuickBar || !World)
	{
		return;
	}
	if (HasCarriedTransplant())
	{
		ClientMessage(
			TEXT("Vous devez d'abord rempoter la plante portee."));
		return;
	}

	const FBotanicusQuickBarSlot SelectedSlot =
		QuickBar->GetSelectedSlot();
	if (SelectedSlot.IsEmpty())
	{
		ClientMessage(TEXT("Selectionnez d'abord un objet dans la hotbar."));
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, SelectedSlot.ItemKey);
	if (!Definition ||
		Definition->WeightClass !=
			EBotanicusItemWeightClass::Hotbar ||
		!Definition->CanBePlacedOn(
			EBotanicusPlacementSurface::Floor))
	{
		ClientMessage(TEXT("Cet objet ne peut pas etre pose au sol."));
		return;
	}

	UClass* PreviewClass =
		Definition->WorldActorClass.LoadSynchronous();
	if (!PreviewClass ||
		!PreviewClass->IsChildOf(
			ABotanicusPlaceableItemActor::StaticClass()))
	{
		PreviewClass =
			ABotanicusPlaceableItemActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusPlaceableItemActor* Preview =
		World->SpawnActor<ABotanicusPlaceableItemActor>(
			PreviewClass,
			GetPawn()->GetActorTransform(),
			SpawnParameters);
	if (!Preview)
	{
		ClientMessage(TEXT("Impossible de creer l'apercu de l'objet."));
		return;
	}

	DestroyEquippedQuickBarItem();
	Preview->Tags.AddUnique(TEXT("BotanicusPlacementPreview"));
	const int32 InitialPlacementQuantity =
		SelectedSlot.ItemKey == TEXT("PottingSoil")
			? FMath::Clamp(
				SelectedSlot.Quantity,
				1,
				PottingSoilGroundStackLimit)
			: 1;
	Preview->InitializePlacedItem(
		SelectedSlot.ItemKey,
		InitialPlacementQuantity);
	ApplyCarriedItemState(SelectedSlot, Preview);
	Preview->ConfigureAsLocalPreview(false);
	LocalQuickBarItemPreview = Preview;

	ABotanicusPlaceableItemActor* InspectedItem =
		World->SpawnActor<ABotanicusPlaceableItemActor>(
			PreviewClass,
			GetPawn()->GetActorTransform(),
			SpawnParameters);
	if (InspectedItem)
	{
		InspectedItem->Tags.AddUnique(
			TEXT("BotanicusInspectionPreview"));
		InspectedItem->InitializePlacedItem(
			SelectedSlot.ItemKey,
			InitialPlacementQuantity);
		ApplyCarriedItemState(SelectedSlot, InspectedItem);
		InspectedItem->ConfigureAsLocalPreview(true);
		InspectedItem->ConfigureAsLocalInspection();
		// This representation exists only while ground-placement mode is active
		// (after pressing A). Keep the regular hotbar-held item at its authored
		// size, and compact only this placement-mode representation.
		InspectedItem->SetActorScale3D(
			InspectedItem->GetActorScale3D() * 0.45f);
		LocalInspectedQuickBarItem = InspectedItem;
	}

	LocalQuickBarItemSlotIndex = QuickBar->GetSelectedSlotIndex();
	LocalQuickBarItemInstanceId = SelectedSlot.InstanceId;
	LocalQuickBarItemKey = SelectedSlot.ItemKey;
	LocalQuickBarPlacementQuantity = InitialPlacementQuantity;
	QuickBarItemPlacementYaw = GetPawn()->GetActorRotation().Yaw;
	QuickBarItemPreviewUpdateAccumulator = 1.0f;
	bLocalQuickBarItemPlacementValid = false;
}

void ABotanicusPlayerController::ApplyCarriedItemState(
	const FBotanicusQuickBarSlot& Slot,
	ABotanicusPlaceableItemActor* Item) const
{
	if (!IsValid(Item))
	{
		return;
	}

	if (Slot.CarriedState.bHasMultiPlanterState)
	{
		if (ABotanicusMultiPlantPotActor* MultiPlanter =
			Cast<ABotanicusMultiPlantPotActor>(Item))
		{
			TArray<FBotanicusMultiPlantSlotState> Slots;
			for (int32 Index = 0;
				 Index <
				 Slot.CarriedState.MultiPlanterPlantKeys.Num();
				 ++Index)
			{
				FBotanicusMultiPlantSlotState& PlantSlot =
					Slots.AddDefaulted_GetRef();
				PlantSlot.PlantKey =
					Slot.CarriedState.
						MultiPlanterPlantKeys[Index];
				PlantSlot.WaterLevel =
					Slot.CarriedState.MultiPlanterWaterLevels.
						IsValidIndex(Index)
						? Slot.CarriedState.
							MultiPlanterWaterLevels[Index]
						: 0.0f;
				PlantSlot.GrowthProgress =
					Slot.CarriedState.
						MultiPlanterGrowthProgress.
						IsValidIndex(Index)
						? Slot.CarriedState.
							MultiPlanterGrowthProgress[Index]
						: 0.0f;
				PlantSlot.CareScore =
					Slot.CarriedState.MultiPlanterCareScores.
						IsValidIndex(Index)
						? Slot.CarriedState.
							MultiPlanterCareScores[Index]
						: 0.0f;
				PlantSlot.WateringCount =
					Slot.CarriedState.
						MultiPlanterWateringCounts.
						IsValidIndex(Index)
						? Slot.CarriedState.
							MultiPlanterWateringCounts[Index]
						: 0;
			}
			MultiPlanter->RestoreMultiPlantState(
				Slot.CarriedState.MultiPlanterSoilUnits,
				Slots);
		}
	}
	else if (Slot.CarriedState.bHasPlantPotState)
	{
		if (ABotanicusPlantPotActor* PlantPot =
			Cast<ABotanicusPlantPotActor>(Item))
		{
			PlantPot->RestoreGrowingState(
				Slot.CarriedState.bPlantPotHasSoil,
				Slot.CarriedState.PlantKey,
				Slot.CarriedState.WaterLevel,
				Slot.CarriedState.GrowthProgress,
				Slot.CarriedState.CareScore);
			PlantPot->RestoreWateringCount(
				Slot.CarriedState.WateringCount);
		}
	}
	else if (Slot.CarriedState.bHasSalePotState)
	{
		if (ABotanicusSalePotActor* SalePot =
			Cast<ABotanicusSalePotActor>(Item))
		{
			SalePot->RestoreSalePotState(
				Slot.CarriedState.SaleSoilItemKey,
				Slot.CarriedState.SalePlantItemKey);
		}
	}
	else if (Slot.CarriedState.bHasWateringCanState)
	{
		if (ABotanicusWateringCanActor* WateringCan =
			Cast<ABotanicusWateringCanActor>(Item))
		{
			WateringCan->RestoreWaterLevel(
				Slot.CarriedState.WateringCanWaterLevel);
		}
	}
}

void ABotanicusPlayerController::HandleEquippedQuickBarChanged()
{
	bLocalEquippedQuickBarDirty = true;
}

void ABotanicusPlayerController::DestroyEquippedQuickBarItem()
{
	if (IsValid(LocalEquippedQuickBarItem))
	{
		LocalEquippedQuickBarItem->Destroy();
	}
	LocalEquippedQuickBarItem = nullptr;
}

void ABotanicusPlayerController::UpdateEquippedQuickBarItem()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (LocalEquippedQuickBarSource.Get() != QuickBar)
	{
		if (LocalEquippedQuickBarSource.IsValid())
		{
			LocalEquippedQuickBarSource->OnQuickBarChanged.
				RemoveDynamic(
					this,
					&ABotanicusPlayerController::HandleEquippedQuickBarChanged);
		}
		LocalEquippedQuickBarSource = QuickBar;
		if (QuickBar)
		{
			QuickBar->OnQuickBarChanged.AddUniqueDynamic(
				this,
				&ABotanicusPlayerController::HandleEquippedQuickBarChanged);
		}
		bLocalEquippedQuickBarDirty = true;
	}

	const bool bCanDisplayEquippedItem =
		QuickBar &&
		BotanicusCharacter &&
		!bBuildingTopDownViewActive &&
		!IsValid(LocalQuickBarItemPreview) &&
		!IsValid(LocalMovedPlaceableItem);
	if (!bCanDisplayEquippedItem)
	{
		DestroyEquippedQuickBarItem();
		return;
	}

	const int32 SelectedSlotIndex =
		QuickBar->GetSelectedSlotIndex();
	const FBotanicusQuickBarSlot SelectedSlot =
		QuickBar->GetSelectedSlot();
	const FName DisplayItemKey =
		HasCarriedTransplant() &&
		!CarriedTransplantItemKey.IsNone()
			? CarriedTransplantItemKey
			: SelectedSlot.ItemKey;
	const bool bSelectionChanged =
		bLocalEquippedQuickBarDirty ||
		LocalEquippedQuickBarSlotIndex != SelectedSlotIndex ||
		LocalEquippedQuickBarInstanceId != SelectedSlot.InstanceId ||
		LocalEquippedQuickBarItemKey != DisplayItemKey ||
		LocalEquippedQuickBarQuantity != SelectedSlot.Quantity;
	if (bSelectionChanged)
	{
		DestroyEquippedQuickBarItem();
		LocalEquippedQuickBarSlotIndex = SelectedSlotIndex;
		LocalEquippedQuickBarInstanceId = SelectedSlot.InstanceId;
		LocalEquippedQuickBarItemKey = DisplayItemKey;
		LocalEquippedQuickBarQuantity = SelectedSlot.Quantity;
		bLocalEquippedQuickBarDirty = false;
	}

	if (SelectedSlot.IsEmpty())
	{
		DestroyEquippedQuickBarItem();
		return;
	}

	if (!IsValid(LocalEquippedQuickBarItem))
	{
		const FBotanicusItemDefinition* Definition =
			FindItemDefinition(this, DisplayItemKey);
		UWorld* World = GetWorld();
		if (!Definition || !World)
		{
			return;
		}

		UClass* EquippedClass =
			Definition->WorldActorClass.LoadSynchronous();
		if (!EquippedClass ||
			!EquippedClass->IsChildOf(
				ABotanicusPlaceableItemActor::StaticClass()))
		{
			EquippedClass =
				ABotanicusPlaceableItemActor::StaticClass();
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalEquippedQuickBarItem =
			World->SpawnActor<ABotanicusPlaceableItemActor>(
				EquippedClass,
				BotanicusCharacter->GetActorTransform(),
				SpawnParameters);
		if (!IsValid(LocalEquippedQuickBarItem))
		{
			return;
		}

		LocalEquippedQuickBarItem->Tags.AddUnique(
			TEXT("BotanicusEquippedPreview"));
		LocalEquippedQuickBarItem->InitializePlacedItem(
			DisplayItemKey,
			1);
		if (!HasCarriedTransplant())
		{
			ApplyCarriedItemState(
				SelectedSlot,
				LocalEquippedQuickBarItem);
		}
		LocalEquippedQuickBarItem->ConfigureAsLocalPreview(true);
		LocalEquippedQuickBarItem->ConfigureAsLocalInspection();

		// Harvested plants use their full-size world mesh.  That is correct once
		// the plant is placed, but far too large for the first-person hotbar
		// representation.  Only this local inspection actor is scaled down; the
		// placement preview and the replicated world actor keep their authored
		// dimensions.
		const UBotanicusPlantSubsystem* Plants =
			GetGameInstance()
				? GetGameInstance()->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		if (Plants && Plants->FindPlantByHarvestItem(DisplayItemKey))
		{
			LocalEquippedQuickBarItem->SetActorScale3D(
				LocalEquippedQuickBarItem->GetActorScale3D() * 0.25f);
		}
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();
	const FVector ViewRight =
		FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const FVector ViewUp =
		FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z);
	const float ItemRadius =
		LocalEquippedQuickBarItem->GetPlacementBoxExtent().
			GetAbs().GetMax();
	const float ForwardDistance =
		FMath::Clamp(
			70.0f + ItemRadius * 0.35f,
			75.0f,
			130.0f);
	const bool bIsPottingSoil =
		DisplayItemKey == TEXT("PottingSoil");
	const float HeldRightOffset =
		bIsPottingSoil ? 68.0f : 24.0f;
	const float HeldUpOffset =
		bIsPottingSoil ? 32.0f : -24.0f;
	const float HeldRoll =
		bIsPottingSoil ? 90.0f : 0.0f;
	LocalEquippedQuickBarItem->SetActorLocationAndRotation(
		ViewLocation +
			ViewForward * ForwardDistance +
			ViewRight * HeldRightOffset +
			ViewUp * HeldUpOffset,
		FRotator(
			0.0f,
			ViewRotation.Yaw + 180.0f,
			HeldRoll),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ABotanicusPlayerController::ReleaseQuickBarThrowCharge()
{
	if (!bQuickBarThrowChargeActive)
	{
		return;
	}

	bQuickBarThrowChargeActive = false;
	HideThrowPowerWidget();
	const float HoldDuration = FMath::Clamp(
		static_cast<float>(
			FPlatformTime::Seconds() -
			QuickBarThrowChargeStartTime),
		0.0f,
		MaximumQuickBarThrowHoldDuration);
	if (HoldDuration < QuickBarThrowHoldThreshold)
	{
		ConfirmQuickBarItemPlacement();
		return;
	}

	ThrowSelectedQuickBarItem(HoldDuration);
}

void ABotanicusPlayerController::InitializeThrowPowerWidget()
{
	if (!IsLocalPlayerController() || ThrowPowerWidget)
	{
		return;
	}

	ThrowPowerWidget =
		CreateWidget<UBotanicusThrowPowerWidget>(
			this,
			UBotanicusThrowPowerWidget::StaticClass());
	if (!ThrowPowerWidget)
	{
		return;
	}

	ThrowPowerWidget->AddToPlayerScreen(62);
	ThrowPowerWidget->SetAlignmentInViewport(
		FVector2D(0.5f, 0.5f));
	ThrowPowerWidget->SetDesiredSizeInViewport(
		FVector2D(360.0f, 82.0f));
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	GetViewportSize(ViewportWidth, ViewportHeight);
	ThrowPowerWidget->SetPositionInViewport(
		FVector2D(
			static_cast<float>(ViewportWidth) * 0.5f,
			static_cast<float>(ViewportHeight) - 205.0f),
		true);
	ThrowPowerWidget->SetVisibility(
		ESlateVisibility::Collapsed);
}

void ABotanicusPlayerController::UpdateThrowPowerWidget()
{
	if (!bQuickBarThrowChargeActive)
	{
		return;
	}

	InitializeThrowPowerWidget();
	if (!ThrowPowerWidget)
	{
		return;
	}

	const float HoldDuration = FMath::Max(
		0.0f,
		static_cast<float>(
			FPlatformTime::Seconds() -
			QuickBarThrowChargeStartTime));
	ThrowPowerWidget->SetThrowPower(
		HoldDuration /
			FMath::Max(
				0.1f,
				MaximumQuickBarThrowHoldDuration));
	ThrowPowerWidget->SetVisibility(
		ESlateVisibility::HitTestInvisible);
}

void ABotanicusPlayerController::HideThrowPowerWidget()
{
	if (ThrowPowerWidget)
	{
		ThrowPowerWidget->SetThrowPower(0.0f);
		ThrowPowerWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

void ABotanicusPlayerController::ThrowSelectedQuickBarItem(
	float HoldDuration)
{
	if (!IsLocalPlayerController() ||
		!GetPawn() ||
		!IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalMovedPlaceableItem) ||
		LocalQuickBarItemSlotIndex == INDEX_NONE ||
		LocalQuickBarItemKey.IsNone())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	FVector ThrowDirection =
		ViewRotation.Vector() + FVector(0.0f, 0.0f, 0.12f);
	if (!ThrowDirection.Normalize())
	{
		ThrowDirection = GetPawn()->GetActorForwardVector();
	}

	const int32 SlotIndex = LocalQuickBarItemSlotIndex;
	const FGuid InstanceId = LocalQuickBarItemInstanceId;
	const FName ItemKey = LocalQuickBarItemKey;
	CancelQuickBarItemPlacement();
	ServerThrowQuickBarItem(
		SlotIndex,
		InstanceId,
		ItemKey,
		ThrowDirection,
		HoldDuration);
}

void ABotanicusPlayerController::BeginWorldItemMove(
	ABotanicusPlaceableItemActor* WorldItem)
{
	UWorld* World = GetWorld();
	if (!IsLocalPlayerController() || !World || !GetPawn() ||
		!IsValid(WorldItem) ||
		IsValid(LocalQuickBarItemPreview))
	{
		return;
	}

	const FName ItemKey = WorldItem->GetItemKey();
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	if (!Definition ||
		(!Definition->CanBePlacedOn(
			 EBotanicusPlacementSurface::Floor) &&
		 !ABotanicusStorageShelfActor::IsCatalogItemCompatible(
			 this,
			 ItemKey) &&
		 !ABotanicusWorkSurfaceActor::IsCatalogItemCompatible(
			 this,
			 ItemKey)))
	{
		ClientMessage(TEXT("Cet objet ne peut pas etre deplace."));
		return;
	}

	UClass* PreviewClass =
		Definition->WorldActorClass.LoadSynchronous();
	if (!PreviewClass ||
		!PreviewClass->IsChildOf(
			ABotanicusPlaceableItemActor::StaticClass()))
	{
		PreviewClass =
			ABotanicusPlaceableItemActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusPlaceableItemActor* Preview =
		World->SpawnActor<ABotanicusPlaceableItemActor>(
			PreviewClass,
			WorldItem->GetActorTransform(),
			SpawnParameters);
	if (!Preview)
	{
		ClientMessage(TEXT("Impossible de deplacer cet objet."));
		return;
	}

	Preview->Tags.AddUnique(TEXT("BotanicusPlacementPreview"));
	Preview->InitializePlacedItem(ItemKey, WorldItem->GetQuantity());
	Preview->ConfigureAsLocalPreview(false);
	LocalQuickBarItemPreview = Preview;
	LocalMovedPlaceableItem = WorldItem;
	LocalQuickBarItemSlotIndex = INDEX_NONE;
	LocalQuickBarItemInstanceId.Invalidate();
	LocalQuickBarItemKey = ItemKey;
	LocalQuickBarPlacementQuantity =
		FMath::Max(1, WorldItem->GetQuantity());
	QuickBarItemPlacementYaw = WorldItem->GetActorRotation().Yaw;
	QuickBarItemPreviewUpdateAccumulator = 1.0f;
	bLocalQuickBarItemPlacementValid = false;
	ServerBeginPlaceableItemMove(WorldItem);
	ClientMessage(
		TEXT(
			"Deplacement : le placement suit votre regard, clic gauche pour valider, clic droit pour annuler."));
}

void ABotanicusPlayerController::UpdateQuickBarItemPlacement(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		bBuildingTopDownViewActive ||
		!IsValid(LocalQuickBarItemPreview) ||
		!GetPawn())
	{
		return;
	}

	QuickBarItemPreviewUpdateAccumulator += DeltaTime;
	if (QuickBarItemPreviewUpdateAccumulator < (1.0f / 30.0f))
	{
		return;
	}
	QuickBarItemPreviewUpdateAccumulator = 0.0f;

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	if (IsValid(LocalInspectedQuickBarItem))
	{
		const FVector ViewForward = ViewRotation.Vector();
		const FVector ViewRight =
			FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
		const FVector ViewUp =
			FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z);
		LocalInspectedQuickBarItem->SetActorLocationAndRotation(
			ViewLocation +
				ViewForward * 85.0f +
				ViewRight * 52.0f -
				ViewUp * 22.0f,
			FRotator(
				0.0f,
				ViewRotation.Yaw + 180.0f,
				0.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	const FVector RequestedLocation =
		GetViewDirectedGroundPlacementLocation(
			MinimumQuickBarItemPlacementDistance,
			MaximumQuickBarItemPlacementDistance);
	const ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	const UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;

	if (!IsValid(LocalMovedPlaceableItem))
	{
		if (LocalQuickBarItemKey == TEXT("PottingSoil") &&
			QuickBar)
		{
			const FBotanicusQuickBarSlot SelectedSlot =
				QuickBar->GetSlot(LocalQuickBarItemSlotIndex);
			const int32 MaximumQuantity =
				SelectedSlot.ItemKey == LocalQuickBarItemKey &&
				SelectedSlot.InstanceId ==
					LocalQuickBarItemInstanceId
					? FMath::Min(
						SelectedSlot.Quantity,
						PottingSoilGroundStackLimit)
					: 1;
			LocalQuickBarPlacementQuantity = FMath::Clamp(
				LocalQuickBarPlacementQuantity,
				1,
				FMath::Max(1, MaximumQuantity));
			LocalQuickBarItemPreview->InitializePlacedItem(
				LocalQuickBarItemKey,
				LocalQuickBarPlacementQuantity);
			RefreshStorageQuantityWidget(
				true,
				LocalQuickBarItemKey,
				LocalQuickBarPlacementQuantity,
				FMath::Max(1, MaximumQuantity));
		}
		else
		{
			if (LocalQuickBarPlacementQuantity != 1)
			{
				LocalQuickBarPlacementQuantity = 1;
				LocalQuickBarItemPreview->InitializePlacedItem(
					LocalQuickBarItemKey,
					1);
			}
			if (StorageQuantityWidget &&
				!IsValid(LocalStorageCollectionItem))
			{
				StorageQuantityWidget->SetVisibility(
					ESlateVisibility::Collapsed);
			}
		}
	}

	FTransform PlacementTransform;
	bLocalQuickBarItemPlacementValid =
		ResolveQuickBarItemPlacement(
			LocalQuickBarItemKey,
			RequestedLocation,
			QuickBarItemPlacementYaw,
			PlacementTransform,
			LocalMovedPlaceableItem,
			LocalQuickBarPlacementQuantity,
			true);
	LocalQuickBarItemPreview->SetActorTransform(
		PlacementTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	LocalQuickBarItemPreview->ConfigureAsLocalPreview(
		bLocalQuickBarItemPlacementValid);
	DrawQuickBarItemAlignmentGuides(PlacementTransform);
}

void ABotanicusPlayerController::RotateQuickBarItemPlacement(
	float Direction)
{
	if (!IsValid(LocalQuickBarItemPreview))
	{
		return;
	}

	const float RotationStep =
		[&]()
		{
			const FBotanicusItemDefinition* Definition =
				FindItemDefinition(this, LocalQuickBarItemKey);
			const bool bFineRotation =
				IsInputKeyDown(EKeys::LeftShift) ||
				IsInputKeyDown(EKeys::RightShift);
			if (Definition)
			{
				return bFineRotation
					? Definition->FineRotationStep
					: Definition->RotationStep;
			}
			return bFineRotation
				? FineEquipmentRotationStep
				: EquipmentRotationStep;
		}();
	QuickBarItemPlacementYaw =
		FMath::UnwindDegrees(
			QuickBarItemPlacementYaw +
			Direction * RotationStep);
	QuickBarItemPreviewUpdateAccumulator = 1.0f;
}

int32 ABotanicusPlayerController::
	GetMaximumQuickBarPlacementQuantity(
		ABotanicusStorageShelfActor* Shelf,
		int32 SlotIndex) const
{
	if (!IsValid(Shelf) ||
		SlotIndex < 0 ||
		!ABotanicusStorageShelfActor::IsCatalogItemCompatible(
			this,
			LocalQuickBarItemKey))
	{
		return 0;
	}

	const ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	const UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	const FBotanicusQuickBarSlot Slot = QuickBar
		? QuickBar->GetSlot(LocalQuickBarItemSlotIndex)
		: FBotanicusQuickBarSlot();
	if (Slot.IsEmpty() ||
		Slot.ItemKey != LocalQuickBarItemKey ||
		Slot.InstanceId != LocalQuickBarItemInstanceId)
	{
		return 0;
	}

	const int32 StackLimit =
		ABotanicusStorageShelfActor::GetStorageStackLimit(
			LocalQuickBarItemKey);
	const ABotanicusPlaceableItemActor* ExistingItem =
		Shelf->GetStoredItemInSlot(SlotIndex);
	if (ExistingItem &&
		ExistingItem->GetItemKey() != LocalQuickBarItemKey)
	{
		return 0;
	}
	const int32 ExistingQuantity =
		ExistingItem
			? FMath::Max(1, ExistingItem->GetQuantity())
			: 0;
	return FMath::Max(
		0,
		FMath::Min(
			Slot.Quantity,
			StackLimit - ExistingQuantity));
}

void ABotanicusPlayerController::AdjustQuickBarPlacementQuantity(
	int32 Direction)
{
	if (!IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalMovedPlaceableItem))
	{
		return;
	}

	int32 MaximumQuantity = 0;
	if (LocalQuickBarItemKey == TEXT("PottingSoil"))
	{
		const ABotanicusCharacter* BotanicusCharacter =
			Cast<ABotanicusCharacter>(GetPawn());
		const UBotanicusQuickBarComponent* QuickBar =
			BotanicusCharacter
				? BotanicusCharacter->GetQuickBarComponent()
				: nullptr;
		const FBotanicusQuickBarSlot Slot = QuickBar
			? QuickBar->GetSlot(LocalQuickBarItemSlotIndex)
			: FBotanicusQuickBarSlot();
		if (!Slot.IsEmpty() &&
			Slot.ItemKey == LocalQuickBarItemKey &&
			Slot.InstanceId == LocalQuickBarItemInstanceId)
		{
			MaximumQuantity = FMath::Min(
				Slot.Quantity,
				PottingSoilGroundStackLimit);
		}
	}
	if (MaximumQuantity <= 0)
	{
		ClientMessage(TEXT("Seuls les sacs de terreau se posent en pile au sol."));
		return;
	}

	const int32 Step =
		(IsInputKeyDown(EKeys::LeftShift) ||
		 IsInputKeyDown(EKeys::RightShift))
			? 5
			: 1;
	LocalQuickBarPlacementQuantity = FMath::Clamp(
		LocalQuickBarPlacementQuantity + Direction * Step,
		1,
		MaximumQuantity);
	LocalQuickBarItemPreview->InitializePlacedItem(
		LocalQuickBarItemKey,
		LocalQuickBarPlacementQuantity);
	RefreshStorageQuantityWidget(
		true,
		LocalQuickBarItemKey,
		LocalQuickBarPlacementQuantity,
		MaximumQuantity);
	QuickBarItemPreviewUpdateAccumulator = 1.0f;
}

void ABotanicusPlayerController::ConfirmQuickBarItemPlacement()
{
	if (!IsValid(LocalQuickBarItemPreview))
	{
		return;
	}
	if (!bLocalQuickBarItemPlacementValid)
	{
		ClientMessage(
			TEXT("Position invalide : avec A, l'objet doit etre pose au sol."));
		return;
	}

	const FVector RequestedLocation =
		LocalQuickBarItemPreview->GetActorLocation();
	if (IsValid(LocalMovedPlaceableItem))
	{
		ServerConfirmPlaceableItemMove(
			LocalMovedPlaceableItem,
			RequestedLocation,
			QuickBarItemPlacementYaw);
	}
	else
	{
		ServerPlaceQuickBarItem(
			LocalQuickBarItemSlotIndex,
			LocalQuickBarItemInstanceId,
			LocalQuickBarItemKey,
			RequestedLocation,
			QuickBarItemPlacementYaw,
			LocalQuickBarPlacementQuantity,
			true);
	}
	LocalQuickBarItemPreview->Destroy();
	if (IsValid(LocalInspectedQuickBarItem))
	{
		LocalInspectedQuickBarItem->Destroy();
	}
	LocalQuickBarItemPreview = nullptr;
	LocalInspectedQuickBarItem = nullptr;
	LocalMovedPlaceableItem = nullptr;
	LocalQuickBarItemSlotIndex = INDEX_NONE;
	LocalQuickBarItemInstanceId.Invalidate();
	LocalQuickBarItemKey = NAME_None;
	LocalQuickBarPlacementQuantity = 1;
	bLocalQuickBarItemPlacementValid = false;
	if (StorageQuantityWidget)
	{
		StorageQuantityWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

void ABotanicusPlayerController::CancelQuickBarItemPlacement()
{
	bQuickBarThrowChargeActive = false;
	HideThrowPowerWidget();
	if (IsValid(LocalMovedPlaceableItem))
	{
		ServerCancelPlaceableItemMove(LocalMovedPlaceableItem);
	}
	if (IsValid(LocalQuickBarItemPreview))
	{
		LocalQuickBarItemPreview->Destroy();
	}
	if (IsValid(LocalInspectedQuickBarItem))
	{
		LocalInspectedQuickBarItem->Destroy();
	}
	LocalQuickBarItemPreview = nullptr;
	LocalInspectedQuickBarItem = nullptr;
	LocalMovedPlaceableItem = nullptr;
	LocalQuickBarItemSlotIndex = INDEX_NONE;
	LocalQuickBarItemInstanceId.Invalidate();
	LocalQuickBarItemKey = NAME_None;
	LocalQuickBarPlacementQuantity = 1;
	bLocalQuickBarItemPlacementValid = false;
	if (StorageQuantityWidget &&
		!IsValid(LocalStorageCollectionItem))
	{
		StorageQuantityWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

bool ABotanicusPlayerController::FindAimedStorageSlot(
	ABotanicusStorageShelfActor*& OutShelf,
	int32& OutSlotIndex) const
{
	OutShelf = nullptr;
	OutSlotIndex = INDEX_NONE;
	if (!GetWorld() || !GetPawn())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection =
		ViewRotation.Vector().GetSafeNormal();
	float BestAngularOffset = TNumericLimits<float>::Max();
	for (TActorIterator<ABotanicusStorageShelfActor> ShelfIt(
			 GetWorld());
		 ShelfIt;
		 ++ShelfIt)
	{
		if (ShelfIt->ActorHasTag(
				TEXT("BotanicusPlacementPreview")) ||
			IsValid(ShelfIt->GetCarrier()) ||
			ShelfIt->IsInPlacementMode() ||
			FVector::DistSquared(
				GetPawn()->GetActorLocation(),
				ShelfIt->GetActorLocation()) >
				FMath::Square(700.0f))
		{
			continue;
		}

		for (int32 SlotIndex = 0;
			 SlotIndex < ShelfIt->GetStorageSlotCount();
			 ++SlotIndex)
		{
			const FVector ToSlot =
				ShelfIt->GetStorageSlotAimLocation(SlotIndex) -
				ViewLocation;
			const float ForwardDistance =
				FVector::DotProduct(ToSlot, ViewDirection);
			if (ForwardDistance < 40.0f ||
				ForwardDistance > 700.0f)
			{
				continue;
			}
			const float PerpendicularDistance =
				(ToSlot -
				 ViewDirection * ForwardDistance).Size();
			const float MaximumAimRadius =
				FMath::Clamp(
					ForwardDistance * 0.07f,
					22.0f,
					48.0f);
			if (PerpendicularDistance > MaximumAimRadius)
			{
				continue;
			}

			const float AngularOffset =
				PerpendicularDistance /
				FMath::Max(1.0f, ForwardDistance);
			if (AngularOffset < BestAngularOffset)
			{
				BestAngularOffset = AngularOffset;
				OutShelf = *ShelfIt;
				OutSlotIndex = SlotIndex;
			}
		}
	}
	return IsValid(OutShelf) &&
		OutSlotIndex != INDEX_NONE;
}

bool ABotanicusPlayerController::
	FindStorageDestinationAtLocation(
		FName ItemKey,
		int32 ItemQuantity,
		const FVector& ItemExtent,
		const FVector& WorldLocation,
		const AActor* IgnoredWorldItem,
		ABotanicusStorageShelfActor*& OutShelf,
		int32& OutSlotIndex,
		ABotanicusPlaceableItemActor*& OutExistingStack) const
{
	OutShelf = nullptr;
	OutSlotIndex = INDEX_NONE;
	OutExistingStack = nullptr;
	if (!GetWorld() ||
		!ABotanicusStorageShelfActor::IsCatalogItemCompatible(
			this,
			ItemKey))
	{
		return false;
	}

	float BestDistanceSquared = FMath::Square(35.0f);
	for (TActorIterator<ABotanicusStorageShelfActor> ShelfIt(
			 GetWorld());
		 ShelfIt;
		 ++ShelfIt)
	{
		for (int32 SlotIndex = 0;
			 SlotIndex < ShelfIt->GetStorageSlotCount();
			 ++SlotIndex)
		{
			FTransform CandidateTransform;
			ABotanicusPlaceableItemActor* ExistingStack = nullptr;
			if (!ShelfIt->GetStorageSlotPlacement(
					SlotIndex,
					ItemKey,
					ItemExtent,
					CandidateTransform,
					IgnoredWorldItem,
					&ExistingStack,
					ItemQuantity))
			{
				continue;
			}
			const float DistanceSquared = FVector::DistSquared(
				WorldLocation,
				CandidateTransform.GetLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				OutShelf = *ShelfIt;
				OutSlotIndex = SlotIndex;
				OutExistingStack = ExistingStack;
			}
		}
	}
	return IsValid(OutShelf) &&
		OutSlotIndex != INDEX_NONE;
}

bool ABotanicusPlayerController::ResolveQuickBarItemPlacement(
	FName ItemKey,
	const FVector& RequestedLocation,
	float RequestedYaw,
	FTransform& OutTransform,
	const AActor* IgnoredWorldItem,
	int32 RequestedQuantity,
	bool bFloorOnlyPlacement) const
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	const FVector BoxExtent = IsValid(LocalQuickBarItemPreview)
		? LocalQuickBarItemPreview->GetPlacementBoxExtent().GetAbs()
		: IsValid(IgnoredWorldItem)
			? CastChecked<ABotanicusPlaceableItemActor>(
				IgnoredWorldItem)->GetPlacementBoxExtent().GetAbs()
		: FVector(20.0f);
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	const FVector EffectiveBoxExtent =
		Definition &&
			!Definition->CollisionHalfExtentOverride.IsNearlyZero()
			? Definition->CollisionHalfExtentOverride.GetAbs()
			: BoxExtent;
	const int32 PlacementQuantity =
		IsValid(IgnoredWorldItem)
			? FMath::Max(
				1,
				CastChecked<ABotanicusPlaceableItemActor>(
					IgnoredWorldItem)->GetQuantity())
			: FMath::Max(1, RequestedQuantity);
	if (!World || !ControlledPawn ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			RequestedLocation) >
			FMath::Square(MaximumQuickBarItemPlacementDistance))
	{
		OutTransform = FTransform(
			FRotator(0.0f, RequestedYaw, 0.0f),
			RequestedLocation);
		return false;
	}

	if (!bFloorOnlyPlacement &&
		ABotanicusWorkSurfaceActor::IsCatalogItemCompatible(
			this,
			ItemKey))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetPlayerViewPoint(ViewLocation, ViewRotation);
		FCollisionQueryParams SurfaceTraceQuery(
			SCENE_QUERY_STAT(BotanicusWorkSurfaceAim),
			false);
		SurfaceTraceQuery.AddIgnoredActor(ControlledPawn);
		if (IsValid(LocalQuickBarItemPreview))
		{
			SurfaceTraceQuery.AddIgnoredActor(
				LocalQuickBarItemPreview);
		}
		if (IsValid(IgnoredWorldItem))
		{
			SurfaceTraceQuery.AddIgnoredActor(IgnoredWorldItem);
		}

		FHitResult SurfaceHit;
		if (World->LineTraceSingleByChannel(
				SurfaceHit,
				ViewLocation,
				ViewLocation + ViewRotation.Vector() * 700.0f,
				ECC_Visibility,
				SurfaceTraceQuery))
		{
			ABotanicusWorkSurfaceActor* WorkSurface =
				Cast<ABotanicusWorkSurfaceActor>(
					SurfaceHit.GetActor());
			FTransform SurfaceTransform;
			if (IsValid(WorkSurface) &&
				SurfaceHit.ImpactNormal.Z >= 0.65f &&
				WorkSurface->GetFreePlacementTransform(
					ItemKey,
					SurfaceHit.ImpactPoint,
					RequestedYaw,
					EffectiveBoxExtent,
					SurfaceTransform))
			{
				OutTransform = SurfaceTransform;

				FCollisionObjectQueryParams SurfaceObjectQuery;
				SurfaceObjectQuery.AddObjectTypesToQuery(
					ECC_WorldStatic);
				SurfaceObjectQuery.AddObjectTypesToQuery(
					ECC_WorldDynamic);
				SurfaceObjectQuery.AddObjectTypesToQuery(
					ECC_Pawn);
				FCollisionQueryParams SurfaceOverlapQuery(
					SCENE_QUERY_STAT(
						BotanicusWorkSurfacePlacementOverlap),
					false);
				SurfaceOverlapQuery.AddIgnoredActor(
					ControlledPawn);
				SurfaceOverlapQuery.AddIgnoredActor(WorkSurface);
				if (IsValid(LocalQuickBarItemPreview))
				{
					SurfaceOverlapQuery.AddIgnoredActor(
						LocalQuickBarItemPreview);
				}
				if (IsValid(IgnoredWorldItem))
				{
					SurfaceOverlapQuery.AddIgnoredActor(
						IgnoredWorldItem);
				}
				const FVector SurfaceTestExtent(
					FMath::Max(
						4.0f,
						EffectiveBoxExtent.X - 3.0f),
					FMath::Max(
						4.0f,
						EffectiveBoxExtent.Y - 3.0f),
					FMath::Max(
						4.0f,
						EffectiveBoxExtent.Z - 3.0f));
				TArray<FOverlapResult> SurfaceOverlaps;
				return !World->OverlapMultiByObjectType(
					SurfaceOverlaps,
					SurfaceTransform.GetLocation(),
					SurfaceTransform.GetRotation(),
					SurfaceObjectQuery,
					FCollisionShape::MakeBox(
						SurfaceTestExtent),
					SurfaceOverlapQuery);
			}
		}
	}

	if (!bFloorOnlyPlacement && ItemKey == TEXT("SelfCheckout"))
	{
		FTransform ClosestSlotTransform;
		float ClosestSlotDistanceSquared =
			TNumericLimits<float>::Max();
		bool bFoundAvailableSlot = false;
		for (TActorIterator<ABotanicusCashRegisterActor>
				 RegisterIt(World);
			 RegisterIt;
			 ++RegisterIt)
		{
			if (RegisterIt->ActorHasTag(
					TEXT("BotanicusPlacementPreview")))
			{
				continue;
			}
			FTransform CandidateTransform;
			if (!RegisterIt->
					FindClosestAvailableSelfCheckoutSlot(
						RequestedLocation,
						CandidateTransform,
						IgnoredWorldItem))
			{
				continue;
			}
			const float DistanceSquared =
				FVector::DistSquared2D(
					RequestedLocation,
					CandidateTransform.GetLocation());
			if (DistanceSquared <
				ClosestSlotDistanceSquared)
			{
				ClosestSlotDistanceSquared = DistanceSquared;
				ClosestSlotTransform = CandidateTransform;
				bFoundAvailableSlot = true;
			}
		}
		if (bFoundAvailableSlot &&
			ClosestSlotDistanceSquared <=
				FMath::Square(190.0f))
		{
			OutTransform = ClosestSlotTransform;
			return true;
		}
		OutTransform = FTransform(
			FRotator(0.0f, RequestedYaw, 0.0f),
			RequestedLocation);
		return false;
	}

	if (!bFloorOnlyPlacement && HasAuthority() &&
		ABotanicusStorageShelfActor::IsCatalogItemCompatible(
			this,
			ItemKey))
	{
		ABotanicusStorageShelfActor* RequestedShelf = nullptr;
		int32 RequestedSlotIndex = INDEX_NONE;
		ABotanicusPlaceableItemActor* RequestedExistingStack =
			nullptr;
		if (FindStorageDestinationAtLocation(
				ItemKey,
				PlacementQuantity,
				EffectiveBoxExtent,
				RequestedLocation,
				IgnoredWorldItem,
				RequestedShelf,
				RequestedSlotIndex,
				RequestedExistingStack) &&
			RequestedShelf->GetStorageSlotPlacement(
				RequestedSlotIndex,
				ItemKey,
				EffectiveBoxExtent,
				OutTransform,
				IgnoredWorldItem,
				nullptr,
				PlacementQuantity))
		{
			return true;
		}
	}

	ABotanicusStorageShelfActor* TargetShelf = nullptr;
	int32 TargetSlotIndex = INDEX_NONE;
	if (!bFloorOnlyPlacement && FindAimedStorageSlot(
			TargetShelf,
			TargetSlotIndex))
	{
		OutTransform = FTransform(
			FRotator(0.0f, RequestedYaw, 0.0f),
			TargetShelf->GetStorageSlotAimLocation(
				TargetSlotIndex));
		if (!ABotanicusStorageShelfActor::
				IsCatalogItemCompatible(this, ItemKey))
		{
			return false;
		}

		TargetShelf->ShowAvailableSlotsForLocalPlayer(
			ItemKey,
			IgnoredWorldItem);
		return TargetShelf->GetStorageSlotPlacement(
			TargetSlotIndex,
			ItemKey,
			EffectiveBoxExtent,
			OutTransform,
			IgnoredWorldItem,
			nullptr,
			PlacementQuantity);
	}

	if (Definition &&
		!Definition->CanBePlacedOn(
			EBotanicusPlacementSurface::Floor))
	{
		OutTransform = FTransform(
			FRotator(0.0f, RequestedYaw, 0.0f),
			RequestedLocation);
		return false;
	}

	const ABotanicusSalePotActor* SalePot =
		Cast<ABotanicusSalePotActor>(IgnoredWorldItem);
	const bool bIsSalePotItem =
		IsSalePotItemKey(ItemKey) || IsValid(SalePot);
	if (!bFloorOnlyPlacement && SalePot && SalePot->IsReadyForSale())
	{
		for (TActorIterator<ABotanicusSalesDisplayActor> DisplayIt(
				 World);
			 DisplayIt;
			 ++DisplayIt)
		{
			if (!DisplayIt->IsEmpty())
			{
				continue;
			}
			const FTransform DisplayTransform =
				DisplayIt->GetSalePotPlacementTransform();
			if (FVector::DistSquared2D(
					RequestedLocation,
					DisplayTransform.GetLocation()) <=
				FMath::Square(170.0f))
			{
				OutTransform = DisplayTransform;
				return true;
			}
		}
	}
	if (!bFloorOnlyPlacement && bIsSalePotItem)
	{
		for (TActorIterator<ABotanicusPreparationWorkbenchActor>
				 WorkbenchIt(World);
			 WorkbenchIt;
			 ++WorkbenchIt)
		{
			FTransform WorkbenchTransform;
			if (!WorkbenchIt->FindClosestAvailableSalePotSlot(
					RequestedLocation,
					WorkbenchTransform,
					SalePot))
			{
				continue;
			}
			if (FVector::DistSquared2D(
					RequestedLocation,
					WorkbenchTransform.GetLocation()) <=
				FMath::Square(170.0f))
			{
				OutTransform = WorkbenchTransform;
				return true;
			}
		}
	}

	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusQuickBarItemPlacementFloor),
		false);
	FloorQuery.AddIgnoredActor(ControlledPawn);
	if (IsValid(LocalQuickBarItemPreview))
	{
		FloorQuery.AddIgnoredActor(LocalQuickBarItemPreview);
	}
	if (IsValid(IgnoredWorldItem))
	{
		FloorQuery.AddIgnoredActor(IgnoredWorldItem);
	}
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

	const FVector PlacementLocation(
		RequestedLocation.X,
		RequestedLocation.Y,
		bFoundFloor
			? FloorHit.ImpactPoint.Z + EffectiveBoxExtent.Z + 3.0f
			: RequestedLocation.Z);
	const FQuat PlacementRotation =
		FRotator(0.0f, RequestedYaw, 0.0f).Quaternion();
	OutTransform = FTransform(
		PlacementRotation,
		PlacementLocation);
	if (!bFoundFloor || FloorHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}
	if (bFloorOnlyPlacement &&
		IsValid(Cast<ABotanicusPlaceableItemActor>(
			FloorHit.GetActor())))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams OverlapQuery(
		SCENE_QUERY_STAT(BotanicusQuickBarItemPlacementOverlap),
		false);
	OverlapQuery.AddIgnoredActor(ControlledPawn);
	if (IsValid(LocalQuickBarItemPreview))
	{
		OverlapQuery.AddIgnoredActor(LocalQuickBarItemPreview);
	}
	if (IsValid(IgnoredWorldItem))
	{
		OverlapQuery.AddIgnoredActor(IgnoredWorldItem);
	}
	const FVector TestExtent(
		FMath::Max(4.0f, EffectiveBoxExtent.X - 3.0f),
		FMath::Max(4.0f, EffectiveBoxExtent.Y - 3.0f),
		FMath::Max(4.0f, EffectiveBoxExtent.Z - 4.0f));
	TArray<FOverlapResult> Overlaps;
	return !World->OverlapMultiByObjectType(
		Overlaps,
		PlacementLocation,
		PlacementRotation,
		ObjectQuery,
		FCollisionShape::MakeBox(TestExtent),
		OverlapQuery);
}

void ABotanicusPlayerController::DrawQuickBarItemAlignmentGuides(
	const FTransform& PlacementTransform) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(LocalQuickBarItemPreview))
	{
		return;
	}

	const FVector PreviewExtent =
		LocalQuickBarItemPreview->GetPlacementBoxExtent().GetAbs();
	const FBotanicusItemDefinition* PreviewDefinition =
		FindItemDefinition(this, LocalQuickBarItemKey);
	if (PreviewDefinition &&
		!PreviewDefinition->bShowAlignmentGuides)
	{
		return;
	}
	const float EdgeTolerance =
		PreviewDefinition
			? PreviewDefinition->AlignmentEdgeTolerance
			: EquipmentAlignmentGuideTolerance;
	const float AngleTolerance =
		PreviewDefinition
			? PreviewDefinition->AlignmentAngleTolerance
			: EquipmentAlignmentAngleTolerance;
	auto DrawGuidesToActor =
		[this,
		 World,
		 &PlacementTransform,
		 &PreviewExtent,
		 EdgeTolerance,
		 AngleTolerance](
			const AActor* OtherActor)
		{
			if (!IsValid(OtherActor) ||
				OtherActor == LocalQuickBarItemPreview)
			{
				return;
			}

			DrawOrientedEdgeAlignmentGuides(
				World,
				PlacementTransform,
				PreviewExtent,
				OtherActor,
				GetItemAlignmentExtent(OtherActor),
				EquipmentAlignmentGuideDistance,
				EdgeTolerance,
				AngleTolerance);
		};

	for (TActorIterator<ABotanicusPlaceableItemActor> ItemIt(World);
		 ItemIt;
		 ++ItemIt)
	{
		if (!ItemIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			DrawGuidesToActor(*ItemIt);
		}
	}
	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(World);
		 EquipmentIt;
		 ++EquipmentIt)
	{
		if (!IsValid(EquipmentIt->GetCarrier()))
		{
			DrawGuidesToActor(*EquipmentIt);
		}
	}
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

	if (NearestParcel->IsOpened())
	{
		ServerCollectDeliveryParcel(NearestParcel);
	}
	else
	{
		BeginParcelMoveCharge(NearestParcel);
	}
	return true;
}

void ABotanicusPlayerController::BeginParcelMoveCharge(
	ABotanicusDeliveryParcelActor* Parcel)
{
	if (!IsLocalPlayerController() ||
		!IsValid(Parcel) ||
		Parcel->IsOpened() ||
		IsValid(LocalParcelMoveCandidate) ||
		IsValid(LocalParcelMovePreview) ||
		IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalLargeEquipmentPlacement))
	{
		return;
	}

	LocalParcelMoveCandidate = Parcel;
	ParcelMoveChargeElapsed = 0.0f;
	bEquipmentCarryKeyHeld = true;
	InitializeInteractionTargetWidget();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->BeginLocalHoldProgress(
			PlaceableItemMoveHoldDuration);
	}

	if (!CarryProgressWidget)
	{
		CarryProgressWidget =
			CreateWidget<UBotanicusCarryProgressWidget>(
				this,
				UBotanicusCarryProgressWidget::StaticClass());
		if (CarryProgressWidget)
		{
			CarryProgressWidget->AddToPlayerScreen(50);
			CarryProgressWidget->SetAlignmentInViewport(
				FVector2D(0.5f, 0.5f));
			CarryProgressWidget->SetDesiredSizeInViewport(
				FVector2D(68.0f, 68.0f));
		}
	}

	if (CarryProgressWidget)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		GetViewportSize(ViewportWidth, ViewportHeight);
		CarryProgressWidget->SetPositionInViewport(
			FVector2D(
				static_cast<float>(ViewportWidth) * 0.5f,
				static_cast<float>(ViewportHeight) * 0.5f + 85.0f),
			true);
		CarryProgressWidget->SetCarryProgress(0.0f);
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}
}

void ABotanicusPlayerController::UpdateParcelMoveCharge(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		!IsValid(LocalParcelMoveCandidate))
	{
		return;
	}

	if (!bEquipmentCarryKeyHeld ||
		LocalParcelMoveCandidate->IsOpened() ||
		!IsLookingAtWorldItem(LocalParcelMoveCandidate, 450.0f))
	{
		CancelParcelMoveCharge();
		return;
	}

	ParcelMoveChargeElapsed += DeltaTime;
	const float ChargeProgress = FMath::Clamp(
		ParcelMoveChargeElapsed /
			FMath::Max(0.1f, PlaceableItemMoveHoldDuration),
		0.0f,
		1.0f);
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetCarryProgress(ChargeProgress);
	}
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(ChargeProgress);
	}
	if (ChargeProgress < 1.0f)
	{
		return;
	}

	ABotanicusDeliveryParcelActor* ParcelToMove =
		LocalParcelMoveCandidate;
	LocalParcelMoveCandidate = nullptr;
	ParcelMoveChargeElapsed = 0.0f;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}
	BeginDeliveryParcelMove(ParcelToMove);
}

void ABotanicusPlayerController::CancelParcelMoveCharge()
{
	LocalParcelMoveCandidate = nullptr;
	ParcelMoveChargeElapsed = 0.0f;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}
}

void ABotanicusPlayerController::BeginDeliveryParcelMove(
	ABotanicusDeliveryParcelActor* Parcel)
{
	UWorld* World = GetWorld();
	if (!IsLocalPlayerController() ||
		!World ||
		!GetPawn() ||
		!IsValid(Parcel) ||
		Parcel->IsOpened() ||
		IsValid(LocalParcelMovePreview))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	ABotanicusDeliveryParcelActor* Preview =
		World->SpawnActor<ABotanicusDeliveryParcelActor>(
			ABotanicusDeliveryParcelActor::StaticClass(),
			Parcel->GetActorTransform(),
			SpawnParameters);
	if (!Preview)
	{
		ClientMessage(TEXT("Impossible de deplacer ce colis."));
		return;
	}

	Preview->SetReplicates(false);
	Preview->Tags.AddUnique(TEXT("BotanicusPlacementPreview"));
	Preview->RestoreParcelState(
		Parcel->GetItemKey(),
		Parcel->GetQuantity(),
		Parcel->GetCutCoverageMask(),
		false);
	Preview->SetActorEnableCollision(false);

	LocalParcelMovePreview = Preview;
	LocalMovedDeliveryParcel = Parcel;
	ParcelPlacementYaw = Parcel->GetActorRotation().Yaw;
	ParcelPreviewUpdateAccumulator = 1.0f;
	bLocalParcelPlacementValid = false;
	ServerBeginDeliveryParcelMove(Parcel);
	ClientMessage(
		TEXT(
			"Deplacement du colis : le placement suit votre regard, molette pour tourner, clic gauche pour poser."));
}

void ABotanicusPlayerController::UpdateDeliveryParcelPlacement(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		bBuildingTopDownViewActive ||
		!IsValid(LocalParcelMovePreview) ||
		!IsValid(LocalMovedDeliveryParcel) ||
		!GetPawn())
	{
		return;
	}

	ParcelPreviewUpdateAccumulator += DeltaTime;
	if (ParcelPreviewUpdateAccumulator < (1.0f / 30.0f))
	{
		return;
	}
	ParcelPreviewUpdateAccumulator = 0.0f;

	const FVector RequestedLocation =
		GetViewDirectedGroundPlacementLocation(
			MinimumQuickBarItemPlacementDistance,
			MaximumQuickBarItemPlacementDistance);
	FTransform PlacementTransform;
	bLocalParcelPlacementValid =
		ResolveDeliveryParcelPlacement(
			LocalMovedDeliveryParcel,
			RequestedLocation,
			ParcelPlacementYaw,
			PlacementTransform);
	LocalParcelMovePreview->SetActorTransform(
		PlacementTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ABotanicusPlayerController::RotateDeliveryParcelPlacement(
	float Direction)
{
	if (!IsValid(LocalParcelMovePreview))
	{
		return;
	}

	const bool bFineRotation =
		IsInputKeyDown(EKeys::LeftShift) ||
		IsInputKeyDown(EKeys::RightShift);
	ParcelPlacementYaw =
		FMath::UnwindDegrees(
			ParcelPlacementYaw +
			Direction *
				(bFineRotation
					? FineEquipmentRotationStep
					: EquipmentRotationStep));
	ParcelPreviewUpdateAccumulator = 1.0f;
}

void ABotanicusPlayerController::ConfirmDeliveryParcelPlacement()
{
	if (!IsValid(LocalParcelMovePreview) ||
		!IsValid(LocalMovedDeliveryParcel))
	{
		return;
	}
	if (!bLocalParcelPlacementValid)
	{
		ClientMessage(
			TEXT("Position invalide : le colis ne peut pas etre pose ici."));
		return;
	}

	ServerConfirmDeliveryParcelMove(
		LocalMovedDeliveryParcel,
		LocalParcelMovePreview->GetActorLocation(),
		ParcelPlacementYaw);
	LocalParcelMovePreview->Destroy();
	LocalParcelMovePreview = nullptr;
	LocalMovedDeliveryParcel = nullptr;
	bLocalParcelPlacementValid = false;
}

void ABotanicusPlayerController::CancelDeliveryParcelPlacement()
{
	if (IsValid(LocalMovedDeliveryParcel))
	{
		ServerCancelDeliveryParcelMove(LocalMovedDeliveryParcel);
	}
	if (IsValid(LocalParcelMovePreview))
	{
		LocalParcelMovePreview->Destroy();
	}
	LocalParcelMovePreview = nullptr;
	LocalMovedDeliveryParcel = nullptr;
	bLocalParcelPlacementValid = false;
}

bool ABotanicusPlayerController::ResolveDeliveryParcelPlacement(
	const ABotanicusDeliveryParcelActor* Parcel,
	const FVector& RequestedLocation,
	float RequestedYaw,
	FTransform& OutTransform) const
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn || !IsValid(Parcel) ||
		Parcel->IsOpened() ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			RequestedLocation) >
			FMath::Square(MaximumQuickBarItemPlacementDistance))
	{
		OutTransform = FTransform(
			FRotator(0.0f, RequestedYaw, 0.0f),
			RequestedLocation);
		return false;
	}

	const FVector BoxExtent = Parcel->GetParcelHalfExtent().GetAbs();
	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusDeliveryParcelPlacementFloor),
		false);
	FloorQuery.AddIgnoredActor(ControlledPawn);
	FloorQuery.AddIgnoredActor(Parcel);
	if (IsValid(LocalParcelMovePreview))
	{
		FloorQuery.AddIgnoredActor(LocalParcelMovePreview);
	}

	const float PawnBaseZ = ControlledPawn->GetActorLocation().Z;
	const FVector TraceStart(
		RequestedLocation.X,
		RequestedLocation.Y,
		PawnBaseZ + 160.0f);
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
		PlacementLocation);
	if (!bFoundFloor || FloorHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams OverlapQuery(
		SCENE_QUERY_STAT(BotanicusDeliveryParcelPlacementOverlap),
		false);
	OverlapQuery.AddIgnoredActor(ControlledPawn);
	OverlapQuery.AddIgnoredActor(Parcel);
	if (IsValid(LocalParcelMovePreview))
	{
		OverlapQuery.AddIgnoredActor(LocalParcelMovePreview);
	}

	const FVector TestExtent(
		FMath::Max(5.0f, BoxExtent.X - 3.0f),
		FMath::Max(5.0f, BoxExtent.Y - 3.0f),
		FMath::Max(5.0f, BoxExtent.Z - 3.0f));
	TArray<FOverlapResult> Overlaps;
	return !World->OverlapMultiByObjectType(
		Overlaps,
		PlacementLocation,
		PlacementRotation,
		ObjectQuery,
		FCollisionShape::MakeBox(TestExtent),
		OverlapQuery);
}

bool ABotanicusPlayerController::TryHandleNearbyWateringCan()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!BotanicusCharacter)
	{
		return false;
	}
	if (ABotanicusWateringCanActor* HeldCan =
		BotanicusCharacter->GetHeldWateringCan())
	{
		ServerToggleWateringCan(HeldCan);
		return true;
	}

	ABotanicusWateringCanActor* NearestCan = nullptr;
	float BestDistanceSquared = FMath::Square(350.0f);
	for (TActorIterator<ABotanicusWateringCanActor> CanIt(GetWorld());
		 CanIt;
		 ++CanIt)
	{
		if (IsValid(CanIt->GetCarrier()))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			BotanicusCharacter->GetActorLocation(),
			CanIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(*CanIt, 350.0f))
		{
			BestDistanceSquared = DistanceSquared;
			NearestCan = *CanIt;
		}
	}
	if (!NearestCan)
	{
		return false;
	}
	ServerToggleWateringCan(NearestCan);
	return true;
}

bool ABotanicusPlayerController::TryRefillHeldWateringCan()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}
	const ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	const UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!QuickBar || !QuickBar->HasSelectedWateringCan())
	{
		return false;
	}

	ABotanicusWaterReserveActor* NearestReserve = nullptr;
	float BestDistanceSquared = FMath::Square(400.0f);
	for (TActorIterator<ABotanicusWaterReserveActor> ReserveIt(
			 GetWorld());
		 ReserveIt;
		 ++ReserveIt)
	{
		const float DistanceSquared = FVector::DistSquared(
			BotanicusCharacter->GetActorLocation(),
			ReserveIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(
				*ReserveIt,
				MaximumWorldInteractionDistance))
		{
			BestDistanceSquared = DistanceSquared;
			NearestReserve = *ReserveIt;
		}
	}
	if (!NearestReserve)
	{
		return false;
	}
	LocalActiveWaterReserve = NearestReserve;
	bWaterRefillActionHeld = true;
	WaterRefillRequestAccumulator = 0.0f;
	ServerRefillWateringCan(NearestReserve);
	return true;
}

void ABotanicusPlayerController::UpdateWateringCanRefill(
	float DeltaTime)
{
	if (IsLocalPlayerController() &&
		bWaterRefillActionHeld)
	{
		const ABotanicusCharacter* BotanicusCharacter =
			Cast<ABotanicusCharacter>(GetPawn());
		const UBotanicusQuickBarComponent* QuickBar =
			BotanicusCharacter
				? BotanicusCharacter->GetQuickBarComponent()
				: nullptr;
		if (!IsValid(LocalActiveWaterReserve) ||
			!QuickBar ||
			!QuickBar->HasSelectedWateringCan() ||
			QuickBar->GetSelectedWateringCanWaterLevel() >=
				1.0f - KINDA_SMALL_NUMBER ||
			!IsLookingAtWorldItem(
				LocalActiveWaterReserve,
				MaximumWorldInteractionDistance))
		{
			EndWateringCanRefill(true);
		}
		else
		{
			WaterRefillRequestAccumulator +=
				FMath::Max(0.0f, DeltaTime);
			if (WaterRefillRequestAccumulator >= 0.1f)
			{
				WaterRefillRequestAccumulator =
					FMath::Fmod(
						WaterRefillRequestAccumulator,
						0.1f);
				ServerRefillWateringCan(
					LocalActiveWaterReserve);
			}
		}
	}
}

void ABotanicusPlayerController::EndWateringCanRefill(
	bool bNotifyServer)
{
	ABotanicusWaterReserveActor* PreviousReserve =
		LocalActiveWaterReserve;
	LocalActiveWaterReserve = nullptr;
	bWaterRefillActionHeld = false;
	WaterRefillRequestAccumulator = 0.0f;
	if (bNotifyServer && IsValid(PreviousReserve))
	{
		ServerEndWateringCanRefill(PreviousReserve);
	}
}

bool ABotanicusPlayerController::TryUseNearbyComputer()
{
	if (!IsLocalPlayerController() ||
		bFurnitureMoveModeActive ||
		!GetPawn() ||
		!GetWorld())
	{
		return false;
	}

	const float ComputerInteractionDistance = MaximumWorldInteractionDistance;
	ABotanicusComputerActor* NearestComputer = nullptr;
	float BestDistanceSquared =
		FMath::Square(ComputerInteractionDistance);
	for (TActorIterator<ABotanicusComputerActor> ComputerIt(GetWorld());
		 ComputerIt;
		 ++ComputerIt)
	{
		if (ComputerIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			ComputerIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(
				*ComputerIt,
				ComputerInteractionDistance))
		{
			BestDistanceSquared = DistanceSquared;
			NearestComputer = *ComputerIt;
		}
	}

	if (!NearestComputer)
	{
		return false;
	}

	ServerUseComputer(NearestComputer);
	return true;
}

bool ABotanicusPlayerController::TryMoveNearbyPlaceableItem()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld() ||
		IsValid(LocalQuickBarItemPreview))
	{
		return false;
	}

	ABotanicusPlaceableItemActor* NearestItem = nullptr;
	const float FurnitureSelectionDistance =
		bFurnitureMoveModeActive ? 650.0f : 400.0f;
	float BestDistanceSquared =
		FMath::Square(FurnitureSelectionDistance);
	if (!bFurnitureMoveModeActive)
	{
		ABotanicusPlaceableItemActor* HighlightedItem =
			Cast<ABotanicusPlaceableItemActor>(
				LocalInteractionHighlightActor.Get());
		if (IsValid(HighlightedItem) &&
			!HighlightedItem->ActorHasTag(
				TEXT("BotanicusPlacementPreview")) &&
			!IsFurnitureActor(HighlightedItem) &&
			FVector::DistSquared(
				GetPawn()->GetActorLocation(),
				HighlightedItem->GetActorLocation()) <=
				BestDistanceSquared)
		{
			NearestItem = HighlightedItem;
			BestDistanceSquared = FVector::DistSquared(
				GetPawn()->GetActorLocation(),
				HighlightedItem->GetActorLocation());
		}
	}
	for (TActorIterator<ABotanicusPlaceableItemActor> ItemIt(GetWorld());
		 ItemIt;
		 ++ItemIt)
	{
		if (NearestItem)
		{
			break;
		}
		if (ItemIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}
		if (!bFurnitureMoveModeActive &&
			IsFurnitureActor(*ItemIt))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			ItemIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(
				*ItemIt,
				FurnitureSelectionDistance))
		{
			BestDistanceSquared = DistanceSquared;
			NearestItem = *ItemIt;
		}
	}

	if (!NearestItem)
	{
		return false;
	}

	if (bFurnitureMoveModeActive &&
		IsFurnitureActor(NearestItem))
	{
		BeginWorldItemMove(NearestItem);
	}
	else
	{
		BeginPlaceableItemMoveCharge(NearestItem);
	}
	return true;
}

void ABotanicusPlayerController::BeginPlaceableItemMoveCharge(
	ABotanicusPlaceableItemActor* WorldItem)
{
	if (!IsLocalPlayerController() ||
		!IsValid(WorldItem) ||
		IsValid(LocalPlaceableItemMoveCandidate) ||
		IsValid(LocalQuickBarItemPreview))
	{
		return;
	}

	LocalPlaceableItemMoveCandidate = WorldItem;
	PlaceableItemMoveChargeElapsed = 0.0f;
	bEquipmentCarryKeyHeld = true;
	InitializeInteractionTargetWidget();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->BeginLocalHoldProgress(
			GetMoveHoldDurationForActor(
				WorldItem,
				PlaceableItemMoveHoldDuration));
	}
	ServerBeginPlaceableItemMove(WorldItem);

	if (!CarryProgressWidget)
	{
		CarryProgressWidget =
			CreateWidget<UBotanicusCarryProgressWidget>(
				this,
				UBotanicusCarryProgressWidget::StaticClass());
		if (CarryProgressWidget)
		{
			CarryProgressWidget->AddToPlayerScreen(50);
			CarryProgressWidget->SetAlignmentInViewport(
				FVector2D(0.5f, 0.5f));
			CarryProgressWidget->SetDesiredSizeInViewport(
				FVector2D(68.0f, 68.0f));
		}
	}

	if (CarryProgressWidget)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		GetViewportSize(ViewportWidth, ViewportHeight);
		CarryProgressWidget->SetPositionInViewport(
			FVector2D(
				static_cast<float>(ViewportWidth) * 0.5f,
				static_cast<float>(ViewportHeight) * 0.5f + 85.0f),
			true);
		CarryProgressWidget->SetCarryProgress(0.0f);
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}
}

void ABotanicusPlayerController::UpdatePlaceableItemMoveCharge(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		!IsValid(LocalPlaceableItemMoveCandidate))
	{
		return;
	}

	if (!bEquipmentCarryKeyHeld ||
		!IsLookingAtWorldItem(
			LocalPlaceableItemMoveCandidate,
			450.0f))
	{
		CancelPlaceableItemMoveCharge();
		return;
	}

	PlaceableItemMoveChargeElapsed += DeltaTime;
	const float RequiredHoldDuration =
		GetMoveHoldDurationForActor(
			LocalPlaceableItemMoveCandidate,
			PlaceableItemMoveHoldDuration);
	const float ChargeProgress = FMath::Clamp(
		PlaceableItemMoveChargeElapsed /
			FMath::Max(0.1f, RequiredHoldDuration),
		0.0f,
		1.0f);
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetCarryProgress(ChargeProgress);
	}
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(ChargeProgress);
	}
	if (ChargeProgress < 1.0f)
	{
		return;
	}

	ABotanicusPlaceableItemActor* ItemToMove =
		LocalPlaceableItemMoveCandidate;
	LocalPlaceableItemMoveCandidate = nullptr;
	PlaceableItemMoveChargeElapsed = 0.0f;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}

	BeginStorageCollectionQuantitySelection(ItemToMove);
}

void ABotanicusPlayerController::CancelPlaceableItemMoveCharge()
{
	if (IsValid(LocalPlaceableItemMoveCandidate))
	{
		ServerCancelPlaceableItemMove(
			LocalPlaceableItemMoveCandidate);
	}
	LocalPlaceableItemMoveCandidate = nullptr;
	PlaceableItemMoveChargeElapsed = 0.0f;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}
}

void ABotanicusPlayerController::
	BeginStorageCollectionQuantitySelection(
		ABotanicusPlaceableItemActor* WorldItem)
{
	if (!IsLocalPlayerController() || !IsValid(WorldItem))
	{
		return;
	}
	const int32 AvailableQuantity =
		FMath::Max(1, WorldItem->GetQuantity());
	if (AvailableQuantity == 1)
	{
		if (StorageQuantityWidget)
		{
			StorageQuantityWidget->SetVisibility(
				ESlateVisibility::Collapsed);
		}
		ServerCollectStorageItem(WorldItem, 1);
		return;
	}
	LocalStorageCollectionItem = WorldItem;
	LocalStorageCollectionMaximum = AvailableQuantity;
	// Keep the former "take the stack" behavior as the fast default;
	// the wheel lets the player reduce it before validating.
	LocalStorageCollectionQuantity =
		LocalStorageCollectionMaximum;
	RefreshStorageQuantityWidget(
		false,
		WorldItem->GetItemKey(),
		LocalStorageCollectionQuantity,
		LocalStorageCollectionMaximum);
}

void ABotanicusPlayerController::
	ClientBeginCollectedItemInspection_Implementation(
		int32 SlotIndex,
		FName ItemKey)
{
	PendingCollectedItemSlotIndex = SlotIndex;
	PendingCollectedItemKey = ItemKey;
	CollectedItemInspectionRetryCount = 0;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			CollectedItemInspectionRetryTimer);
		World->GetTimerManager().SetTimer(
			CollectedItemInspectionRetryTimer,
			this,
			&ABotanicusPlayerController::
				TryBeginCollectedItemInspection,
			0.05f,
			true);
	}
	TryBeginCollectedItemInspection();
}

void ABotanicusPlayerController::
	TryBeginCollectedItemInspection()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!QuickBar ||
		PendingCollectedItemSlotIndex == INDEX_NONE ||
		PendingCollectedItemKey.IsNone())
	{
		return;
	}

	const FBotanicusQuickBarSlot Slot =
		QuickBar->GetSlot(PendingCollectedItemSlotIndex);
	if (!Slot.IsEmpty() &&
		Slot.ItemKey == PendingCollectedItemKey)
	{
		QuickBar->SelectSlot(PendingCollectedItemSlotIndex);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				CollectedItemInspectionRetryTimer);
		}
		PendingCollectedItemSlotIndex = INDEX_NONE;
		PendingCollectedItemKey = NAME_None;
		CollectedItemInspectionRetryCount = 0;
		// Picking an item only equips its newly selected hotbar slot. Ground
		// placement is an explicit second action started by pressing A.
		return;
	}

	++CollectedItemInspectionRetryCount;
	if (CollectedItemInspectionRetryCount >= 40)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				CollectedItemInspectionRetryTimer);
		}
		PendingCollectedItemSlotIndex = INDEX_NONE;
		PendingCollectedItemKey = NAME_None;
		CollectedItemInspectionRetryCount = 0;
		ClientMessage(
			TEXT(
				"L'objet est dans la hotbar, mais son slot n'a pas pu etre selectionne automatiquement."));
	}
}

void ABotanicusPlayerController::
	AdjustStorageCollectionQuantity(int32 Direction)
{
	if (!IsValid(LocalStorageCollectionItem))
	{
		return;
	}
	LocalStorageCollectionMaximum = FMath::Max(
		1,
		LocalStorageCollectionItem->GetQuantity());
	const int32 Step =
		(IsInputKeyDown(EKeys::LeftShift) ||
		 IsInputKeyDown(EKeys::RightShift))
			? 5
			: 1;
	LocalStorageCollectionQuantity = FMath::Clamp(
		LocalStorageCollectionQuantity + Direction * Step,
		1,
		LocalStorageCollectionMaximum);
	RefreshStorageQuantityWidget(
		false,
		LocalStorageCollectionItem->GetItemKey(),
		LocalStorageCollectionQuantity,
		LocalStorageCollectionMaximum);
}

void ABotanicusPlayerController::
	ConfirmStorageCollectionQuantitySelection()
{
	if (!IsValid(LocalStorageCollectionItem))
	{
		CancelStorageCollectionQuantitySelection(false);
		return;
	}
	ABotanicusPlaceableItemActor* Item =
		LocalStorageCollectionItem;
	const int32 Quantity = FMath::Clamp(
		LocalStorageCollectionQuantity,
		1,
		FMath::Max(1, Item->GetQuantity()));
	LocalStorageCollectionItem = nullptr;
	LocalStorageCollectionQuantity = 1;
	LocalStorageCollectionMaximum = 1;
	if (StorageQuantityWidget)
	{
		StorageQuantityWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	ServerCollectStorageItem(Item, Quantity);
}

void ABotanicusPlayerController::
	CancelStorageCollectionQuantitySelection(bool bNotifyServer)
{
	if (bNotifyServer && IsValid(LocalStorageCollectionItem))
	{
		ServerCancelPlaceableItemMove(
			LocalStorageCollectionItem);
	}
	LocalStorageCollectionItem = nullptr;
	LocalStorageCollectionQuantity = 1;
	LocalStorageCollectionMaximum = 1;
	if (StorageQuantityWidget)
	{
		StorageQuantityWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

void ABotanicusPlayerController::RefreshStorageQuantityWidget(
	bool bStoring,
	FName ItemKey,
	int32 Quantity,
	int32 MaximumQuantity)
{
	InitializeStorageQuantityWidget();
	if (!StorageQuantityWidget)
	{
		return;
	}
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	const FText ItemName =
		Definition
			? Definition->DisplayName
			: FText::FromName(ItemKey);
	StorageQuantityWidget->SetQuantitySelection(
		ItemName,
		Quantity,
		MaximumQuantity,
		bStoring);
}

bool ABotanicusPlayerController::TryTogglePlantInspection()
{
	ABotanicusPlantPotActor* PlantPot =
		Cast<ABotanicusPlantPotActor>(LocalInteractionHighlightActor.Get());
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar = BotanicusCharacter
		? BotanicusCharacter->GetQuickBarComponent()
		: nullptr;
	if (!IsValid(PlantPot) || PlantPot->GetPlantKey().IsNone() ||
		!QuickBar || !QuickBar->GetSelectedSlot().ItemKey.IsNone())
	{
		return false;
	}

	const bool bOpen = !PlantPot->IsInspectionVisible();
	if (LocalInspectedPlant.IsValid() && LocalInspectedPlant.Get() != PlantPot)
	{
		LocalInspectedPlant->SetInspectionVisible(false);
	}
	PlantPot->SetInspectionVisible(bOpen);
	LocalInspectedPlant = bOpen ? PlantPot : nullptr;
	return true;
}

bool ABotanicusPlayerController::TryBeginNearbyPlantPotAction()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld() ||
		IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalLargeEquipmentPlacement))
	{
		return false;
	}

	ABotanicusPlantPotActor* NearestPot = nullptr;
	float BestDistanceSquared = FMath::Square(400.0f);

	// When an interaction target is outlined, the click must belong to that
	// exact actor.  Do not let a nearby growing pot inside the broad aim cone
	// steal a click intended for a highlighted sale pot on the workbench.
	if (AActor* HighlightedActor =
			LocalInteractionHighlightActor.Get())
	{
		ABotanicusPlantPotActor* HighlightedPot =
			Cast<ABotanicusPlantPotActor>(HighlightedActor);
		if (!IsValid(HighlightedPot))
		{
			return false;
		}
		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			HighlightedPot->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(HighlightedPot, 400.0f))
		{
			NearestPot = HighlightedPot;
		}
	}
	else
	{
		for (TActorIterator<ABotanicusPlantPotActor> PotIt(GetWorld());
			 PotIt;
			 ++PotIt)
		{
			const float DistanceSquared = FVector::DistSquared(
				GetPawn()->GetActorLocation(),
				PotIt->GetActorLocation());
			if (DistanceSquared <= BestDistanceSquared &&
				IsLookingAtWorldItem(*PotIt, 400.0f))
			{
				BestDistanceSquared = DistanceSquared;
				NearestPot = *PotIt;
			}
		}
	}

	if (!NearestPot)
	{
		return false;
	}

	LocalActivePlantPot = NearestPot;
	PlantPotActionHoldElapsed = 0.0f;
	bPlantPotActionHeld = true;
	ServerBeginPlantPotAction(NearestPot);
	return true;
}

bool ABotanicusPlayerController::TryBeginNearbyParcelCut()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	const UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!IsLocalPlayerController() ||
		!BotanicusCharacter ||
		!QuickBar ||
		!GetWorld() ||
		QuickBar->GetSelectedSlot().ItemKey != TEXT("BoxCutter"))
	{
		return false;
	}

	ABotanicusDeliveryParcelActor* TargetParcel = nullptr;
	float BestDistanceSquared = FMath::Square(450.0f);
	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(
			 GetWorld());
		 ParcelIt;
		 ++ParcelIt)
	{
		if (ParcelIt->IsOpened())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			BotanicusCharacter->GetActorLocation(),
			ParcelIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(*ParcelIt, 450.0f))
		{
			BestDistanceSquared = DistanceSquared;
			TargetParcel = *ParcelIt;
		}
	}
	if (!TargetParcel)
	{
		return false;
	}

	LocalActiveParcelCut = TargetParcel;
	bParcelCutActionHeld = true;
	ServerBeginParcelCut(TargetParcel);
	return true;
}

void ABotanicusPlayerController::EndParcelCut()
{
	if (IsValid(LocalActiveParcelCut))
	{
		ServerEndParcelCut(LocalActiveParcelCut);
	}
	LocalActiveParcelCut = nullptr;
	bParcelCutActionHeld = false;
}

bool ABotanicusPlayerController::HasCarriedTransplant() const
{
	return !CarriedTransplantPlantKey.IsNone();
}

void ABotanicusPlayerController::ClearCarriedTransplant()
{
	CarriedTransplantPlantKey = NAME_None;
	CarriedTransplantItemKey = NAME_None;
	CarriedTransplantGrowth = 0.0f;
	CarriedTransplantCare = 0.0f;
	CarriedTransplantWateringCount = 0;
	bCarriedTransplantElementalDead = false;
	OnRep_CarriedTransplantState();
	ForceNetUpdate();
	if (HasAuthority())
	{
		if (ABotanicusGameMode* GameMode =
				GetWorld()
					? GetWorld()->GetAuthGameMode<ABotanicusGameMode>()
					: nullptr)
		{
			GameMode->ScheduleInventoryAutosave();
		}
	}
}

void ABotanicusPlayerController::RestoreCarriedTransplantState(
	FName PlantKey,
	FName ItemKey,
	float Growth,
	float Care,
	int32 WateringCount,
	bool bElementalDead)
{
	if (!HasAuthority())
	{
		return;
	}
	CarriedTransplantPlantKey = PlantKey;
	CarriedTransplantItemKey = PlantKey.IsNone() ? NAME_None : ItemKey;
	CarriedTransplantGrowth =
		PlantKey.IsNone() ? 0.0f : FMath::Clamp(Growth, 0.0f, 1.0f);
	CarriedTransplantCare =
		PlantKey.IsNone() ? 0.0f : FMath::Clamp(Care, 0.0f, 1.0f);
	CarriedTransplantWateringCount =
		PlantKey.IsNone() ? 0 : FMath::Max(0, WateringCount);
	bCarriedTransplantElementalDead =
		!PlantKey.IsNone() && bElementalDead;
	OnRep_CarriedTransplantState();
	ForceNetUpdate();
}

void ABotanicusPlayerController::OnRep_CarriedTransplantState()
{
	bLocalEquippedQuickBarDirty = true;
}

bool ABotanicusPlayerController::TryUseGardenTrowelForTransplant()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	const UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!IsLocalPlayerController() || !BotanicusCharacter || !QuickBar ||
		!GetWorld())
	{
		return false;
	}

	const bool bAlreadyCarryingPlant = HasCarriedTransplant();
	if (!bAlreadyCarryingPlant &&
		QuickBar->GetSelectedSlot().ItemKey != TEXT("GardenTrowel"))
	{
		return false;
	}

	AActor* TargetPot = nullptr;
	float BestDistanceSquared = FMath::Square(450.0f);
	auto ConsiderTarget =
		[this, BotanicusCharacter, bAlreadyCarryingPlant,
		 &TargetPot, &BestDistanceSquared](AActor* Candidate)
		{
			if (!IsValid(Candidate) ||
				Candidate->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
				!IsLookingAtWorldItem(Candidate, 450.0f))
			{
				return;
			}

			bool bCompatible = false;
			if (const ABotanicusPlantPotActor* PlantPot =
					Cast<ABotanicusPlantPotActor>(Candidate))
			{
				bCompatible = bAlreadyCarryingPlant
					? PlantPot->HasSoil() &&
						PlantPot->GetPlantKey().IsNone()
					: !PlantPot->GetPlantKey().IsNone();
			}
			else if (const ABotanicusSalePotActor* SalePot =
						 Cast<ABotanicusSalePotActor>(Candidate))
			{
				bCompatible = bAlreadyCarryingPlant
					? SalePot->HasSoil() && !SalePot->IsReadyForSale()
					: SalePot->IsReadyForSale();
			}
			if (!bCompatible)
			{
				return;
			}

			const float DistanceSquared = FVector::DistSquared(
				BotanicusCharacter->GetActorLocation(),
				Candidate->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				TargetPot = Candidate;
			}
		};

	for (TActorIterator<ABotanicusPlantPotActor> PotIt(GetWorld());
		 PotIt;
		 ++PotIt)
	{
		ConsiderTarget(*PotIt);
	}
	for (TActorIterator<ABotanicusSalePotActor> PotIt(GetWorld());
		 PotIt;
		 ++PotIt)
	{
		if (!PotIt->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			ConsiderTarget(*PotIt);
		}
	}

	if (!TargetPot)
	{
		if (bAlreadyCarryingPlant)
		{
			ClientMessage(
				TEXT("Regardez un pot de culture vide avec du terreau pour rempoter la plante."));
			return true;
		}
		return false;
	}

	if (!bAlreadyCarryingPlant)
	{
		if (ABotanicusPlantPotActor* PlantPot =
				Cast<ABotanicusPlantPotActor>(TargetPot))
		{
			// A mature plant is harvested by the pot's primary action. Only an
			// immature plant enters the held unpot/repot workflow.
			if (PlantPot->IsMature())
			{
				return false;
			}
			LocalTrowelTransplantTarget = PlantPot;
			GardenTrowelTransplantHoldElapsed = 0.0f;
			bTrowelTransplantActionHeld = true;
			return true;
		}
	}

	ServerUseGardenTrowelForTransplant(TargetPot);
	return true;
}

void ABotanicusPlayerController::UpdateGardenTrowelTransplantHold(
	float DeltaTime)
{
	if (!bTrowelTransplantActionHeld)
	{
		return;
	}
	ABotanicusPlantPotActor* Target = LocalTrowelTransplantTarget;
	const ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	const UBotanicusQuickBarComponent* QuickBar = BotanicusCharacter
		? BotanicusCharacter->GetQuickBarComponent()
		: nullptr;
	if (!IsValid(Target) || !QuickBar ||
		QuickBar->GetSelectedSlot().ItemKey != TEXT("GardenTrowel") ||
		Target->IsMature() || !IsLookingAtWorldItem(Target, 450.0f))
	{
		CancelGardenTrowelTransplantHold();
		return;
	}

	GardenTrowelTransplantHoldElapsed += DeltaTime;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(
			GardenTrowelTransplantHoldElapsed /
			FMath::Max(0.1f, GardenTrowelTransplantHoldDuration));
	}
	if (GardenTrowelTransplantHoldElapsed >=
		GardenTrowelTransplantHoldDuration)
	{
		ABotanicusPlantPotActor* CompletedTarget = Target;
		CancelGardenTrowelTransplantHold();
		ServerUseGardenTrowelForTransplant(CompletedTarget);
	}
}

void ABotanicusPlayerController::CancelGardenTrowelTransplantHold()
{
	bTrowelTransplantActionHeld = false;
	GardenTrowelTransplantHoldElapsed = 0.0f;
	LocalTrowelTransplantTarget = nullptr;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(0.0f);
	}
}

void ABotanicusPlayerController::EndPlantPotAction()
{
	if (IsValid(LocalActivePlantPot))
	{
		ServerEndPlantPotAction(LocalActivePlantPot);
	}
	if (IsValid(LocalActiveSalePot))
	{
		ServerEndSalePotAction(LocalActiveSalePot);
	}
	LocalActivePlantPot = nullptr;
	LocalActiveSalePot = nullptr;
	PlantPotActionHoldElapsed = 0.0f;
	bPlantPotActionHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(0.0f);
	}
}

bool ABotanicusPlayerController::TryBeginNearbySalePotAction()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	ABotanicusSalePotActor* NearestPot = nullptr;
	float BestDistanceSquared = FMath::Square(400.0f);

	// The blue outline is the player's explicit target.  This is especially
	// important on upgraded workbenches where several sale pots are close
	// enough to pass the same angular test but may be at different prep stages.
	if (AActor* HighlightedActor =
			LocalInteractionHighlightActor.Get())
	{
		ABotanicusSalePotActor* HighlightedPot =
			Cast<ABotanicusSalePotActor>(HighlightedActor);
		if (!IsValid(HighlightedPot))
		{
			return false;
		}
		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			HighlightedPot->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(HighlightedPot, 400.0f))
		{
			NearestPot = HighlightedPot;
		}
	}
	else
	{
		for (TActorIterator<ABotanicusSalePotActor> PotIt(GetWorld());
			 PotIt;
			 ++PotIt)
		{
			const float DistanceSquared = FVector::DistSquared(
				GetPawn()->GetActorLocation(),
				PotIt->GetActorLocation());
			if (DistanceSquared <= BestDistanceSquared &&
				IsLookingAtWorldItem(*PotIt, 400.0f))
			{
				BestDistanceSquared = DistanceSquared;
				NearestPot = *PotIt;
			}
		}
	}
	if (!NearestPot)
	{
		return false;
	}

	LocalActiveSalePot = NearestPot;
	bPlantPotActionHeld = true;
	ServerBeginSalePotAction(NearestPot);
	return true;
}

bool ABotanicusPlayerController::
	TryBeginDisplayedSalePotPickup()
{
	if (!IsLocalPlayerController() || !GetPawn() ||
		IsValid(LocalDisplayedSalePotPickupCandidate) ||
		IsValid(LocalPlaceableItemMoveCandidate) ||
		IsValid(LocalHeldHeavyEquipment))
	{
		return false;
	}

	ABotanicusSalesDisplayActor* SalesDisplay =
		Cast<ABotanicusSalesDisplayActor>(
			LocalInteractionHighlightActor.Get());
	if (!IsValid(SalesDisplay) ||
		!SalesDisplay->CanRetrieveDisplayedSalePot() ||
		!SalesDisplay->IsDisplayedSalePotTargeted(GetPawn()))
	{
		return false;
	}

	LocalDisplayedSalePotPickupCandidate = SalesDisplay;
	DisplayedSalePotPickupElapsed = 0.0f;
	bEquipmentCarryKeyHeld = true;
	InitializeInteractionTargetWidget();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->BeginLocalHoldProgress(
			PlaceableItemMoveHoldDuration);
	}

	if (!CarryProgressWidget)
	{
		CarryProgressWidget =
			CreateWidget<UBotanicusCarryProgressWidget>(
				this,
				UBotanicusCarryProgressWidget::StaticClass());
		if (CarryProgressWidget)
		{
			CarryProgressWidget->AddToPlayerScreen(50);
			CarryProgressWidget->SetAlignmentInViewport(
				FVector2D(0.5f, 0.5f));
			CarryProgressWidget->SetDesiredSizeInViewport(
				FVector2D(68.0f, 68.0f));
		}
	}

	if (CarryProgressWidget)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		GetViewportSize(ViewportWidth, ViewportHeight);
		CarryProgressWidget->SetPositionInViewport(
			FVector2D(
				static_cast<float>(ViewportWidth) * 0.5f,
				static_cast<float>(ViewportHeight) * 0.5f + 85.0f),
			true);
		CarryProgressWidget->SetCarryProgress(0.0f);
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}
	return true;
}

void ABotanicusPlayerController::UpdateDisplayedSalePotPickup(
	float DeltaTime)
{
	if (!IsLocalPlayerController() ||
		!IsValid(LocalDisplayedSalePotPickupCandidate))
	{
		return;
	}

	if (!bEquipmentCarryKeyHeld ||
		!LocalDisplayedSalePotPickupCandidate->
			CanRetrieveDisplayedSalePot() ||
		!LocalDisplayedSalePotPickupCandidate->
			IsDisplayedSalePotTargeted(GetPawn()))
	{
		CancelDisplayedSalePotPickup();
		return;
	}

	DisplayedSalePotPickupElapsed += DeltaTime;
	const float ChargeProgress = FMath::Clamp(
		DisplayedSalePotPickupElapsed /
			FMath::Max(0.1f, PlaceableItemMoveHoldDuration),
		0.0f,
		1.0f);
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetCarryProgress(ChargeProgress);
	}
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->SetHoldProgress(ChargeProgress);
	}
	if (ChargeProgress < 1.0f)
	{
		return;
	}

	ABotanicusSalesDisplayActor* SalesDisplay =
		LocalDisplayedSalePotPickupCandidate;
	LocalDisplayedSalePotPickupCandidate = nullptr;
	DisplayedSalePotPickupElapsed = 0.0f;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}
	ServerRetrieveDisplayedSalePot(SalesDisplay);
}

void ABotanicusPlayerController::CancelDisplayedSalePotPickup()
{
	LocalDisplayedSalePotPickupCandidate = nullptr;
	DisplayedSalePotPickupElapsed = 0.0f;
	bEquipmentCarryKeyHeld = false;
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
	if (CarryProgressWidget)
	{
		CarryProgressWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		CarryProgressWidget->SetCarryProgress(0.0f);
	}
}

bool ABotanicusPlayerController::
	TryPlaceSelectedSalePotOnWorkbench()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!IsLocalPlayerController() ||
		!BotanicusCharacter ||
		!QuickBar ||
		!GetWorld() ||
		IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalLargeEquipmentPlacement) ||
		IsValid(LocalMovedPlaceableItem))
	{
		return false;
	}

	const int32 SelectedSlotIndex =
		QuickBar->GetSelectedSlotIndex();
	const FBotanicusQuickBarSlot SelectedSlot =
		QuickBar->GetSelectedSlot();
	if (SelectedSlot.IsEmpty() ||
		!IsSalePotItemKey(SelectedSlot.ItemKey))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	// Do not use IsLookingAtWorldItem here: its generous look cone is useful
	// for interaction feedback, but at level 2+ it also catches a pot on a
	// neighbouring slot and prevents placement on the slot being aimed at.
	// Only a pot directly under the centre ray should reserve the click.
	FCollisionQueryParams PotAimQuery(
		SCENE_QUERY_STAT(BotanicusWorkbenchSalePotDirectAim),
		false);
	PotAimQuery.AddIgnoredActor(BotanicusCharacter);
	FHitResult PotAimHit;
	if (GetWorld()->LineTraceSingleByChannel(
			PotAimHit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * 400.0f,
			ECC_Visibility,
			PotAimQuery) &&
		Cast<ABotanicusSalePotActor>(PotAimHit.GetActor()))
	{
		return false;
	}

	ABotanicusPreparationWorkbenchActor* TargetWorkbench = nullptr;
	FTransform TargetTransform;
	int32 TargetSlotIndex = INDEX_NONE;
	float BestDistanceSquared = FMath::Square(600.0f);
	for (TActorIterator<ABotanicusPreparationWorkbenchActor>
			 WorkbenchIt(GetWorld());
		 WorkbenchIt;
		 ++WorkbenchIt)
	{
		if (WorkbenchIt->GetCarrier() ||
			WorkbenchIt->IsInPlacementMode() ||
			FVector::DistSquared(
				BotanicusCharacter->GetActorLocation(),
				WorkbenchIt->GetActorLocation()) >
				FMath::Square(500.0f))
		{
			continue;
		}

		FTransform CandidateTransform;
		int32 CandidateSlotIndex = INDEX_NONE;
		if (!WorkbenchIt->FindAimedAvailableSalePotSlot(
				ViewLocation,
				ViewRotation.Vector(),
				CandidateTransform,
				CandidateSlotIndex))
		{
			continue;
		}

		const float DistanceSquared =
			FVector::DistSquared(
				ViewLocation,
				CandidateTransform.GetLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			TargetWorkbench = *WorkbenchIt;
			TargetTransform = CandidateTransform;
			TargetSlotIndex = CandidateSlotIndex;
		}
	}

	if (!TargetWorkbench || TargetSlotIndex == INDEX_NONE)
	{
		return false;
	}

	ServerPlaceQuickBarItem(
		SelectedSlotIndex,
		SelectedSlot.InstanceId,
		SelectedSlot.ItemKey,
		TargetTransform.GetLocation(),
		TargetTransform.Rotator().Yaw,
		1,
		false);
	return true;
}

bool ABotanicusPlayerController::
	TryPlacePlantOnNearbySalesDisplay()
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!IsLocalPlayerController() || !BotanicusCharacter || !QuickBar ||
		!GetWorld() ||
		IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalLargeEquipmentPlacement))
	{
		return false;
	}

	const FBotanicusQuickBarSlot SelectedSlot =
		QuickBar->GetSelectedSlot();
	if (SelectedSlot.IsEmpty() ||
		!IsSalePotItemKey(SelectedSlot.ItemKey) ||
		!SelectedSlot.CarriedState.bHasSalePotState ||
		SelectedSlot.CarriedState.SaleSoilItemKey.IsNone() ||
		SelectedSlot.CarriedState.SalePlantItemKey.IsNone())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	FCollisionQueryParams Query(
		SCENE_QUERY_STAT(BotanicusSalesDisplayPlacementAim),
		false);
	Query.AddIgnoredActor(BotanicusCharacter);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * 450.0f,
			ECC_Visibility,
			Query))
	{
		return false;
	}

	ABotanicusSalesDisplayActor* TargetDisplay =
		Cast<ABotanicusSalesDisplayActor>(Hit.GetActor());
	if (!IsValid(TargetDisplay) || !TargetDisplay->IsEmpty() ||
		FVector::DistSquared(
			BotanicusCharacter->GetActorLocation(),
			TargetDisplay->GetActorLocation()) > FMath::Square(450.0f))
	{
		return false;
	}

	ServerPlacePlantOnSalesDisplay(TargetDisplay);
	return true;
}

bool ABotanicusPlayerController::TryCheckoutNearbyRegister()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld())
	{
		return false;
	}

	ABotanicusCashRegisterActor* TargetRegister = nullptr;
	float BestDistanceSquared = FMath::Square(450.0f);
	for (TActorIterator<ABotanicusCashRegisterActor> RegisterIt(
			 GetWorld());
		 RegisterIt;
		 ++RegisterIt)
	{
		if (!RegisterIt->IsOperational() ||
			!RegisterIt->GetCheckoutCustomer())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			GetPawn()->GetActorLocation(),
			RegisterIt->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared &&
			IsLookingAtWorldItem(*RegisterIt, 450.0f))
		{
			BestDistanceSquared = DistanceSquared;
			TargetRegister = *RegisterIt;
		}
	}
	if (!TargetRegister)
	{
		return false;
	}

	ServerCheckoutRegister(TargetRegister);
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

	const float EffectiveDistance = FMath::Min(
		FMath::Max(0.0f, MaximumDistance),
		MaximumWorldInteractionDistance);
	if (EffectiveDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusWorldItemLook),
		false);
	QueryParams.AddIgnoredActor(GetPawn());

	// The final workbench meshes have lips, shelves and decorations whose
	// Visibility collision can cover a correctly snapped sale pot.  Remember
	// the one workbench that actually owns the pot's slot so it can be ignored
	// for a second trace.  No other occluder is bypassed.
	const ABotanicusPreparationWorkbenchActor* SupportingWorkbench = nullptr;
	if (Cast<ABotanicusSalePotActor>(Item))
	{
		for (TActorIterator<ABotanicusPreparationWorkbenchActor> WorkbenchIt(
				 GetWorld());
			 WorkbenchIt;
			 ++WorkbenchIt)
		{
			if (WorkbenchIt->IsLocationOnPreparationSlot(
					Item->GetActorLocation(),
					55.0f))
			{
				SupportingWorkbench = *WorkbenchIt;
				break;
			}
		}
	}

	// The camera is above the pawn origin, so its ray must be longer than one
	// metre to reach a floor item that is still horizontally within one metre
	// of the player. The actual range gate is applied to the impact point.
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() *
		(EffectiveDistance + 200.0f);
	auto IsItemOrAttachedPart = [Item](const AActor* HitActor)
	{
		for (const AActor* Current = HitActor;
			 IsValid(Current);
			 Current = Current->GetAttachParentActor())
		{
			if (Current == Item)
			{
				return true;
			}
		}
		return false;
	};

	FHitResult VisibilityHit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		VisibilityHit,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
	const FVector PawnLocation = GetPawn()
		? GetPawn()->GetActorLocation()
		: ViewLocation;
	if (bHit &&
		IsItemOrAttachedPart(VisibilityHit.GetActor()) &&
		FVector::DistSquared2D(PawnLocation, VisibilityHit.ImpactPoint) <=
			FMath::Square(EffectiveDistance))
	{
		return true;
	}

	// A sale pot snapped into a workbench can be partly hidden by the
	// workbench lip. Only that known support may be ignored; every other
	// occluder still blocks interaction.
	if (!SupportingWorkbench ||
		!bHit ||
		VisibilityHit.GetActor() != SupportingWorkbench)
	{
		return false;
	}

	FCollisionQueryParams PotQueryParams = QueryParams;
	PotQueryParams.AddIgnoredActor(SupportingWorkbench);
	FHitResult PotHit;
	return GetWorld()->LineTraceSingleByChannel(
			PotHit,
			ViewLocation,
			TraceEnd,
			ECC_Visibility,
			PotQueryParams) &&
		IsItemOrAttachedPart(PotHit.GetActor()) &&
		FVector::DistSquared2D(PawnLocation, PotHit.ImpactPoint) <=
			FMath::Square(EffectiveDistance);
}

float ABotanicusPlayerController::GetMoveHoldDurationForActor(
	const AActor* Actor,
	float DefaultDuration) const
{
	return Cast<ABotanicusComputerActor>(Actor) ||
			Cast<ABotanicusCashRegisterActor>(Actor) ||
			Cast<ABotanicusPreparationWorkbenchActor>(Actor)
		? StarterFixtureMoveHoldDuration
		: DefaultDuration;
}

void ABotanicusPlayerController::TryDeletePathSegmentAtCursor()
{
	FHitResult CursorHit;
	if (!TraceTopDownCursor(CursorHit))
	{
		return;
	}

	ABotanicusPathActor* Path = nullptr;
	int32 SegmentIndex = INDEX_NONE;
	FVector ClosestPoint = FVector::ZeroVector;
	float BestDistance = ExistingPathSnapDistance;
	for (TActorIterator<ABotanicusPathActor> PathIt(GetWorld());
		 PathIt;
		 ++PathIt)
	{
		ABotanicusPathActor* Candidate = *PathIt;
		if (!IsValid(Candidate) || Candidate->IsPreviewPath())
		{
			continue;
		}

		int32 CandidateSegment = INDEX_NONE;
		FVector CandidatePoint = FVector::ZeroVector;
		float CandidateDistance = 0.0f;
		if (Candidate->FindClosestSegment(
				CursorHit.ImpactPoint,
				CandidateSegment,
				CandidatePoint,
				CandidateDistance) &&
			CandidateDistance <= BestDistance)
		{
			Path = Candidate;
			SegmentIndex = CandidateSegment;
			ClosestPoint = CandidatePoint;
			BestDistance = CandidateDistance;
		}
	}

	if (!IsValid(Path))
	{
		ClientMessage(TEXT("Cliquez pres d'une portion de route."));
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
		PathPreviewActor->SetPreviewPath(
			PendingPathPoints,
			PendingPathType);
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
		PathPreviewActor->SetPreviewPath(
			PendingPathPoints,
			PendingPathType);
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
	if (bPathStrokeActive &&
		FVector::Dist2D(PendingPathPoints.Last(), CursorPoint) >= 140.0f &&
		PendingPathPoints.Num() < 63)
	{
		PendingPathPoints.Add(CursorPoint);
	}

	TArray<FVector> PreviewPoints = PendingPathPoints;
	if (FVector::Dist2D(PreviewPoints.Last(), CursorPoint) >= 25.0f)
	{
		PreviewPoints.Add(CursorPoint);
	}
	PathPreviewActor->SetPreviewPath(
		PreviewPoints,
		PendingPathType);
}

void ABotanicusPlayerController::RefreshTopDownToolbar()
{
	if (TopDownToolbarWidget)
	{
		TopDownToolbarWidget->RefreshPathState(
			bPathPlacementActive,
			PendingPathPoints.Num() >= 2,
			bPathDeletionActive,
			PendingPathType ==
				EBotanicusPathType::VisitorRoute,
			PendingVisitorZoneType,
			bDoorEditSelectionActive ||
				bCommunicationDoorPlacementActive);
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

	if (PendingPathType == EBotanicusPathType::VisitorRoute)
	{
		const FVector RawPoint =
			CursorHit.ImpactPoint + FVector(0.0f, 0.0f, 4.0f);
		ABotanicusPathActor* ConnectedPath = nullptr;
		if (!SnapPathPoint(
				RawPoint, OutPoint, ConnectedPath, PendingPathType))
		{
			OutPoint = RawPoint;
		}
		return true;
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
	if (!SnapPathPoint(
			RawPoint, OutPoint, ConnectedPath, PendingPathType))
	{
		OutPoint = RawPoint;
	}
	return true;
}

bool ABotanicusPlayerController::SnapPathPoint(
	const FVector& RawPoint,
	FVector& OutSnappedPoint,
	ABotanicusPathActor*& OutConnectedPath,
	EBotanicusPathType DesiredPathType) const
{
	OutConnectedPath = nullptr;
	if (FindNearestBuildingEntrance(RawPoint, OutSnappedPoint))
	{
		return true;
	}

	OutConnectedPath =
		FindNearestExistingPath(
			RawPoint, OutSnappedPoint, DesiredPathType);
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
		FVector& OutPathPoint,
		EBotanicusPathType DesiredPathType) const
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
		if (!IsValid(Path) || Path->IsPreviewPath() ||
			Path->GetPathType() != DesiredPathType)
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
	if (BuildingCatalogWidget &&
		BuildingCatalogWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		return true;
	}

	if (OrderCatalogWidget &&
		OrderCatalogWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		return true;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	GetViewportSize(ViewportX, ViewportY);
	return MouseY <= 170.0f &&
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
		LocalBuildingGroup.Num() == 0 ||
		bBuildingCameraOrbitActive)
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
	if (!bHighlighted && !BuildingPreviewMaterialMeshes.IsEmpty())
	{
		int32 MaterialOffset = 0;
		for (int32 MeshIndex = 0;
			 MeshIndex < BuildingPreviewMaterialMeshes.Num();
			 ++MeshIndex)
		{
			UMeshComponent* Mesh = BuildingPreviewMaterialMeshes[MeshIndex];
			const int32 MaterialCount =
				BuildingPreviewMaterialCounts.IsValidIndex(MeshIndex)
					? BuildingPreviewMaterialCounts[MeshIndex]
					: 0;
			if (Mesh)
			{
				for (int32 Index = 0; Index < MaterialCount; ++Index)
				{
					if (BuildingPreviewOriginalMaterials.IsValidIndex(
						MaterialOffset + Index))
					{
						Mesh->SetMaterial(
							Index,
							BuildingPreviewOriginalMaterials[
								MaterialOffset + Index]);
					}
				}
				Mesh->SetOverlayMaterial(nullptr);
			}
			MaterialOffset += MaterialCount;
		}
		BuildingPreviewMaterialMeshes.Reset();
		BuildingPreviewOriginalMaterials.Reset();
		BuildingPreviewMaterialCounts.Reset();
	}

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
	UMaterialInterface* SilhouetteMaterial =
		bPlacementValid
			? ValidBuildingPlacementMaterial
			: InvalidBuildingPlacementMaterial;

	if (BuildingPreviewMaterialMeshes.IsEmpty())
	{
		for (AActor* Actor : LocalBuildingGroup)
		{
			if (!IsValid(Actor))
			{
				continue;
			}
			TInlineComponentArray<UMeshComponent*> MeshComponents(Actor);
			for (UMeshComponent* Mesh : MeshComponents)
			{
				if (!Mesh)
				{
					continue;
				}
				const int32 MaterialCount = Mesh->GetNumMaterials();
				BuildingPreviewMaterialMeshes.Add(Mesh);
				BuildingPreviewMaterialCounts.Add(MaterialCount);
				for (int32 Index = 0; Index < MaterialCount; ++Index)
				{
					BuildingPreviewOriginalMaterials.Add(Mesh->GetMaterial(Index));
				}
			}
		}
	}

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
					for (int32 Index = 0;
						 Index < Mesh->GetNumMaterials();
						 ++Index)
					{
						Mesh->SetMaterial(Index, SilhouetteMaterial);
					}
					Mesh->SetOverlayMaterial(nullptr);
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
				// A native catalogue building owns its floor, walls and roof
				// in one actor. Ignoring the owner would make the complete
				// building unselectable while its roof is hidden.
				QueryParams.AddIgnoredComponent(
					RoofComponent.Get());
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

void ABotanicusPlayerController::ServerPurchaseCatalogBuilding_Implementation(
	FName BuildingKey)
{
	UWorld* World = GetWorld();
	if (!World || ServerBuildingGroup.Num() > 0 || BuildingKey.IsNone())
	{
		return;
	}
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Server received building purchase %s with %d credits."),
		*BuildingKey.ToString(),
		GetAvailableFunds());

	const FBotanicusBuildingDefinition* Definition =
		FindBuildingDefinition(this, BuildingKey);
	if (!Definition || !Definition->bUnlockedByDefault)
	{
		ClientMessage(TEXT("Ce bâtiment n'est pas disponible."));
		UE_LOG(
			LogBotanicus,
			Warning,
			TEXT("Rejected building purchase for unknown or locked key %s."),
			*BuildingKey.ToString());
		return;
	}
	const int32 RequiredLevel =
		FMath::Max(1, Definition->RequiredDevelopmentLevel);
	if (BuildingProgressionLevel < RequiredLevel)
	{
		ClientMessage(
			*FString::Printf(
				TEXT("Bâtiment verrouillé : niveau %d requis."),
				RequiredLevel));
		UE_LOG(
			LogBotanicus,
			Display,
			TEXT("Rejected building purchase %s: level %d/%d."),
			*BuildingKey.ToString(),
			BuildingProgressionLevel,
			RequiredLevel);
		return;
	}
	const int32 Price = FMath::Max(0, Definition->Price);
	if (GetAvailableFunds() < Price)
	{
		ClientMessage(TEXT("Crédits insuffisants pour ce bâtiment."));
		return;
	}

	AActor* TemplateSeed = nullptr;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Candidate = *ActorIt;
		if (IsStructuralBuildingActor(Candidate) &&
			!Candidate->ActorHasTag(PlayerPurchasedBuildingTag) &&
			Candidate->ActorHasTag(Definition->TemplateTag))
		{
			TemplateSeed = Candidate;
			break;
		}
	}

	// Older versions of the test map spawn their EBS references at runtime
	// and therefore cannot carry saved actor tags. Keep those maps usable by
	// resolving distinct complete groups in proximity order.
	const bool bSupportsLegacyBuildingTemplate =
		BuildingKey == TEXT("GreenhouseCompact") ||
		BuildingKey == TEXT("GreenhouseWorkshop");
	if (!TemplateSeed && bSupportsLegacyBuildingTemplate)
	{
		TSet<TWeakObjectPtr<AActor>> VisitedTemplateActors;
		TArray<TPair<AActor*, float>> LegacyTemplateSeeds;
		const FVector ReferenceLocation =
			GetPawn()
				? GetPawn()->GetActorLocation()
				: FVector::ZeroVector;
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Candidate = *ActorIt;
			if (!IsStructuralBuildingActor(Candidate) ||
				Candidate->ActorHasTag(PlayerPurchasedBuildingTag) ||
				VisitedTemplateActors.Contains(Candidate))
			{
				continue;
			}

			const TArray<AActor*> CandidateGroup =
				BuildCompleteBuildingGroup(Candidate);
			if (CandidateGroup.Num() == 0)
			{
				continue;
			}
			for (AActor* TemplateGroupActor : CandidateGroup)
			{
				VisitedTemplateActors.Add(TemplateGroupActor);
			}
			const float DistanceSquared = FVector::DistSquared2D(
				ReferenceLocation,
				CalculateBuildingGroupPivot(CandidateGroup));
			LegacyTemplateSeeds.Emplace(Candidate, DistanceSquared);
		}

		LegacyTemplateSeeds.Sort(
			[](const TPair<AActor*, float>& A,
			   const TPair<AActor*, float>& B)
			{
				return A.Value < B.Value;
			});
		if (LegacyTemplateSeeds.IsValidIndex(
				Definition->LegacyTemplateGroupIndex))
		{
			TemplateSeed =
				LegacyTemplateSeeds[
					Definition->LegacyTemplateGroupIndex].Key;
		}
	}

	const TArray<AActor*> TemplateGroup =
		BuildCompleteBuildingGroup(TemplateSeed);

	TSubclassOf<ABotanicusCatalogBuildingActor> FallbackPrefabClass =
		Definition->FallbackPrefabClass.LoadSynchronous();
	if (!FallbackPrefabClass)
	{
		FallbackPrefabClass =
			BuildingKey == TEXT("GreenhouseWorkshop")
				? ABotanicusWorkshopGreenhouseActor::StaticClass()
				: ABotanicusCompactGreenhouseActor::StaticClass();
	}
	if (TemplateGroup.Num() == 0 && !FallbackPrefabClass)
	{
		ClientMessage(
			TEXT("Aucun modèle disponible pour ce bâtiment."));
		UE_LOG(
			LogBotanicus,
			Error,
			TEXT("Building purchase %s has neither an EBS template nor a prefab."),
			*BuildingKey.ToString());
		return;
	}

	const FVector InitialOffset = Definition->PreviewOffset;
	if (!TrySpendSharedFunds(this, Price))
	{
		ClientMessage(TEXT("Crédits insuffisants pour ce bâtiment."));
		return;
	}
	ServerPendingBuildingPurchasePrice = Price;
	ServerPendingBuildingPurchaseKey = BuildingKey;
	ForceNetUpdate();
	OnRep_OrderState();
	TArray<AActor*> PurchasedGroup;
	FVector PurchasedPivot = FVector::ZeroVector;
	if (TemplateGroup.Num() == 0)
	{
		FVector PrefabSpawnLocation =
			GetPawn()
				? GetPawn()->GetActorLocation() +
					FVector(900.0f, 0.0f, 0.0f)
				: FVector::ZeroVector;
		float LandscapeHeight = PrefabSpawnLocation.Z;
		if (FindLandscapeHeight(
				FVector2D(
					PrefabSpawnLocation.X,
					PrefabSpawnLocation.Y),
				LandscapeHeight))
		{
			PrefabSpawnLocation.Z = LandscapeHeight;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABotanicusCatalogBuildingActor* PurchasedPrefab =
			World->SpawnActor<ABotanicusCatalogBuildingActor>(
			FallbackPrefabClass,
			PrefabSpawnLocation,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (PurchasedPrefab)
		{
			PurchasedPrefab->Tags.AddUnique(PlayerPurchasedBuildingTag);
			ConfigurePurchasedActorForNetworking(PurchasedPrefab);
			PurchasedGroup.Add(PurchasedPrefab);
			PurchasedPivot = PrefabSpawnLocation;
		}
	}
	else
	{
		PurchasedGroup.Reserve(TemplateGroup.Num());
		PurchasedPivot =
			CalculateBuildingGroupPivot(TemplateGroup) + InitialOffset;
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
				RefundPendingBuildingPurchase();
				ClientMessage(TEXT("Impossible de créer le bâtiment."));
				return;
			}

			PurchasedActor->Tags = TemplateActor->Tags;
			PurchasedActor->Tags.Remove(Definition->TemplateTag);
			PurchasedActor->Tags.AddUnique(PlayerPurchasedBuildingTag);
			ConfigurePurchasedActorForNetworking(PurchasedActor);
			PurchasedGroup.Add(PurchasedActor);
		}
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
		RefundPendingBuildingPurchase();
		ClientMessage(TEXT("Impossible de réserver ce bâtiment."));
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

	ServerBuildingOriginalPivot = PurchasedPivot;
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
		*FString::Printf(
			TEXT("%s acheté pour %d crédits : choisissez son emplacement."),
			*Definition->DisplayName.ToString(),
			Price));
	BroadcastPurchasedBuildingSnapshot(true);
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Server purchased building %s for %d credits with %d replicated actors for %s."),
		*BuildingKey.ToString(),
		Price,
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

	const FName TestItemKey(TEXT("SeedPacket_AureliaSweet"));
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, TestItemKey);
	Parcel->InitializeParcel(
		TestItemKey,
		Definition
			? FMath::Max(1, Definition->DeliveryQuantity)
			: 10);
	Parcel->SetActorLocation(
		Parcel->GetActorLocation() +
		FVector(0.0f, 0.0f, Parcel->GetParcelHalfExtent().Z));
	ClientMessage(
		TEXT(
			"Commande livrée : quittez la vue top-down et récupérez le colis avec E."));
}

void ABotanicusPlayerController::
	ServerOrderTestLargeEquipment_Implementation()
{
	SpawnTestLargeEquipment(TEXT("LargeEquipment_Test"));
}

void ABotanicusPlayerController::
	ServerOrderTestSoloEquipment_Implementation()
{
	SpawnTestLargeEquipment(TEXT("LargeEquipmentSolo_Test"));
}

void ABotanicusPlayerController::SpawnTestLargeEquipment(
	FName ItemKey)
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

	Equipment->InitializeEquipment(ItemKey);
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	const bool bNeedsTwoPlayers =
		Definition &&
		Definition->WeightClass ==
			EBotanicusItemWeightClass::TwoPlayerCarry;
	ClientMessage(
		bNeedsTwoPlayers
			? TEXT(
				  "Objet lourd livre : deux joueurs doivent maintenir E pour le soulever.")
			: TEXT(
				  "Objet lourd solo livre : maintenez E pour le soulever."));
}

void ABotanicusPlayerController::ServerPlaceCatalogOrder_Implementation(
	FName ItemKey)
{
	if (ItemKey.IsNone() || !GetWorld() || !GetPawn())
	{
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	if (!Definition || !Definition->bPurchasable)
	{
		ClientMessage(TEXT("Commande refusee : article indisponible."));
		return;
	}
	if (!CanOrderCatalogItem(ItemKey))
	{
		if (ItemKey == TEXT("SelfCheckout") &&
			GetMainShopLevel() < 3)
		{
			ClientMessage(
				TEXT(
					"Commande refusee : la caisse automatique se debloque au niveau 3."));
		}
		else
		{
			ClientMessage(
				TEXT(
					"Commande refusee : limite de caisses automatiques atteinte."));
		}
		return;
	}

	const int32 Price = FMath::Max(0, Definition->Price);
	if (GetAvailableFunds() < Price)
	{
		ClientMessage(TEXT("Fonds insuffisants pour cette commande."));
		return;
	}

	if (!TrySpendSharedFunds(this, Price))
	{
		ClientMessage(TEXT("Fonds insuffisants pour cette commande."));
		return;
	}
	FBotanicusPendingOrder& Order =
		PendingOrders.AddDefaulted_GetRef();
	Order.OrderId = FGuid::NewGuid();
	Order.ItemKey = ItemKey;
	Order.DisplayName =
		Definition->DisplayName.IsEmpty()
			? FText::FromName(ItemKey)
			: Definition->DisplayName;
	Order.Quantity = FMath::Max(1, Definition->DeliveryQuantity);
	Order.ChargedPrice = Price;
	const float Delay =
		FMath::Max(0.1f, Definition->DeliveryDelaySeconds);
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float ServerTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds();
	Order.DeliveryServerTime = ServerTime + Delay;
	if (ABotanicusGameState* BotanicusGameState =
		GetSharedGameState(this))
	{
		BotanicusGameState->RecordCatalogOrder();
	}

	FTimerDelegate DeliveryDelegate;
	DeliveryDelegate.BindUObject(
		this,
		&ABotanicusPlayerController::CompleteCatalogOrder,
		Order.OrderId);
	FTimerHandle& Timer = PendingOrderTimers.Add(Order.OrderId);
	GetWorldTimerManager().SetTimer(
		Timer,
		DeliveryDelegate,
		Delay,
		false);
	ForceNetUpdate();
	OnRep_OrderState();
	ClientMessage(
		*FString::Printf(
			TEXT("Commande confirmee : livraison dans %.1f secondes."),
			Delay));
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Accepted catalogue order %s for %s: price=%d, remaining funds=%d, delay=%.1f."),
		*ItemKey.ToString(),
		PlayerState ? *PlayerState->GetPlayerName() : TEXT("UnknownPlayer"),
		Price,
		GetAvailableFunds(),
		Delay);
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::ServerUpgradeMainShop_Implementation()
{
	ABotanicusGameState* GameState = GetSharedGameState(this);
	if (!GameState)
	{
		return;
	}
	if (!GameState->AreMainShopUpgradeTasksComplete())
	{
		ClientMessage(
			TEXT("Amelioration refusee : terminez toutes les taches."));
		return;
	}
	if (GameState->GetSharedFunds() <
		GameState->GetMainShopUpgradeCost())
	{
		ClientMessage(
			TEXT("Amelioration refusee : credits insuffisants."));
		return;
	}
	if (!GameState->TryUpgradeMainShop())
	{
		return;
	}

	ClientMessage(
		*FString::Printf(
			TEXT(
				"Boutique principale niveau %d : capacite %d visiteurs."),
			GameState->GetMainShopLevel(),
			GameState->GetMainShopVisitorCapacity()));
	OnRep_OrderState();
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::
	ServerUpgradePreparationWorkbench_Implementation()
{
	ABotanicusPreparationWorkbenchActor* Workbench =
		FindPrimaryPreparationWorkbench(this);
	if (!Workbench || !Workbench->CanUpgrade())
	{
		ClientMessage(
			TEXT(
				"L'atelier de preparation est deja au niveau maximum."));
		return;
	}

	const int32 UpgradeCost = Workbench->GetUpgradeCost();
	if (UpgradeCost <= 0 ||
		!TrySpendSharedFunds(this, UpgradeCost))
	{
		ClientMessage(
			TEXT(
				"Credits communs insuffisants pour ameliorer l'atelier."));
		return;
	}
	if (!Workbench->UpgradeWorkbench())
	{
		AddSharedFunds(this, UpgradeCost);
		return;
	}

	ClientMessage(
		*FString::Printf(
			TEXT(
				"Atelier niveau %d : %d pots peuvent etre prepares en meme temps."),
			Workbench->GetWorkbenchLevel(),
			Workbench->GetSlotCount()));
	OnRep_OrderState();
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::
	ServerUseWorkbenchUpgradeTerminal_Implementation(
		ABotanicusPreparationWorkbenchActor* Workbench)
{
	if (!IsValid(Workbench) ||
		!Workbench->CanAccessUpgradeTerminal(GetPawn()) ||
		!Workbench->IsUpgradeTerminalTargeted(GetPawn()))
	{
		return;
	}

	ClientOpenPreparationWorkbenchUpgrade(Workbench);
}

void ABotanicusPlayerController::
	ServerSetPreparationWorkbenchLevel_Implementation(
		ABotanicusPreparationWorkbenchActor* Workbench,
		int32 TargetLevel)
{
	if (!IsValid(Workbench) ||
		!Workbench->CanAccessUpgradeTerminal(GetPawn()) ||
		TargetLevel < 1 ||
		TargetLevel > 5)
	{
		return;
	}

	const int32 CurrentLevel = Workbench->GetWorkbenchLevel();
	if (TargetLevel == CurrentLevel)
	{
		return;
	}
	if (!Workbench->CanChangeWorkbenchLevel(TargetLevel))
	{
		ClientMessage(
			TEXT(
				"Downgrade impossible : retirez d'abord les pots des emplacements qui vont disparaitre."));
		return;
	}

	const int32 TransitionCost =
		Workbench->GetLevelTransitionCost(TargetLevel);
	if (TransitionCost > 0 &&
		!TrySpendSharedFunds(this, TransitionCost))
	{
		ClientMessage(
			TEXT(
				"Credits communs insuffisants pour cette amelioration."));
		return;
	}

	if (!Workbench->SetWorkbenchLevel(TargetLevel))
	{
		if (TransitionCost > 0)
		{
			AddSharedFunds(this, TransitionCost);
		}
		return;
	}

	if (TransitionCost < 0)
	{
		AddSharedFunds(this, -TransitionCost);
	}

	const FString ResultMessage =
		TargetLevel > CurrentLevel
			? FString::Printf(
				TEXT(
					"Atelier ameliore au niveau %d pour %d credits."),
				TargetLevel,
				FMath::Abs(TransitionCost))
			: FString::Printf(
				TEXT(
					"Atelier ramene au niveau %d : %d credits rembourses."),
				TargetLevel,
				FMath::Abs(TransitionCost));
	ClientMessage(ResultMessage);
	OnRep_OrderState();
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::ServerSetMainShopOpen_Implementation(
	bool bOpen)
{
	ABotanicusGameState* GameState = GetSharedGameState(this);
	if (!GameState || GameState->IsMainShopOpen() == bOpen)
	{
		return;
	}

	GameState->SetMainShopOpen(bOpen);
	OnRep_OrderState();
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::ServerAddTestCredits_Implementation()
{
	constexpr int32 TestCreditAmount = 100;
	AddSharedFunds(this, TestCreditAmount);
	OnRep_OrderState();
	ClientMessage(TEXT("Test : +100 crédits ajoutés à la caisse commune."));
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::
	ServerSetDevelopmentTimeScale_Implementation(float TimeScale)
{
	ABotanicusGameState* GameState = GetSharedGameState(this);
	if (!GameState)
	{
		return;
	}
	GameState->SetDevelopmentTimeScale(TimeScale);
	ClientMessage(
		*FString::Printf(
			TEXT("Debug : vitesse de simulation x%.0f."),
			GameState->GetDevelopmentTimeScale()));
}

void ABotanicusPlayerController::
	ServerAdjustMainShopLevelForDevelopment_Implementation(
		int32 Delta)
{
	ABotanicusGameState* GameState = GetSharedGameState(this);
	if (!GameState)
	{
		return;
	}
	const int32 NewLevel =
		FMath::Clamp(
			GameState->GetMainShopLevel() +
				FMath::Clamp(Delta, -1, 1),
			1,
			99);
	GameState->SetMainShopLevelForDevelopment(NewLevel);
	OnRep_OrderState();
	ClientMessage(
		*FString::Printf(
			TEXT("Debug : boutique passee au niveau %d."),
			NewLevel));
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::ServerUseComputer_Implementation(
	ABotanicusComputerActor* Computer)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(Computer) ||
		!ControlledPawn ||
		!IsLookingAtWorldItem(Computer, MaximumWorldInteractionDistance))
	{
		ClientMessage(
			TEXT(
				"Regardez l'ordinateur et rapprochez-vous pour l'utiliser."));
		return;
	}
	Computer->Interact_Implementation(ControlledPawn);
}

ABotanicusDeliveryZoneActor*
ABotanicusPlayerController::FindOrCreateDeliveryZone()
{
	check(HasAuthority());
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn)
	{
		return nullptr;
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
	if (DeliveryZone)
	{
		return DeliveryZone;
	}

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

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return World->SpawnActor<ABotanicusDeliveryZoneActor>(
		ZoneLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
}

bool ABotanicusPlayerController::DeliverCatalogItem(
	FName ItemKey,
	int32 Quantity)
{
	check(HasAuthority());
	UWorld* World = GetWorld();
	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	ABotanicusDeliveryZoneActor* DeliveryZone =
		FindOrCreateDeliveryZone();
	if (!World || !Definition || !DeliveryZone)
	{
		return false;
	}

	int32 DeliveryIndex = 0;
	for (TActorIterator<ABotanicusDeliveryParcelActor> ParcelIt(World);
		 ParcelIt;
		 ++ParcelIt)
	{
		if (FVector::DistSquared2D(
				ParcelIt->GetActorLocation(),
				DeliveryZone->GetActorLocation()) <
			FMath::Square(700.0f))
		{
			++DeliveryIndex;
		}
	}
	for (TActorIterator<ABotanicusPlaceableItemActor> ItemIt(World);
		 ItemIt;
		 ++ItemIt)
	{
		if (!ItemIt->ActorHasTag(TEXT("BotanicusPlacementPreview")) &&
			FVector::DistSquared2D(
				ItemIt->GetActorLocation(),
				DeliveryZone->GetActorLocation()) <
				FMath::Square(700.0f))
		{
			++DeliveryIndex;
		}
	}
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
			++DeliveryIndex;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector DeliverySpawnLocation =
		DeliveryZone->GetParcelSpawnLocation(DeliveryIndex);
	ABotanicusDeliveryParcelActor* Parcel =
		World->SpawnActor<ABotanicusDeliveryParcelActor>(
			DeliverySpawnLocation,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!Parcel)
	{
		return false;
	}
	Parcel->InitializeParcel(ItemKey, FMath::Max(1, Quantity));
	Parcel->SetActorLocation(
		DeliverySpawnLocation +
		FVector(0.0f, 0.0f, Parcel->GetParcelHalfExtent().Z));
	return true;
}

void ABotanicusPlayerController::CompleteCatalogOrder(
	FGuid OrderId)
{
	check(HasAuthority());
	const int32 OrderIndex = PendingOrders.IndexOfByPredicate(
		[&OrderId](const FBotanicusPendingOrder& Order)
		{
			return Order.OrderId == OrderId;
		});
	if (OrderIndex == INDEX_NONE)
	{
		PendingOrderTimers.Remove(OrderId);
		return;
	}

	// A reconnect can restore the queue before its pawn has spawned. Keep the
	// completed order pending briefly instead of treating that lifecycle gap
	// as a delivery failure and refunding it.
	if (!GetPawn())
	{
		constexpr float PawnRetryDelay = 1.0f;
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const float ServerTime = GameState
			? GameState->GetServerWorldTimeSeconds()
			: GetWorld()->GetTimeSeconds();
		PendingOrders[OrderIndex].DeliveryServerTime =
			ServerTime + PawnRetryDelay;
		FTimerDelegate RetryDelegate;
		RetryDelegate.BindUObject(
			this,
			&ABotanicusPlayerController::CompleteCatalogOrder,
			OrderId);
		FTimerHandle& RetryTimer =
			PendingOrderTimers.FindOrAdd(OrderId);
		GetWorldTimerManager().SetTimer(
			RetryTimer,
			RetryDelegate,
			PawnRetryDelay,
			false);
		ForceNetUpdate();
		OnRep_OrderState();
		return;
	}

	const FBotanicusPendingOrder Order = PendingOrders[OrderIndex];
	const bool bDelivered =
		DeliverCatalogItem(Order.ItemKey, Order.Quantity);
	PendingOrders.RemoveAt(OrderIndex);
	PendingOrderTimers.Remove(OrderId);
	if (!bDelivered)
	{
		AddSharedFunds(this, Order.ChargedPrice);
		ClientMessage(
			TEXT("Livraison impossible : commande remboursee."));
	}
	else
	{
		ClientMessage(
			TEXT("Commande livree sur la zone de livraison."));
	}
	ForceNetUpdate();
	OnRep_OrderState();
	ScheduleSharedStateAutosave(this);
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

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, Equipment->GetItemKey());
	if (!Definition ||
		Definition->WeightClass !=
			EBotanicusItemWeightClass::OnePlayerCarry)
	{
		ClientMessage(
			TEXT("Cet equipement ne peut pas etre porte seul."));
		return;
	}

	if (IsFurnitureActor(Equipment) &&
		!bServerFurnitureMoveModeActive)
	{
		ClientMessage(
			TEXT(
				"Activez le mode meubles avec B pour deplacer ce meuble."));
		return;
	}

	ABotanicusStorageShelfActor* StorageShelf =
		Cast<ABotanicusStorageShelfActor>(Equipment);
	if (StorageShelf &&
		StorageShelf->HasStoredItems() &&
		!bServerFurnitureMoveModeActive)
	{
		ClientMessage(
			TEXT(
				"Activez le mode meubles avec B pour deplacer cette etagere avec son contenu."));
		return;
	}
	if (StorageShelf)
	{
		StorageShelf->SetMoveContentsWithFurniture(
			bServerFurnitureMoveModeActive);
	}
	ABotanicusPreparationWorkbenchActor* Workbench =
		Cast<ABotanicusPreparationWorkbenchActor>(Equipment);
	if (Workbench &&
		Workbench->HasPreparedPots() &&
		!bServerFurnitureMoveModeActive)
	{
		ClientMessage(
			TEXT(
				"Activez le mode meubles avec B pour deplacer cet etabli avec ses pots."));
		return;
	}
	if (Workbench)
	{
		Workbench->SetMoveContentsWithFurniture(
			bServerFurnitureMoveModeActive);
	}
	ABotanicusWorkSurfaceActor* WorkSurface =
		Cast<ABotanicusWorkSurfaceActor>(Equipment);
	if (WorkSurface &&
		WorkSurface->HasSurfaceContents() &&
		!bServerFurnitureMoveModeActive)
	{
		ClientMessage(
			TEXT(
				"Activez le mode meubles avec B pour deplacer ce plan de travail avec son contenu."));
		return;
	}
	if (WorkSurface)
	{
		WorkSurface->SetMoveContentsWithFurniture(
			bServerFurnitureMoveModeActive);
	}

	const bool bAlreadyCarried =
		Equipment->GetCarrier() == ControlledPawn;
	if (bAlreadyCarried)
	{
		return;
	}

	const float ServerInteractionDistance =
		bServerFurnitureMoveModeActive &&
			IsFurnitureActor(Equipment)
			? 700.0f
			: 500.0f;
	if (!IsLookingAtWorldItem(
			Equipment,
			ServerInteractionDistance))
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
	else if (StorageShelf)
	{
		StorageShelf->SetMoveContentsWithFurniture(false);
	}
	else if (Workbench)
	{
		Workbench->SetMoveContentsWithFurniture(false);
	}
	else if (WorkSurface)
	{
		WorkSurface->SetMoveContentsWithFurniture(false);
	}
}

void ABotanicusPlayerController::
	ServerSetFurnitureMoveMode_Implementation(bool bEnabled)
{
	bServerFurnitureMoveModeActive = bEnabled;
}

void ABotanicusPlayerController::
	ServerSetHeavyEquipmentHold_Implementation(
		ABotanicusLargeEquipmentActor* Equipment,
		bool bHeld)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!IsValid(Equipment) || !BotanicusCharacter)
	{
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, Equipment->GetItemKey());
	if (!Definition ||
		Definition->WeightClass !=
			EBotanicusItemWeightClass::TwoPlayerCarry)
	{
		return;
	}

	if (!bHeld)
	{
		ABotanicusCharacter* Primary = Equipment->GetCarrier();
		ABotanicusCharacter* Secondary = Equipment->GetHelper();
		if (BotanicusCharacter != Primary &&
			BotanicusCharacter != Secondary)
		{
			return;
		}

		ABotanicusPlayerController* PrimaryController =
			Primary
				? Cast<ABotanicusPlayerController>(
					  Primary->GetController())
				: nullptr;
		ABotanicusPlayerController* SecondaryController =
			Secondary
				? Cast<ABotanicusPlayerController>(
					  Secondary->GetController())
				: nullptr;

		Equipment->EndCooperativeHold(BotanicusCharacter);
		if (PrimaryController)
		{
			PrimaryController->ClientCancelHeavyEquipmentPlacement(
				Equipment);
		}
		if (SecondaryController &&
			SecondaryController != PrimaryController)
		{
			SecondaryController->ClientCancelHeavyEquipmentPlacement(
				Equipment);
		}
		return;
	}

	if (BotanicusCharacter != Equipment->GetCarrier() &&
		BotanicusCharacter != Equipment->GetHelper() &&
		!IsLookingAtWorldItem(Equipment, 500.0f))
	{
		ClientMessage(
			TEXT(
				"Regardez l'equipement lourd et restez a proximite."));
		return;
	}

	for (TActorIterator<ABotanicusLargeEquipmentActor> EquipmentIt(
			 GetWorld());
		 EquipmentIt;
		 ++EquipmentIt)
	{
		if (*EquipmentIt != Equipment &&
			(EquipmentIt->GetCarrier() == BotanicusCharacter ||
			 EquipmentIt->GetHelper() == BotanicusCharacter))
		{
			ClientMessage(
				TEXT("Vous participez deja au portage d'un equipement."));
			return;
		}
	}

	Equipment->BeginCooperativeHold(BotanicusCharacter);
	if (Equipment->GetCarrier() == BotanicusCharacter &&
		!IsValid(Equipment->GetHelper()))
	{
		ClientMessage(
			TEXT(
				"Maintenez E : un second joueur doit maintenir E sur l'equipement."));
		return;
	}

	if (IsValid(Equipment->GetCarrier()) &&
		IsValid(Equipment->GetHelper()) &&
		Equipment->IsInPlacementMode())
	{
		ABotanicusPlayerController* PrimaryController =
			Cast<ABotanicusPlayerController>(
				Equipment->GetCarrier()->GetController());
		if (PrimaryController)
		{
			PrimaryController->ClientBeginLargeEquipmentPlacement(
				Equipment);
			PrimaryController->ClientMessage(
				TEXT(
					"Portage cooperatif actif : gardez E maintenu et placez l'equipement."));
		}
		ClientMessage(
			TEXT(
				"Vous aidez au portage : gardez E maintenu et restez proche."));
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

	if (Equipment->RequiresTwoPlayers())
	{
		if (!IsValid(Equipment->GetHelper()) ||
			FVector::DistSquared(
				Equipment->GetCarrier()->GetActorLocation(),
				Equipment->GetHelper()->GetActorLocation()) >
				FMath::Square(700.0f))
		{
			Equipment->EndCooperativeHold(
				Equipment->GetCarrier());
			return;
		}
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

	if (Equipment->RequiresTwoPlayers() &&
		(!IsValid(Equipment->GetHelper()) ||
		 FVector::DistSquared(
			 Equipment->GetCarrier()->GetActorLocation(),
			 Equipment->GetHelper()->GetActorLocation()) >
			 FMath::Square(700.0f)))
	{
		Equipment->EndCooperativeHold(
			Equipment->GetCarrier());
		ClientCancelHeavyEquipmentPlacement(Equipment);
		return;
	}

	FTransform PlacementTransform;
	const bool bIsValid = ResolveLargeEquipmentPlacement(
		Equipment,
		FVector(RequestedLocation),
		RequestedYaw,
		PlacementTransform);
	Equipment->UpdatePlacement(PlacementTransform, bIsValid);
	if (bIsValid)
	{
		Equipment->ConfirmPlacement();
		if (ABotanicusGameMode* GameMode =
				GetWorld()->GetAuthGameMode<
					ABotanicusGameMode>())
		{
			GameMode->ScheduleInventoryAutosave();
		}
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
		if (Equipment->RequiresTwoPlayers())
		{
			Equipment->EndCooperativeHold(
				Cast<ABotanicusCharacter>(GetPawn()));
		}
		else
		{
			Equipment->CancelPlacement();
		}
		ClientCancelHeavyEquipmentPlacement(Equipment);
	}
}

void ABotanicusPlayerController::
	ClientBeginLargeEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	BeginLargeEquipmentPlacement(Equipment);
}

void ABotanicusPlayerController::
	ClientCancelHeavyEquipmentPlacement_Implementation(
		ABotanicusLargeEquipmentActor* Equipment)
{
	if (LocalLargeEquipmentPlacement == Equipment ||
		LocalHeldHeavyEquipment == Equipment)
	{
		CancelEquipmentCarryCharge(false);
	}
}

void ABotanicusPlayerController::
	ServerPlaceQuickBarItem_Implementation(
		int32 SlotIndex,
		FGuid InstanceId,
		FName ItemKey,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw,
		int32 Quantity,
		bool bFloorOnlyPlacement)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	UWorld* World = GetWorld();
	if (!QuickBar || !World)
	{
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	const bool bCanPlaceOnFloor =
		Definition && Definition->CanBePlacedOn(
			EBotanicusPlacementSurface::Floor);
	const bool bCanPlaceOnFurniture =
		ABotanicusStorageShelfActor::IsCatalogItemCompatible(
			this,
			ItemKey) ||
		ABotanicusWorkSurfaceActor::IsCatalogItemCompatible(
			this,
			ItemKey);
	if (!Definition ||
		Definition->WeightClass !=
			EBotanicusItemWeightClass::Hotbar ||
		(bFloorOnlyPlacement
			? !bCanPlaceOnFloor
			: !bCanPlaceOnFloor && !bCanPlaceOnFurniture))
	{
		ClientMessage(
			TEXT("Placement refuse : definition de catalogue incompatible."));
		return;
	}

	const FBotanicusQuickBarSlot Slot = QuickBar->GetSlot(SlotIndex);
	if (Slot.IsEmpty() ||
		Slot.ItemKey != ItemKey ||
		Slot.InstanceId != InstanceId ||
		Quantity <= 0 ||
		Quantity > Slot.Quantity)
	{
		ClientMessage(
			TEXT("Placement refuse : le contenu du slot a change."));
		return;
	}

	FTransform PlacementTransform;
	if (!ResolveQuickBarItemPlacement(
			ItemKey,
			FVector(RequestedLocation),
			RequestedYaw,
			PlacementTransform,
			nullptr,
			Quantity,
			bFloorOnlyPlacement))
	{
		ClientMessage(
			TEXT("Placement refuse : position invalide."));
		return;
	}

	const FVector StorageItemExtent =
		!Definition->CollisionHalfExtentOverride.IsNearlyZero()
			? Definition->CollisionHalfExtentOverride.GetAbs()
			: Definition->WorldScale.GetAbs() * 50.0f;
	ABotanicusStorageShelfActor* DestinationShelf = nullptr;
	int32 DestinationSlotIndex = INDEX_NONE;
	ABotanicusPlaceableItemActor* ExistingStack = nullptr;
	const bool bStorageDestination =
		!bFloorOnlyPlacement &&
		FindStorageDestinationAtLocation(
			ItemKey,
			Quantity,
			StorageItemExtent,
			PlacementTransform.GetLocation(),
			nullptr,
			DestinationShelf,
			DestinationSlotIndex,
			ExistingStack);
	const bool bValidGroundSoilStack =
		!bStorageDestination &&
		ItemKey == TEXT("PottingSoil") &&
		Quantity <= PottingSoilGroundStackLimit;
	if (!bStorageDestination && Quantity != 1 &&
		!bValidGroundSoilStack)
	{
		ClientMessage(
			TEXT(
				"Placement refuse : les piles doivent etre rangees sur une etagere."));
		return;
	}
	if (bStorageDestination &&
		IsValid(ExistingStack))
	{
		if (!QuickBar->RemoveQuantity(SlotIndex, Quantity))
		{
			ClientMessage(
				TEXT(
					"Placement refuse : quantite insuffisante."));
			return;
		}
		const int32 NewQuantity =
			ExistingStack->GetQuantity() + Quantity;
		ExistingStack->InitializePlacedItem(
			ItemKey,
			NewQuantity);
		ExistingStack->SetNetDormancy(DORM_Awake);
		ExistingStack->FlushNetDormancy();
		ExistingStack->ForceNetUpdate();
		ClientMessage(
			*FString::Printf(
				TEXT("Pile rangee : x%d/%d."),
				NewQuantity,
				ABotanicusStorageShelfActor::
					GetStorageStackLimit(ItemKey)));
		if (ABotanicusGameMode* GameMode =
				GetWorld()->GetAuthGameMode<
					ABotanicusGameMode>())
		{
			GameMode->ScheduleInventoryAutosave();
		}
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* PlacedItemClass =
		Definition->WorldActorClass.LoadSynchronous();
	if (!PlacedItemClass ||
		!PlacedItemClass->IsChildOf(
			ABotanicusPlaceableItemActor::StaticClass()))
	{
		PlacedItemClass =
			ABotanicusPlaceableItemActor::StaticClass();
	}
	ABotanicusPlaceableItemActor* PlacedItem =
		World->SpawnActor<ABotanicusPlaceableItemActor>(
			PlacedItemClass,
			PlacementTransform,
			SpawnParameters);
	if (!PlacedItem)
	{
		ClientMessage(
			TEXT("Placement refuse : impossible de creer l'objet."));
		return;
	}

	PlacedItem->InitializePlacedItem(ItemKey, Quantity);
	ApplyCarriedItemState(Slot, PlacedItem);
	if (!QuickBar->RemoveQuantity(SlotIndex, Quantity))
	{
		PlacedItem->Destroy();
		ClientMessage(
			TEXT("Placement refuse : quantite insuffisante."));
		return;
	}

	bool bMountedOnSalesDisplay = false;
	if (!bFloorOnlyPlacement)
	{
	if (ABotanicusSalePotActor* SalePot =
			Cast<ABotanicusSalePotActor>(PlacedItem);
		IsValid(SalePot) && SalePot->IsReadyForSale())
	{
		for (TActorIterator<ABotanicusSalesDisplayActor> DisplayIt(
				 World);
			 DisplayIt;
			 ++DisplayIt)
		{
			const FVector DisplayLocation =
				DisplayIt->GetSalePotPlacementTransform().
					GetLocation();
			const FVector PotLocation =
				PlacementTransform.GetLocation();
			if (DisplayIt->IsEmpty() &&
				FVector::DistSquared2D(
					PotLocation,
					DisplayLocation) <=
					FMath::Square(170.0f) &&
				FMath::Abs(PotLocation.Z - DisplayLocation.Z) <=
					180.0f &&
				DisplayIt->TryMountSalePot(SalePot, this))
			{
				bMountedOnSalesDisplay = true;
				break;
			}
		}
	}
	}

	if (!bMountedOnSalesDisplay && IsValid(PlacedItem))
	{
		PlacedItem->SetOwner(nullptr);
		PlacedItem->SetNetDormancy(DORM_Awake);
		PlacedItem->FlushNetDormancy();
		PlacedItem->ForceNetUpdate();
	}
	if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::
	ServerThrowQuickBarItem_Implementation(
		int32 SlotIndex,
		FGuid InstanceId,
		FName ItemKey,
		FVector_NetQuantizeNormal ThrowDirection,
		float HoldDuration)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	UWorld* World = GetWorld();
	if (!BotanicusCharacter || !QuickBar || !World)
	{
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, ItemKey);
	const bool bShelfCompatible =
		ABotanicusStorageShelfActor::IsCatalogItemCompatible(
			this,
			ItemKey);
	const bool bWorkSurfaceCompatible =
		ABotanicusWorkSurfaceActor::IsCatalogItemCompatible(
			this,
			ItemKey);
	if (!Definition ||
		Definition->WeightClass !=
			EBotanicusItemWeightClass::Hotbar ||
		(!Definition->CanBePlacedOn(
			 EBotanicusPlacementSurface::Floor) &&
		 !bShelfCompatible &&
		 !bWorkSurfaceCompatible))
	{
		ClientMessage(
			TEXT("Lancer refuse : objet incompatible."));
		return;
	}

	const FBotanicusQuickBarSlot Slot =
		QuickBar->GetSlot(SlotIndex);
	if (Slot.IsEmpty() ||
		Slot.ItemKey != ItemKey ||
		Slot.InstanceId != InstanceId)
	{
		ClientMessage(
			TEXT("Lancer refuse : le contenu du slot a change."));
		return;
	}

	FVector Direction(ThrowDirection);
	if (Direction.ContainsNaN() || !Direction.Normalize())
	{
		ClientMessage(TEXT("Lancer refuse : direction invalide."));
		return;
	}

	const float ClampedHoldDuration = FMath::Clamp(
		HoldDuration,
		QuickBarThrowHoldThreshold,
		MaximumQuickBarThrowHoldDuration);
	const float ChargeAlpha = FMath::GetRangePct(
		FVector2D(
			QuickBarThrowHoldThreshold,
			MaximumQuickBarThrowHoldDuration),
		ClampedHoldDuration);
	const float ThrowSpeed = FMath::Lerp(
		MinimumQuickBarThrowSpeed,
		MaximumQuickBarThrowSpeed,
		FMath::Clamp(ChargeAlpha, 0.0f, 1.0f));

	UClass* ThrownItemClass =
		Definition->WorldActorClass.LoadSynchronous();
	if (!ThrownItemClass ||
		!ThrownItemClass->IsChildOf(
			ABotanicusPlaceableItemActor::StaticClass()))
	{
		ThrownItemClass =
			ABotanicusPlaceableItemActor::StaticClass();
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	BotanicusCharacter->GetActorEyesViewPoint(
		EyeLocation,
		EyeRotation);
	const FVector ThrownItemSpawnLocation =
		EyeLocation + Direction * 100.0f;
	const FTransform SpawnTransform(
		Direction.Rotation(),
		ThrownItemSpawnLocation);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusPlaceableItemActor* ThrownItem =
		World->SpawnActor<ABotanicusPlaceableItemActor>(
			ThrownItemClass,
			SpawnTransform,
			SpawnParameters);
	if (!ThrownItem)
	{
		ClientMessage(
			TEXT("Lancer refuse : impossible de creer l'objet."));
		return;
	}

	ThrownItem->InitializePlacedItem(ItemKey, 1);
	ApplyCarriedItemState(Slot, ThrownItem);
	if (!QuickBar->RemoveQuantity(SlotIndex, 1))
	{
		ThrownItem->Destroy();
		ClientMessage(
			TEXT("Lancer refuse : quantite insuffisante."));
		return;
	}

	ThrownItem->SetOwner(nullptr);
	ThrownItem->LaunchItem(Direction * ThrowSpeed);
}

void ABotanicusPlayerController::
	ServerBeginPlaceableItemMove_Implementation(
		ABotanicusPlaceableItemActor* WorldItem)
{
	if (IsValid(WorldItem) &&
		IsFurnitureActor(WorldItem) &&
		!bServerFurnitureMoveModeActive)
	{
		ClientMessage(
			TEXT(
				"Activez le mode meubles avec B pour deplacer ce meuble."));
		ClientEndPlaceableItemHold();
		return;
	}

	const float ServerInteractionDistance =
		bServerFurnitureMoveModeActive &&
			IsFurnitureActor(WorldItem)
			? 700.0f
			: 450.0f;
	if (!IsValid(WorldItem) ||
		WorldItem->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
		!IsLookingAtWorldItem(
			WorldItem,
			ServerInteractionDistance))
	{
		ClientMessage(
			TEXT("Regardez l'objet et rapprochez-vous pour le deplacer."));
		ClientEndPlaceableItemHold();
		return;
	}

	for (TActorIterator<ABotanicusPlayerController> ControllerIt(
			 GetWorld());
		 ControllerIt;
		 ++ControllerIt)
	{
		if (*ControllerIt != this &&
			ControllerIt->ServerMovedPlaceableItem == WorldItem)
		{
			ClientMessage(
				TEXT(
					"Cet objet est deja pris par un autre joueur."));
			ClientEndPlaceableItemHold();
			return;
		}
	}

	ServerMovedPlaceableItem = WorldItem;
	ClientBeginPlaceableItemHold(
		GetMoveHoldDurationForActor(
			WorldItem,
			PlaceableItemMoveHoldDuration));
}

void ABotanicusPlayerController::
	ClientBeginPlaceableItemHold_Implementation(float DurationSeconds)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	InitializeHudLayoutWidget();
	if (HudLayoutWidget && HudLayoutWidget->GetInteractionWidget())
	{
		InteractionTargetWidget =
			HudLayoutWidget->GetInteractionWidget();
	}
	InitializeInteractionTargetWidget();
	if (InteractionTargetWidget)
	{
		InteractionTargetWidget->BeginLocalHoldProgress(DurationSeconds);
	}
}

void ABotanicusPlayerController::
	ClientEndPlaceableItemHold_Implementation()
{
	if (IsLocalPlayerController() && InteractionTargetWidget)
	{
		InteractionTargetWidget->EndLocalHoldProgress();
	}
}

void ABotanicusPlayerController::
	ServerCollectStorageItem_Implementation(
		ABotanicusPlaceableItemActor* WorldItem,
		int32 Quantity)
{
	ClientEndPlaceableItemHold();
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!IsValid(WorldItem) ||
		!QuickBar ||
		ServerMovedPlaceableItem != WorldItem ||
		!IsLookingAtWorldItem(WorldItem, 450.0f) ||
		Quantity <= 0 ||
		Quantity > WorldItem->GetQuantity())
	{
		ClientMessage(
			TEXT(
				"Impossible de prendre cet objet dans l'etagere."));
		ServerMovedPlaceableItem = nullptr;
		return;
	}

	const FBotanicusItemDefinition* Definition =
		FindItemDefinition(this, WorldItem->GetItemKey());
	if (!Definition ||
		Definition->WeightClass !=
			EBotanicusItemWeightClass::Hotbar)
	{
		ClientMessage(
			TEXT(
				"Cet objet ne peut pas aller dans la hotbar."));
		ServerMovedPlaceableItem = nullptr;
		return;
	}

	int32 AddedSlotIndex = INDEX_NONE;
	FBotanicusCarriedItemState CarriedState;
	if (const ABotanicusMultiPlantPotActor* MultiPlanter =
		Cast<ABotanicusMultiPlantPotActor>(WorldItem))
	{
		CarriedState.bHasMultiPlanterState = true;
		CarriedState.MultiPlanterSoilUnits =
			MultiPlanter->GetSoilUnits();
		for (const FBotanicusMultiPlantSlotState& Slot :
			MultiPlanter->GetPlantSlots())
		{
			CarriedState.MultiPlanterPlantKeys.Add(
				Slot.PlantKey);
			CarriedState.MultiPlanterWaterLevels.Add(
				Slot.WaterLevel);
			CarriedState.MultiPlanterGrowthProgress.Add(
				Slot.GrowthProgress);
			CarriedState.MultiPlanterCareScores.Add(
				Slot.CareScore);
			CarriedState.MultiPlanterWateringCounts.Add(
				Slot.WateringCount);
		}
	}
	else if (const ABotanicusPlantPotActor* PlantPot =
		Cast<ABotanicusPlantPotActor>(WorldItem))
	{
		CarriedState.bHasPlantPotState = true;
		CarriedState.bPlantPotHasSoil = PlantPot->HasSoil();
		CarriedState.PlantKey = PlantPot->GetPlantKey();
		CarriedState.WaterLevel = PlantPot->GetWaterLevel();
		CarriedState.GrowthProgress =
			PlantPot->GetGrowthProgress();
		CarriedState.CareScore = PlantPot->GetCareScore();
		CarriedState.WateringCount =
			PlantPot->GetWateringCount();
	}
	else if (const ABotanicusSalePotActor* SalePot =
		Cast<ABotanicusSalePotActor>(WorldItem))
	{
		CarriedState.bHasSalePotState = true;
		CarriedState.SaleSoilItemKey =
			SalePot->GetSoilItemKey();
		CarriedState.SalePlantItemKey =
			SalePot->GetPlantItemKey();
	}
	else if (const ABotanicusWateringCanActor* WateringCan =
		Cast<ABotanicusWateringCanActor>(WorldItem))
	{
		CarriedState.bHasWateringCanState = true;
		CarriedState.WateringCanWaterLevel =
			WateringCan->GetWaterLevel();
	}

	if (!QuickBar->AddItem(
			WorldItem->GetItemKey(),
			Quantity,
			AddedSlotIndex))
	{
		ClientMessage(
			TEXT(
				"Hotbar pleine : liberez assez de place avant de prendre cette pile."));
		ServerMovedPlaceableItem = nullptr;
		return;
	}
	QuickBar->SetCarriedItemState(
		AddedSlotIndex,
		CarriedState);

	const FString ItemName =
		Definition->DisplayName.ToString();
	const FName CollectedItemKey =
		WorldItem->GetItemKey();
	const int32 RemainingQuantity =
		WorldItem->GetQuantity() - Quantity;
	ServerMovedPlaceableItem = nullptr;
	if (RemainingQuantity <= 0)
	{
		WorldItem->Destroy();
	}
	else
	{
		WorldItem->InitializePlacedItem(
			WorldItem->GetItemKey(),
			RemainingQuantity);
		WorldItem->SetNetDormancy(DORM_Awake);
		WorldItem->FlushNetDormancy();
		WorldItem->ForceNetUpdate();
	}
	QuickBar->SelectSlotAuthoritative(AddedSlotIndex);
	ClientBeginCollectedItemInspection(
		AddedSlotIndex,
		CollectedItemKey);
	ClientMessage(
		*FString::Printf(
			TEXT("%s x%d ajoute a la hotbar."),
			*ItemName,
			Quantity));
	if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::
	ServerConfirmPlaceableItemMove_Implementation(
		ABotanicusPlaceableItemActor* WorldItem,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw)
{
	if (!IsValid(WorldItem) ||
		ServerMovedPlaceableItem != WorldItem)
	{
		ClientMessage(TEXT("Deplacement refuse : objet non reserve."));
		return;
	}

	FTransform PlacementTransform;
	if (!ResolveQuickBarItemPlacement(
			WorldItem->GetItemKey(),
			FVector(RequestedLocation),
			RequestedYaw,
			PlacementTransform,
			WorldItem,
			1,
			true))
	{
		ClientMessage(TEXT("Deplacement refuse : position invalide."));
		ServerMovedPlaceableItem = nullptr;
		return;
	}

	if (ABotanicusSalePotActor* SalePot =
		Cast<ABotanicusSalePotActor>(WorldItem))
	{
		for (TActorIterator<ABotanicusSalesDisplayActor> DisplayIt(
				 GetWorld());
			 DisplayIt;
			 ++DisplayIt)
		{
			const FVector DisplayLocation =
				DisplayIt->GetSalePotPlacementTransform().
					GetLocation();
			const FVector PotLocation =
				PlacementTransform.GetLocation();
			if (DisplayIt->IsEmpty() &&
				FVector::DistSquared2D(
					PotLocation,
					DisplayLocation) <=
					FMath::Square(170.0f) &&
				FMath::Abs(PotLocation.Z - DisplayLocation.Z) <=
					180.0f &&
				DisplayIt->TryMountSalePot(SalePot, this))
			{
				ServerMovedPlaceableItem = nullptr;
				return;
			}
		}
	}

	ABotanicusStorageShelfActor* DestinationShelf = nullptr;
	int32 DestinationSlotIndex = INDEX_NONE;
	ABotanicusPlaceableItemActor* ExistingStack = nullptr;
	if (FindStorageDestinationAtLocation(
			WorldItem->GetItemKey(),
			WorldItem->GetQuantity(),
			WorldItem->GetPlacementBoxExtent().GetAbs(),
			PlacementTransform.GetLocation(),
			WorldItem,
			DestinationShelf,
			DestinationSlotIndex,
			ExistingStack) &&
		IsValid(ExistingStack) &&
		ExistingStack != WorldItem)
	{
		const int32 NewQuantity =
			ExistingStack->GetQuantity() +
			WorldItem->GetQuantity();
		ExistingStack->InitializePlacedItem(
			WorldItem->GetItemKey(),
			NewQuantity);
		ExistingStack->SetNetDormancy(DORM_Awake);
		ExistingStack->FlushNetDormancy();
		ExistingStack->ForceNetUpdate();
		ServerMovedPlaceableItem = nullptr;
		WorldItem->Destroy();
		ClientMessage(
			*FString::Printf(
				TEXT("Piles regroupees : x%d/%d."),
				NewQuantity,
				ABotanicusStorageShelfActor::
					GetStorageStackLimit(
						ExistingStack->GetItemKey())));
		if (ABotanicusGameMode* GameMode =
				GetWorld()->GetAuthGameMode<
					ABotanicusGameMode>())
		{
			GameMode->ScheduleInventoryAutosave();
		}
		return;
	}

	ABotanicusCashRegisterActor* CashRegister =
		Cast<ABotanicusCashRegisterActor>(WorldItem);
	TArray<ABotanicusSelfCheckoutActor*> MountedCheckouts;
	TArray<int32> MountedCheckoutSlots;
	if (CashRegister)
	{
		for (TActorIterator<ABotanicusSelfCheckoutActor>
				 CheckoutIt(GetWorld());
			 CheckoutIt;
			 ++CheckoutIt)
		{
			const int32 SlotIndex =
				CashRegister->FindSelfCheckoutSlotIndex(
					CheckoutIt->GetActorLocation());
			if (SlotIndex != INDEX_NONE)
			{
				MountedCheckouts.Add(*CheckoutIt);
				MountedCheckoutSlots.Add(SlotIndex);
			}
		}
	}

	WorldItem->SetActorTransform(
		PlacementTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (CashRegister)
	{
		for (int32 MountedIndex = 0;
			 MountedCheckouts.IsValidIndex(MountedIndex) &&
			 MountedCheckoutSlots.IsValidIndex(MountedIndex);
			 ++MountedIndex)
		{
			if (IsValid(MountedCheckouts[MountedIndex]))
			{
				MountedCheckouts[MountedIndex]->SetActorTransform(
					CashRegister->
						GetSelfCheckoutSlotTransform(
							MountedCheckoutSlots[MountedIndex]),
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
				MountedCheckouts[MountedIndex]->ForceNetUpdate();
			}
		}
	}
	WorldItem->SetNetDormancy(DORM_Awake);
	WorldItem->FlushNetDormancy();
	WorldItem->ForceNetUpdate();
	ServerMovedPlaceableItem = nullptr;
	if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::
	ServerCancelPlaceableItemMove_Implementation(
		ABotanicusPlaceableItemActor* WorldItem)
{
	ClientEndPlaceableItemHold();
	if (ServerMovedPlaceableItem == WorldItem)
	{
		ServerMovedPlaceableItem = nullptr;
	}
}

void ABotanicusPlayerController::
	ServerBeginDeliveryParcelMove_Implementation(
		ABotanicusDeliveryParcelActor* Parcel)
{
	if (!IsValid(Parcel) ||
		Parcel->IsOpened() ||
		Parcel->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
		!IsLookingAtWorldItem(Parcel, 450.0f))
	{
		ClientMessage(
			TEXT(
				"Regardez le colis ferme et rapprochez-vous pour le deplacer."));
		return;
	}

	for (TActorIterator<ABotanicusPlayerController> ControllerIt(
			 GetWorld());
		 ControllerIt;
		 ++ControllerIt)
	{
		if (*ControllerIt != this &&
			ControllerIt->ServerMovedDeliveryParcel == Parcel)
		{
			ClientMessage(
				TEXT("Ce colis est deja deplace par un autre joueur."));
			return;
		}
	}

	Parcel->EndCutting(GetPawn());
	ServerMovedDeliveryParcel = Parcel;
}

void ABotanicusPlayerController::
	ServerConfirmDeliveryParcelMove_Implementation(
		ABotanicusDeliveryParcelActor* Parcel,
		FVector_NetQuantize10 RequestedLocation,
		float RequestedYaw)
{
	if (!IsValid(Parcel) ||
		Parcel->IsOpened() ||
		ServerMovedDeliveryParcel != Parcel)
	{
		ClientMessage(TEXT("Deplacement refuse : colis non reserve."));
		return;
	}

	FTransform PlacementTransform;
	if (!ResolveDeliveryParcelPlacement(
			Parcel,
			FVector(RequestedLocation),
			RequestedYaw,
			PlacementTransform))
	{
		ClientMessage(
			TEXT("Deplacement refuse : position du colis invalide."));
		return;
	}

	Parcel->SetActorTransform(
		PlacementTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Parcel->SetNetDormancy(DORM_Awake);
	Parcel->FlushNetDormancy();
	Parcel->ForceNetUpdate();
	ServerMovedDeliveryParcel = nullptr;

	if (ABotanicusGameMode* GameMode =
			GetWorld()
				? GetWorld()->GetAuthGameMode<ABotanicusGameMode>()
				: nullptr)
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::
	ServerCancelDeliveryParcelMove_Implementation(
		ABotanicusDeliveryParcelActor* Parcel)
{
	if (ServerMovedDeliveryParcel == Parcel)
	{
		ServerMovedDeliveryParcel = nullptr;
	}
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
	ServerBeginParcelCut_Implementation(
		ABotanicusDeliveryParcelActor* Parcel)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(Parcel) ||
		!ControlledPawn ||
		!IsLookingAtWorldItem(Parcel, 450.0f))
	{
		return;
	}
	Parcel->BeginCutting(ControlledPawn);
}

void ABotanicusPlayerController::
	ServerEndParcelCut_Implementation(
		ABotanicusDeliveryParcelActor* Parcel)
{
	if (IsValid(Parcel))
	{
		Parcel->EndCutting(GetPawn());
	}
}

void ABotanicusPlayerController::
	ServerToggleWateringCan_Implementation(
		ABotanicusWateringCanActor* WateringCan)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	if (!BotanicusCharacter || !IsValid(WateringCan))
	{
		return;
	}

	if (BotanicusCharacter->GetHeldWateringCan() == WateringCan)
	{
		WateringCan->Drop();
		ClientMessage(TEXT("Arrosoir pose."));
		return;
	}
	if (IsValid(BotanicusCharacter->GetHeldWateringCan()) ||
		IsValid(WateringCan->GetCarrier()) ||
		!IsLookingAtWorldItem(WateringCan, 400.0f))
	{
		ClientMessage(
			TEXT("Regardez l'arrosoir et rapprochez-vous pour le prendre."));
		return;
	}
	if (WateringCan->TryPickUp(BotanicusCharacter))
	{
		ClientMessage(
			FString::Printf(
				TEXT("Arrosoir en main - eau : %d%%."),
				FMath::RoundToInt(
					WateringCan->GetWaterLevel() * 100.0f)));
	}
}

void ABotanicusPlayerController::
	ServerRefillWateringCan_Implementation(
		ABotanicusWaterReserveActor* WaterReserve)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!BotanicusCharacter || !QuickBar ||
		!QuickBar->HasSelectedWateringCan())
	{
		ClientMessage(
			TEXT(
				"Remplissage impossible : selectionnez l'arrosoir dans la hotbar."));
		return;
	}
	if (!IsLookingAtWorldItem(
			WaterReserve,
			MaximumWorldInteractionDistance))
	{
		ClientMessage(
			TEXT(
				"Remplissage impossible : regardez la reserve et rapprochez-vous."));
		return;
	}
	if (QuickBar->GetSelectedWateringCanWaterLevel() >=
		1.0f - KINDA_SMALL_NUMBER)
	{
		ClientMessage(TEXT("L'arrosoir est deja plein."));
		return;
	}
	ServerActiveWaterReserve = WaterReserve;
	const double CurrentServerTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double ElapsedSincePulse =
		CurrentServerTime - LastServerWaterRefillPulseTime;
	if (ElapsedSincePulse < 0.075)
	{
		return;
	}

	const float RefillDeltaTime =
		LastServerWaterRefillPulseTime < -100.0
			? 0.1f
			: FMath::Clamp(
				static_cast<float>(ElapsedSincePulse),
				0.075f,
				0.15f);
	LastServerWaterRefillPulseTime = CurrentServerTime;
	constexpr float WaterRefillPerSecond = 0.40f;
	if (WaterReserve->TryRefill(
			BotanicusCharacter,
			WaterRefillPerSecond * RefillDeltaTime))
	{
		bServerWaterRefillChanged = true;
		if (QuickBar->GetSelectedWateringCanWaterLevel() >=
			1.0f - KINDA_SMALL_NUMBER)
		{
			ClientMessage(TEXT("Arrosoir rempli : eau 100%."));
		}
	}
}

void ABotanicusPlayerController::
	ServerEndWateringCanRefill_Implementation(
		ABotanicusWaterReserveActor* WaterReserve)
{
	if (ServerActiveWaterReserve != WaterReserve)
	{
		return;
	}
	if (bServerWaterRefillChanged)
	{
		if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
		{
			GameMode->ScheduleInventoryAutosave();
		}
	}
	ServerActiveWaterReserve = nullptr;
	bServerWaterRefillChanged = false;
	LastServerWaterRefillPulseTime = -1000.0;
}

void ABotanicusPlayerController::
	ServerBeginPlantPotAction_Implementation(
		ABotanicusPlantPotActor* PlantPot)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(PlantPot) || !ControlledPawn ||
		!IsLookingAtWorldItem(PlantPot, 450.0f))
	{
		ClientMessage(
			TEXT(
				"Regardez le pot et rapprochez-vous pour l'utiliser."));
		UE_LOG(
			LogBotanicus,
			Warning,
			TEXT("Plant pot primary action rejected by server validation."));
		return;
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Plant pot primary action started for %s."),
		*GetNameSafe(ControlledPawn));
	PlantPot->BeginPrimaryUse(ControlledPawn);
}

void ABotanicusPlayerController::
	ServerEndPlantPotAction_Implementation(
		ABotanicusPlantPotActor* PlantPot)
{
	if (IsValid(PlantPot))
	{
		PlantPot->EndPrimaryUse(GetPawn());
	}
}

void ABotanicusPlayerController::
	ServerBeginSalePotAction_Implementation(
		ABotanicusSalePotActor* SalePot)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(SalePot) || !ControlledPawn ||
		!IsLookingAtWorldItem(SalePot, 450.0f))
	{
		return;
	}
	SalePot->BeginPrimaryUse(ControlledPawn);
}

void ABotanicusPlayerController::
ServerEndSalePotAction_Implementation(
		ABotanicusSalePotActor* SalePot)
{
	if (IsValid(SalePot))
	{
		SalePot->EndPrimaryUse(GetPawn());
	}
}

void ABotanicusPlayerController::
ServerUseGardenTrowelForTransplant_Implementation(AActor* TargetPot)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!BotanicusCharacter || !QuickBar || !IsValid(TargetPot) ||
		!IsLookingAtWorldItem(TargetPot, 450.0f))
	{
		ClientMessage(
			TEXT("Regardez le pot et rapprochez-vous pour rempoter."));
		return;
	}

	UBotanicusPlantSubsystem* Plants =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	if (!Plants)
	{
		return;
	}

	if (HasCarriedTransplant())
	{
		if (ABotanicusPlantPotActor* PlantPot =
				Cast<ABotanicusPlantPotActor>(TargetPot))
		{
			if (!PlantPot->HasSoil() ||
				!PlantPot->GetPlantKey().IsNone())
			{
				ClientMessage(
					TEXT("Ce pot doit etre vide et contenir du terreau."));
				return;
			}

			const float TargetWaterLevel = PlantPot->GetWaterLevel();
			PlantPot->RestoreGrowingState(
				true,
				CarriedTransplantPlantKey,
				TargetWaterLevel,
				CarriedTransplantGrowth,
				CarriedTransplantCare,
				bCarriedTransplantElementalDead);
			PlantPot->RestoreWateringCount(
				CarriedTransplantWateringCount);
			ClearCarriedTransplant();
			ClientMessage(TEXT("Plante rempotee dans le pot de culture."));
		}
		else if (ABotanicusSalePotActor* SalePot =
					 Cast<ABotanicusSalePotActor>(TargetPot))
		{
			if (!SalePot->HasSoil() || SalePot->IsReadyForSale())
			{
				ClientMessage(
					TEXT("Ce pot de vente doit etre vide et contenir du terreau."));
				return;
			}
			if (CarriedTransplantGrowth < 0.999f ||
				bCarriedTransplantElementalDead)
			{
				ClientMessage(
					TEXT("Cette plante doit finir de pousser dans un pot de culture."));
				return;
			}

			SalePot->RestoreSalePotState(
				SalePot->GetSoilItemKey(),
				CarriedTransplantItemKey);
			ClearCarriedTransplant();
			ClientMessage(TEXT("Plante rempotee dans le pot de vente."));
		}
		return;
	}

	if (QuickBar->GetSelectedSlot().ItemKey != TEXT("GardenTrowel"))
	{
		return;
	}

	if (ABotanicusPlantPotActor* PlantPot =
			Cast<ABotanicusPlantPotActor>(TargetPot))
	{
		if (PlantPot->IsMature())
		{
			PlantPot->BeginPrimaryUse(BotanicusCharacter);
			return;
		}
		const FName PlantKey = PlantPot->GetPlantKey();
		const FBotanicusPlantDefinition* Definition =
			Plants->FindPlant(PlantKey);
		if (PlantKey.IsNone() || !Definition)
		{
			return;
		}

		// A newly planted seed starts at 2% as its visual seed stage. Until
		// it progresses beyond that value it can safely return to its packet.
		if (PlantPot->GetGrowthProgress() <= 0.0201f)
		{
			int32 AddedSlotIndex = INDEX_NONE;
			if (!QuickBar->AddItem(
					Definition->SeedItemKey,
					1,
					AddedSlotIndex))
			{
				ClientMessage(
					TEXT("Hotbar pleine : impossible de recuperer la graine."));
				return;
			}

			PlantPot->RestoreGrowingState(
				PlantPot->HasSoil(),
				NAME_None,
				PlantPot->GetWaterLevel(),
				0.0f,
				0.0f);
			PlantPot->RestoreWateringCount(0);
			ClientMessage(
				TEXT("Graine recuperee et rangee dans la hotbar."));
			return;
		}

		CarriedTransplantPlantKey = PlantKey;
		CarriedTransplantItemKey = PlantPot->GetCurrentHarvestItemKey();
		if (CarriedTransplantItemKey.IsNone())
		{
			CarriedTransplantItemKey = Definition->HarvestItemKey;
		}
		CarriedTransplantGrowth = PlantPot->GetGrowthProgress();
		CarriedTransplantCare = PlantPot->GetCareScore();
		CarriedTransplantWateringCount = PlantPot->GetWateringCount();
		bCarriedTransplantElementalDead = PlantPot->IsElementalDead();
		PlantPot->RestoreGrowingState(
			PlantPot->HasSoil(),
			NAME_None,
			PlantPot->GetWaterLevel(),
			0.0f,
			0.0f);
		PlantPot->RestoreWateringCount(0);
		OnRep_CarriedTransplantState();
		ForceNetUpdate();
		ClientMessage(
			TEXT("Plante depotee : rempotez-la obligatoirement dans un autre pot."));
	}
	else if (ABotanicusSalePotActor* SalePot =
				 Cast<ABotanicusSalePotActor>(TargetPot))
	{
		const FName PlantItemKey = SalePot->GetPlantItemKey();
		const FBotanicusPlantDefinition* Definition =
			Plants->FindPlantByHarvestItem(PlantItemKey);
		if (PlantItemKey.IsNone() || !Definition)
		{
			return;
		}

		CarriedTransplantPlantKey = Definition->PlantKey;
		CarriedTransplantItemKey = PlantItemKey;
		CarriedTransplantGrowth = 1.0f;
		CarriedTransplantCare = 1.0f;
		CarriedTransplantWateringCount = 0;
		bCarriedTransplantElementalDead = false;
		SalePot->RestoreSalePotState(
			SalePot->GetSoilItemKey(),
			NAME_None);
		OnRep_CarriedTransplantState();
		ForceNetUpdate();
		ClientMessage(
			TEXT("Plante retiree du pot de vente : rempotez-la dans un autre pot."));
	}

	if (ABotanicusGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::
	ServerRetrieveDisplayedSalePot_Implementation(
		ABotanicusSalesDisplayActor* SalesDisplay)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(SalesDisplay) || !ControlledPawn ||
		!SalesDisplay->CanRetrieveDisplayedSalePot() ||
		!SalesDisplay->IsDisplayedSalePotTargeted(ControlledPawn) ||
		!SalesDisplay->TryRetrieveDisplayedSalePot(ControlledPawn))
	{
		ClientMessage(
			TEXT(
				"Impossible de reprendre ce pot : il est reserve, deja emporte ou la hotbar est pleine."));
	}
}

void ABotanicusPlayerController::
	ServerPlacePlantOnSalesDisplay_Implementation(
		ABotanicusSalesDisplayActor* SalesDisplay)
{
	ABotanicusCharacter* BotanicusCharacter =
		Cast<ABotanicusCharacter>(GetPawn());
	UBotanicusQuickBarComponent* QuickBar =
		BotanicusCharacter
			? BotanicusCharacter->GetQuickBarComponent()
			: nullptr;
	if (!IsValid(SalesDisplay) || !BotanicusCharacter || !QuickBar ||
		!SalesDisplay->IsEmpty() ||
		!IsLookingAtWorldItem(SalesDisplay, 450.0f))
	{
		ClientMessage(
			TEXT(
				"Regardez le presentoir et rapprochez-vous pour exposer la plante."));
		return;
	}

	const int32 SelectedSlotIndex = QuickBar->GetSelectedSlotIndex();
	const FBotanicusQuickBarSlot SelectedSlot =
		QuickBar->GetSlot(SelectedSlotIndex);
	if (SelectedSlot.IsEmpty() ||
		!IsSalePotItemKey(SelectedSlot.ItemKey) ||
		!SelectedSlot.CarriedState.bHasSalePotState ||
		SelectedSlot.CarriedState.SaleSoilItemKey.IsNone() ||
		SelectedSlot.CarriedState.SalePlantItemKey.IsNone())
	{
		ClientMessage(
			TEXT(
				"Selectionnez un pot de vente prepare avec du terreau et une plante."));
		return;
	}

	if (!SalesDisplay->TryMountSalePotState(
			SelectedSlot.CarriedState.SaleSoilItemKey,
			SelectedSlot.CarriedState.SalePlantItemKey,
			this,
			SelectedSlot.ItemKey))
	{
		ClientMessage(TEXT("Ce presentoir ne peut pas recevoir ce pot."));
		return;
	}
	QuickBar->RemoveQuantity(SelectedSlotIndex, 1);
	if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

void ABotanicusPlayerController::
	ServerCheckoutRegister_Implementation(
		ABotanicusCashRegisterActor* CashRegister)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn ||
		!IsValid(CashRegister) ||
		!CashRegister->IsOperational() ||
		!CashRegister->GetCheckoutCustomer() ||
		!IsLookingAtWorldItem(CashRegister, 475.0f))
	{
		return;
	}
	CashRegister->HandleCheckoutAction(ControlledPawn);
}

void ABotanicusPlayerController::
	ServerBeginManualDoorPlacement_Implementation(AActor* SelectedActor)
{
	if (!IsValid(SelectedActor) ||
		(!SelectedActor->IsA<ABotanicusCommunicationDoorActor>() &&
		 !IsEbsBuildingActor(SelectedActor)))
	{
		ClientMessage(TEXT("Sélection de porte invalide."));
		return;
	}

	ServerManualDoorToMove =
		Cast<ABotanicusCommunicationDoorActor>(SelectedActor);
	bServerManualDoorPlacement = true;
	if (!BuildManualDoorCandidates(SelectedActor))
	{
		bServerManualDoorPlacement = false;
		ServerManualDoorToMove = nullptr;
		ClientMessage(
			TEXT("Aucun emplacement de porte compatible trouvé."));
		ClientEndCommunicationDoorPlacement(false);
		return;
	}
	ClientBeginCommunicationDoorPlacement(
		ServerDoorCandidateLocations,
		ServerDoorCandidateYaws);
}

void ABotanicusPlayerController::
	ServerConfirmCommunicationDoor_Implementation(int32 CandidateIndex)
{
	UWorld* World = GetWorld();
	if (!World ||
		!ServerDoorCandidateLocations.IsValidIndex(CandidateIndex) ||
		!ServerDoorCandidateYaws.IsValidIndex(CandidateIndex))
	{
		return;
	}

	if (bServerManualDoorPlacement)
	{
		ABotanicusCommunicationDoorActor* Door =
			ServerManualDoorToMove.Get();
		if (!IsValid(Door))
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Door = World->SpawnActor<ABotanicusCommunicationDoorActor>(
				FVector(ServerDoorCandidateLocations[CandidateIndex]),
				FRotator(
					0.0f,
					ServerDoorCandidateYaws[CandidateIndex],
					0.0f),
				SpawnParameters);
			if (Door)
			{
				ConfigurePurchasedActorForNetworking(Door);
			}
		}
		else
		{
			Door->SetActorLocationAndRotation(
				FVector(ServerDoorCandidateLocations[CandidateIndex]),
				FRotator(
					0.0f,
					ServerDoorCandidateYaws[CandidateIndex],
					0.0f),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Door->ForceNetUpdate();
		}
		if (!Door)
		{
			return;
		}

		ServerDoorCandidatePurchasedWalls.Reset();
		ServerDoorCandidateExistingWalls.Reset();
		ServerDoorCandidateLocations.Reset();
		ServerDoorCandidateYaws.Reset();
		ServerManualDoorToMove = nullptr;
		bServerManualDoorPlacement = false;
		ScheduleSharedStateAutosave(this);
		ClientEndCommunicationDoorPlacement(true);
		return;
	}

	if (!ServerDoorCandidatePurchasedWalls.IsValidIndex(CandidateIndex) ||
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
	ServerManualDoorToMove = nullptr;
	bServerManualDoorPlacement = false;
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
	ServerManualDoorToMove = nullptr;
	bServerManualDoorPlacement = false;
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

	const bool bConfirmingBuildingPurchase =
		bServerBuildingPurchasePlacement;
	const bool bBeginDoorPlacement =
		bConfirmingBuildingPurchase &&
		BuildCommunicationDoorCandidates(ServerBuildingGroup);
	const int32 PreviousProgressionLevel =
		BuildingProgressionLevel;
	int32 NewProgressionLevel = BuildingProgressionLevel;
	if (bConfirmingBuildingPurchase)
	{
		BroadcastPurchasedBuildingSnapshot(true);
		if (const FBotanicusBuildingDefinition* PurchasedDefinition =
			FindBuildingDefinition(
				this,
				ServerPendingBuildingPurchaseKey))
		{
			NewProgressionLevel = FMath::Max(
				BuildingProgressionLevel,
				FMath::Max(
					1,
					PurchasedDefinition->
						RequiredDevelopmentLevel) +
					1);
		}
		BuildingProgressionLevel = NewProgressionLevel;
		if (NewProgressionLevel > PreviousProgressionLevel)
		{
			AddSharedFunds(
				this,
				FMath::Max(0, DevelopmentLevelRewardCredits));
		}
		ServerPendingBuildingPurchasePrice = 0;
		ServerPendingBuildingPurchaseKey = NAME_None;
		ForceNetUpdate();
		OnRep_OrderState();
	}
	ClearServerBuildingGroupMove();
	if (bConfirmingBuildingPurchase)
	{
		ScheduleSharedStateAutosave(this);
	}
	ClientEndBuildingGroupMove(true);
	if (NewProgressionLevel > PreviousProgressionLevel)
	{
		ClientMessage(
			*FString::Printf(
				TEXT(
					"Niveau de développement %d atteint : prime de %d crédits."),
				NewProgressionLevel,
				FMath::Max(0, DevelopmentLevelRewardCredits)));
	}
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
		RefundPendingBuildingPurchase();
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
	bBuildingCameraOrbitInitialized = false;
	bLocalBuildingPlacementValid = true;
	SetBuildingGroupHighlighted(true);
	UpdateBuildingGroupPlacementVisual(true);

	ClientMessage(
		TEXT("Bâtiment sélectionné : souris pour déplacer, molette pour tourner, Ctrl + molette pour zoomer, Maj gauche + souris pour orienter la caméra, clic/E pour confirmer."));
}

void ABotanicusPlayerController::ClientEndBuildingGroupMove_Implementation(
	bool bConfirmed)
{
	if (bBuildingCameraOrbitActive)
	{
		EndBuildingCameraOrbit();
	}
	SetBuildingGroupHighlighted(false);
	LocalBuildingGroup.Reset();
	LocalBuildingOriginalTransforms.Reset();
	LocalBuildingPivot = FVector::ZeroVector;
	LocalBuildingOriginalPivot = FVector::ZeroVector;
	LocalBuildingYaw = 0.0f;
	LocalBuildingGroundOffset = 0.0f;
	BuildingPreviewUpdateAccumulator = 0.0f;
	bBuildingCameraOrbitInitialized = false;
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
	bDoorEditSelectionActive = false;
	RefreshTopDownToolbar();

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
			"Choisissez un emplacement avec la souris, puis clic gauche pour confirmer. Clic droit/Echap pour annuler."));
}

void ABotanicusPlayerController::
	ClientEndCommunicationDoorPlacement_Implementation(bool bCreated)
{
	bCommunicationDoorPlacementActive = false;
	bDoorEditSelectionActive = false;
	LocalDoorCandidateLocations.Reset();
	LocalDoorCandidateYaws.Reset();
	LocalCommunicationDoorCandidateIndex = INDEX_NONE;
	if (IsValid(CommunicationDoorPreviewActor))
	{
		CommunicationDoorPreviewActor->Destroy();
		CommunicationDoorPreviewActor = nullptr;
	}
	RefreshTopDownToolbar();

	ClientMessage(
		bCreated
			? TEXT("Porte enregistrée.")
			: TEXT("Modification de la porte annulée."));
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
	const TArray<FVector_NetQuantize10>& RequestedPoints,
	uint8 RequestedPathType)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	const EBotanicusPathType PathType =
		RequestedPathType ==
			static_cast<uint8>(
				EBotanicusPathType::VisitorRoute)
			? EBotanicusPathType::VisitorRoute
			: EBotanicusPathType::Standard;
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

		FVector GroundedPoint;
		if (PathType == EBotanicusPathType::VisitorRoute)
		{
			GroundedPoint = FVector(RequestedPoint);
		}
		else
		{
			float LandscapeHeight = 0.0f;
			if (!FindLandscapeHeight(
				FVector2D(RequestedPoint.X, RequestedPoint.Y),
				LandscapeHeight))
			{
				return;
			}
			GroundedPoint = FVector(
				RequestedPoint.X,
				RequestedPoint.Y,
				LandscapeHeight + 0.25f);
		}

		if ((PointIndex == 0 ||
			PointIndex == RequestedPoints.Num() - 1)
			)
		{
			FVector SnappedPoint;
			ABotanicusPathActor* ConnectedPath = nullptr;
			if (SnapPathPoint(
				GroundedPoint,
				SnappedPoint,
				ConnectedPath,
				PathType))
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

			if (PathType ==
				EBotanicusPathType::VisitorRoute)
			{
				FCollisionQueryParams QueryParams(
					SCENE_QUERY_STAT(
						BotanicusVisitorRouteWallValidation),
					false);
				QueryParams.AddIgnoredActor(ControlledPawn);
				FHitResult WallHit;
				if (World->LineTraceSingleByChannel(
						WallHit,
						ValidatedPoints.Last() +
							FVector(0.0f, 0.0f, 80.0f),
						GroundedPoint +
							FVector(0.0f, 0.0f, 80.0f),
						ECC_Visibility,
						QueryParams))
				{
					ClientMessage(
						TEXT(
							"Route PNJ refusée : un segment traverse un mur ou un obstacle."));
					return;
				}
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

	Path->InitializeConfirmedPath(
		ValidatedPoints,
		PathType);
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
		TEXT("Created replicated %s with %d points and length %.0f cm."),
		PathType == EBotanicusPathType::VisitorRoute
			? TEXT("visitor route")
			: TEXT("path"),
		ValidatedPoints.Num(),
		TotalLength);
	ScheduleSharedStateAutosave(this);
}

void ABotanicusPlayerController::
	ServerCreateVisitorZone_Implementation(
	FVector_NetQuantize10 RequestedLocation,
	uint8 RequestedZoneType)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World ||
		!ControlledPawn ||
		RequestedLocation.ContainsNaN() ||
		RequestedZoneType > 2 ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			FVector(RequestedLocation)) >
			FMath::Square(MaximumBuildingEditDistance))
	{
		return;
	}

	const EBotanicusVisitorZoneType ZoneType =
		static_cast<EBotanicusVisitorZoneType>(
			RequestedZoneType);
	for (TActorIterator<ABotanicusVisitorZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		if (ZoneIt->GetZoneType() == ZoneType)
		{
			ZoneIt->Destroy();
		}
	}

	FVector ZoneExtent;
	switch (ZoneType)
	{
	case EBotanicusVisitorZoneType::Parking:
		ZoneExtent = FVector(500.0f, 300.0f, 6.0f);
		break;
	case EBotanicusVisitorZoneType::SalesArea:
		ZoneExtent = FVector(700.0f, 500.0f, 6.0f);
		break;
	case EBotanicusVisitorZoneType::Checkout:
	default:
		// 10 m x 6 m: enough room for the starter register, several
		// self-checkouts and a visible customer queue.
		ZoneExtent = FVector(500.0f, 300.0f, 6.0f);
		break;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusVisitorZoneActor* Zone =
		World->SpawnActor<ABotanicusVisitorZoneActor>(
			ABotanicusVisitorZoneActor::StaticClass(),
			FVector(RequestedLocation),
			FRotator::ZeroRotator,
			SpawnParameters);
	if (Zone)
	{
		Zone->InitializeZone(ZoneType, ZoneExtent);
		ScheduleSharedStateAutosave(this);
	}
}

void ABotanicusPlayerController::
	ServerCreateRefundZone_Implementation(
	FVector_NetQuantize10 RequestedLocation)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World ||
		!ControlledPawn ||
		RequestedLocation.ContainsNaN() ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			FVector(RequestedLocation)) >
			FMath::Square(MaximumBuildingEditDistance))
	{
		return;
	}

	for (TActorIterator<ABotanicusRefundZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		ZoneIt->Destroy();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABotanicusRefundZoneActor* Zone =
		World->SpawnActor<ABotanicusRefundZoneActor>(
			ABotanicusRefundZoneActor::StaticClass(),
			FVector(RequestedLocation),
			FRotator::ZeroRotator,
			SpawnParameters);
	if (Zone)
	{
		Zone->InitializeZone(FVector(220.0f, 150.0f, 6.0f));
		ScheduleSharedStateAutosave(this);
	}
}

void ABotanicusPlayerController::
	ServerCreateDeliveryZone_Implementation(
	FVector_NetQuantize10 RequestedLocation)
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World ||
		!ControlledPawn ||
		RequestedLocation.ContainsNaN() ||
		FVector::DistSquared2D(
			ControlledPawn->GetActorLocation(),
			FVector(RequestedLocation)) >
			FMath::Square(MaximumBuildingEditDistance))
	{
		return;
	}

	for (TActorIterator<ABotanicusDeliveryZoneActor> ZoneIt(World);
		 ZoneIt;
		 ++ZoneIt)
	{
		ZoneIt->Destroy();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (World->SpawnActor<ABotanicusDeliveryZoneActor>(
			ABotanicusDeliveryZoneActor::StaticClass(),
			FVector(RequestedLocation),
			FRotator::ZeroRotator,
			SpawnParameters))
	{
		ScheduleSharedStateAutosave(this);
	}
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
	const EBotanicusPathType OriginalPathType =
		Path->GetPathType();
	int32 RemainingPieceCount = 0;
	auto SpawnRemainingPiece =
		[this,
		 World,
		 &OriginalJunctions,
		 OriginalPathType,
		 &RemainingPieceCount](
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

			Piece->InitializeConfirmedPath(
				PiecePoints,
				OriginalPathType);
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

	return Actor->IsA<ABotanicusCatalogBuildingActor>() ||
		Actor->GetClass()->GetPathName().Contains(
			TEXT("/Game/EasyBuildingSystem/Blueprints/BuildingObjects/"));
}

bool ABotanicusPlayerController::IsStructuralBuildingActor(
	const AActor* Actor) const
{
	if (!IsEbsBuildingActor(Actor))
	{
		return false;
	}
	if (Actor->IsA<ABotanicusCatalogBuildingActor>())
	{
		return true;
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

bool ABotanicusPlayerController::BuildManualDoorCandidates(
	AActor* SelectedActor)
{
	check(HasAuthority());

	ServerDoorCandidatePurchasedWalls.Reset();
	ServerDoorCandidateExistingWalls.Reset();
	ServerDoorCandidateLocations.Reset();
	ServerDoorCandidateYaws.Reset();

	UWorld* World = GetWorld();
	if (!World || !IsValid(SelectedActor))
	{
		return false;
	}

	const bool bMovingExistingDoor =
		SelectedActor->IsA<ABotanicusCommunicationDoorActor>();
	TSet<AActor*> CandidateBuildings;
	if (bMovingExistingDoor)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsEbsBuildingActor(*It))
			{
				CandidateBuildings.Add(*It);
			}
		}
	}
	else if (SelectedActor->IsA<ABotanicusCatalogBuildingActor>())
	{
		CandidateBuildings.Add(SelectedActor);
	}
	else
	{
		for (AActor* Actor :
			 BuildCompleteBuildingGroup(SelectedActor))
		{
			if (IsValid(Actor))
			{
				CandidateBuildings.Add(Actor);
			}
		}
	}

	const auto IsOccupied =
		[this, World](const FVector& Location)
		{
			for (TActorIterator<ABotanicusCommunicationDoorActor> It(
					 World);
				 It;
				 ++It)
			{
				if (*It != ServerManualDoorToMove.Get() &&
					FVector::DistSquared2D(
						It->GetActorLocation(),
						Location) < FMath::Square(140.0f))
				{
					return true;
				}
			}
			return false;
		};
	const auto AddCandidate =
		[this, &IsOccupied](
			const FVector& Origin,
			const FVector& Extent)
		{
			FVector Location = Origin;
			Location.Z = Origin.Z - Extent.Z;
			const bool bNormalIsX = Extent.X <= Extent.Y;
			if (IsOccupied(Location))
			{
				return;
			}
			for (const FVector_NetQuantize10& Existing :
				 ServerDoorCandidateLocations)
			{
				if (FVector::DistSquared2D(
						FVector(Existing),
						Location) < FMath::Square(100.0f))
				{
					return;
				}
			}
			ServerDoorCandidateLocations.Add(
				FVector_NetQuantize10(Location));
			ServerDoorCandidateYaws.Add(
				bNormalIsX ? 0.0f : 90.0f);
		};

	for (AActor* Building : CandidateBuildings)
	{
		if (!IsValid(Building))
		{
			continue;
		}
		if (Building->IsA<ABotanicusCatalogBuildingActor>())
		{
			FBox FrontOpeningBounds(EForceInit::ForceInit);
			TInlineComponentArray<UPrimitiveComponent*> Components(
				Building);
			for (UPrimitiveComponent* Component : Components)
			{
				if (!IsValid(Component) ||
					!Component->GetName().Contains(
						TEXT("Wall"),
						ESearchCase::IgnoreCase))
				{
					continue;
				}
				if (Component->GetName().Contains(
						TEXT("FrontWall"),
						ESearchCase::IgnoreCase))
				{
					FrontOpeningBounds += Component->Bounds.GetBox();
					continue;
				}
				AddCandidate(
					Component->Bounds.Origin,
					Component->Bounds.BoxExtent);
			}
			if (FrontOpeningBounds.IsValid)
			{
				AddCandidate(
					FrontOpeningBounds.GetCenter(),
					FrontOpeningBounds.GetExtent());
			}
		}
		else if (IsPlainBuildingWall(Building))
		{
			FVector Origin;
			FVector Extent;
			Building->GetActorBounds(true, Origin, Extent);
			AddCandidate(Origin, Extent);
		}
	}

	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Manual door editing found %d candidate(s)."),
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

		struct FPlacementTestBox
		{
			FVector Center = FVector::ZeroVector;
			FVector Extent = FVector::ZeroVector;
			FQuat Rotation = FQuat::Identity;
		};
		TArray<FPlacementTestBox> TestBoxes;
		if (GroupMember->IsA<ABotanicusCatalogBuildingActor>())
		{
			TInlineComponentArray<UPrimitiveComponent*> Components(
				GroupMember);
			for (UPrimitiveComponent* Component : Components)
			{
				if (!IsValid(Component) ||
					Component->GetCollisionEnabled() ==
						ECollisionEnabled::NoCollision)
				{
					continue;
				}
				FPlacementTestBox& Box =
					TestBoxes.AddDefaulted_GetRef();
				Box.Center = Component->Bounds.Origin;
				Box.Extent = Component->Bounds.BoxExtent;
				Box.Rotation = Component->GetComponentQuat();
			}
		}
		else
		{
			const FBox LocalBounds =
				GroupMember->CalculateComponentsBoundingBoxInLocalSpace(
					false,
					true);
			if (LocalBounds.IsValid)
			{
				const FTransform ActorTransform =
					GroupMember->GetActorTransform();
				FPlacementTestBox& Box =
					TestBoxes.AddDefaulted_GetRef();
				Box.Center = ActorTransform.TransformPosition(
					LocalBounds.GetCenter());
				Box.Extent =
					LocalBounds.GetExtent() *
					ActorTransform.GetScale3D().GetAbs();
				Box.Rotation = ActorTransform.GetRotation();
			}
		}

		for (FPlacementTestBox& TestBox : TestBoxes)
		{
			// A tiny inset allows snapped pieces and neighbouring buildings to
			// touch exactly at their faces without being penetrations.
			TestBox.Extent.X =
				FMath::Max(1.0f, TestBox.Extent.X - 5.0f);
			TestBox.Extent.Y =
				FMath::Max(1.0f, TestBox.Extent.Y - 5.0f);
			TestBox.Extent.Z =
				FMath::Max(1.0f, TestBox.Extent.Z - 5.0f);

			TArray<FOverlapResult> Overlaps;
			World->OverlapMultiByObjectType(
				Overlaps,
				TestBox.Center,
				TestBox.Rotation,
				ObjectQuery,
				FCollisionShape::MakeBox(TestBox.Extent),
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
				// purchased building is magnetised to an existing one.
				if (GroupMember == ServerSnappedMovingWall &&
					OverlappedActor == ServerSnappedExistingWall)
				{
					continue;
				}

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
	ServerPendingBuildingPurchasePrice = 0;
	ServerPendingBuildingPurchaseKey = NAME_None;
}

void ABotanicusPlayerController::RefundPendingBuildingPurchase()
{
	if (ServerPendingBuildingPurchasePrice <= 0)
	{
		ServerPendingBuildingPurchaseKey = NAME_None;
		return;
	}

	const int32 RefundedPrice = ServerPendingBuildingPurchasePrice;
	const FName RefundedKey = ServerPendingBuildingPurchaseKey;
	AddSharedFunds(this, RefundedPrice);
	ServerPendingBuildingPurchasePrice = 0;
	ServerPendingBuildingPurchaseKey = NAME_None;
	ForceNetUpdate();
	OnRep_OrderState();
	ClientMessage(
		*FString::Printf(
			TEXT("Placement annulé : %d crédits remboursés."),
			RefundedPrice));
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT("Refunded %d credits for cancelled building purchase %s."),
		RefundedPrice,
		*RefundedKey.ToString());
	ScheduleSharedStateAutosave(this);
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
