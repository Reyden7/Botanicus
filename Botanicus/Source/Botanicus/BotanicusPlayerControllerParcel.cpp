#include "BotanicusPlayerController.h"

#include "BotanicusGameMode.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputKeyEventArgs.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"

bool ABotanicusPlayerController::TryCollectNearbyDeliveryParcel()
{
	if (!IsLocalPlayerController() || !GetPawn() || !GetWorld() || IsValid(LocalMovedDeliveryParcel) ||
		IsValid(LocalQuickBarItemPreview) || IsValid(LocalLargeEquipmentPlacement) || IsValid(LocalMovedPlaceableItem))
		return false;

	ABotanicusDeliveryParcelActor* AimedParcel = nullptr;
	float BestDistanceSquared = FMath::Square(400.0f);
	for (TActorIterator<ABotanicusDeliveryParcelActor> It(GetWorld()); It; ++It)
	{
		const float DistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation());
		if (!It->GetCarrier() && DistanceSquared <= BestDistanceSquared && IsLookingAtWorldItem(*It, 400.0f))
		{
			AimedParcel = *It;
			BestDistanceSquared = DistanceSquared;
		}
	}
	if (!AimedParcel)
		return false;

	if (AimedParcel->IsOpened())
		ServerCollectDeliveryParcel(AimedParcel);
	else
		ServerBeginDeliveryParcelMove(AimedParcel);
	return true;
}

bool ABotanicusPlayerController::HandleDeliveryParcelInput(const FInputKeyEventArgs& Params)
{
	if (!IsValid(LocalMovedDeliveryParcel))
		return false;

	const bool bPressed = Params.Event == IE_Pressed;
	const bool bPlacing = ParcelCarryMode == EParcelCarryMode::Placement;
	const bool bInspecting = ParcelCarryMode == EParcelCarryMode::Inspection;
	if (Params.Key == EKeys::A || Params.Key == EKeys::E)
	{
		if (Params.Key == EKeys::A && bPressed && bInspecting)
			BeginDeliveryParcelPlacement();
		return true;
	}
	if (Params.Key == EKeys::RightMouseButton)
	{
		if (bPressed && bPlacing)
			CancelDeliveryParcelPlacement();
		else if (bInspecting && bPressed && !bParcelInspectionRotating)
		{
			bParcelInspectionRotating = true;
			RotationInput = FRotator::ZeroRotator;
			SetIgnoreLookInput(true);
		}
		return true;
	}
	if (Params.Key == EKeys::Escape && bPlacing)
	{
		if (bPressed)
			CancelDeliveryParcelPlacement();
		return true;
	}
	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (bPressed && bPlacing)
			ConfirmDeliveryParcelPlacement();
		return true;
	}
	if (Params.Key == EKeys::MouseScrollUp || Params.Key == EKeys::MouseScrollDown)
	{
		if (bPressed && bPlacing)
		{
			const bool bFine = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
			const float Direction = Params.Key == EKeys::MouseScrollUp ? 1.f : -1.f;
			ParcelPlacementYaw = FMath::UnwindDegrees(ParcelPlacementYaw + Direction *
				(bFine ? FineEquipmentRotationStep : EquipmentRotationStep));
			ParcelPreviewUpdateAccumulator = 1.f;
		}
		return true;
	}
	return false;
}

bool ABotanicusPlayerController::HandleParcelInspectionMouse(const FInputKeyEventArgs& Params)
{
	if (!bParcelInspectionRotating)
		return false;
	if (!IsValid(LocalMovedDeliveryParcel) || ParcelCarryMode != EParcelCarryMode::Inspection)
	{
		StopParcelInspectionRotation();
		return false;
	}
	if (Params.Key == EKeys::RightMouseButton && Params.Event == IE_Released)
	{
		StopParcelInspectionRotation(true);
		return true;
	}
	if (Params.Event != IE_Axis)
		return false;

	// The viewport supplies MouseX/Y; Enhanced Input also derives Mouse2D from
	// those samples. Consume them once, before either camera binding sees them.
	if (Params.Key == EKeys::MouseX)
		ParcelInspectionLookDelta.X += Params.AmountDepressed * InspectionRotationSensitivity;
	else if (Params.Key == EKeys::MouseY)
		ParcelInspectionLookDelta.Y += Params.AmountDepressed * InspectionRotationSensitivity *
			(InvertInspectionVerticalAxis ? -1.f : 1.f);
	else
		return Params.Key == EKeys::Mouse2D;
	return true;
}

void ABotanicusPlayerController::StopParcelInspectionRotation(bool bCoast)
{
	if (bParcelInspectionRotating)
		SetIgnoreLookInput(false);
	bParcelInspectionRotating = false;
	if (bCoast)
	{
		ParcelInspectionAngularVelocity *= InspectionAngularInertia;
		return;
	}
	ParcelInspectionLookDelta = FVector2D::ZeroVector;
	ParcelInspectionAngularVelocity = FVector2D::ZeroVector;
	ParcelInspectionTargetRotation = ParcelInspectionRotation;
}

void ABotanicusPlayerController::ClientBeginDeliveryParcelInspection_Implementation(ABotanicusDeliveryParcelActor* Parcel)
{
	if (!IsValid(Parcel) || !IsValid(GetPawn()))
		return;

	LocalMovedDeliveryParcel = Parcel;
	SetInteractionTargetHighlighted(LocalInteractionHighlightActor.Get(), false);
	LocalInteractionHighlightActor.Reset();
	if (LocalActiveParcelCut == Parcel)
	{
		LocalActiveParcelCut = nullptr;
		bParcelCutActionHeld = false;
	}
	ParcelCarryMode = EParcelCarryMode::Inspection;
	LastSentParcelInspectionOrientation.Reset();
	ParcelInspectionSendElapsed = .1f;
	ParcelInspectionRotation = Parcel->GetDefaultCarryRotation();
	StopParcelInspectionRotation();
	LocalParcelPresentation = FTransform(Parcel->GetActorQuat(), Parcel->GetActorLocation());
	ClientMessage(TEXT("Colis en main : maintenez le clic droit pour l’inspecter, puis A pour le poser."));
}

void ABotanicusPlayerController::BeginDeliveryParcelPlacement()
{
	StopParcelInspectionRotation();
	ParcelCarryMode = EParcelCarryMode::Placement;
	ServerSetParcelPlacementPreview(LocalMovedDeliveryParcel.Get(), true);
	ParcelPlacementYaw = GetControlRotation().Yaw;
	ParcelPreviewUpdateAccumulator = 1.f;
	UpdateDeliveryParcelCarry(0.f);
	ClientMessage(TEXT("Molette pour tourner, clic gauche pour poser, clic droit ou Échap pour revenir à l’inspection."));
}

void ABotanicusPlayerController::CancelDeliveryParcelPlacement()
{
	GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>()->ClearPlacementHologram(LocalMovedDeliveryParcel);
	ParcelCarryMode = EParcelCarryMode::Inspection;
	ServerSetParcelPlacementPreview(LocalMovedDeliveryParcel.Get(), false);
	bLocalParcelPlacementValid = false;
}

void ABotanicusPlayerController::UpdateDeliveryParcelCarry(float DeltaTime)
{
	if (ParcelCarryMode == EParcelCarryMode::None)
		return;

	auto* Parcel = LocalMovedDeliveryParcel.Get();
	if (!IsValid(Parcel) || !IsValid(GetPawn()))
	{
		ClearLocalDeliveryParcelCarry();
		return;
	}

	switch (ParcelCarryMode)
	{
	case EParcelCarryMode::Inspection:
		// Inspection uses the current camera after its update this frame.
		return;
	case EParcelCarryMode::Placement:
		ParcelPreviewUpdateAccumulator += DeltaTime;
		if (ParcelPreviewUpdateAccumulator >= 1.f / 30.f)
		{
			ParcelPreviewUpdateAccumulator = 0.f;
			const FVector Ground = GetViewDirectedGroundPlacementLocation(
				MinimumQuickBarItemPlacementDistance, MaximumQuickBarItemPlacementDistance);
			bLocalParcelPlacementValid = ResolveDeliveryParcelPlacement(Parcel, Ground, ParcelPlacementYaw, LocalParcelPlacement);
			GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>()->SetPlacementHologram(Parcel, bLocalParcelPlacementValid);
		}
		break;
	case EParcelCarryMode::Confirming:
		break;
	case EParcelCarryMode::Settling:
		// Keep the accepted visual pose until the collision body receives its
		// authoritative placement. The visual offset has no effect on collision.
		if (!Parcel->GetCarrier() && FVector::DistSquared(Parcel->GetActorLocation(), LocalParcelPlacement.GetLocation()) < FMath::Square(20.f))
		{
			ClearLocalDeliveryParcelCarry();
			return;
		}
		ParcelSettlementRemaining -= DeltaTime;
		if (ParcelSettlementRemaining <= 0.f)
		{
			ClearLocalDeliveryParcelCarry();
			return;
		}
		break;
	default:
		return;
	}

	const float Alpha = 1.f - FMath::Exp(-24.f * DeltaTime);
	LocalParcelPresentation.Blend(LocalParcelPresentation, LocalParcelPlacement, Alpha);
	Parcel->SetLocalParcelPresentation(LocalParcelPresentation);
}

void ABotanicusPlayerController::UpdateParcelInspectionRotation(float DeltaTime)
{
	if (DeltaTime <= UE_SMALL_NUMBER)
		return;

	const float Step = FMath::Min(DeltaTime, .05f);
	if (!ParcelInspectionLookDelta.IsNearlyZero())
	{
		const FVector2D InputVelocity = (ParcelInspectionLookDelta / Step).GetClampedToMaxSize(InspectionMaxAngularSpeed);
		const float Retention = FMath::Pow(InspectionAngularInertia, Step * 60.f);
		ParcelInspectionAngularVelocity = FMath::Lerp(InputVelocity, ParcelInspectionAngularVelocity, Retention);
		ParcelInspectionLookDelta = FVector2D::ZeroVector;
	}
	else
	{
		ParcelInspectionAngularVelocity *= InspectionAngularInertia > 0.f ?
			FMath::Exp(-FMath::Max(.1f, InspectionAngularDamping) * DeltaTime) : 0.f;
	}
	if (ParcelInspectionAngularVelocity.IsNearlyZero(.05f))
		ParcelInspectionAngularVelocity = FVector2D::ZeroVector;

	const FVector2D Turn = ParcelInspectionAngularVelocity * Step;
	ParcelInspectionTargetRotation = (
		FQuat(FVector::UpVector, FMath::DegreesToRadians(Turn.X)) *
		FQuat(FVector::RightVector, FMath::DegreesToRadians(-Turn.Y)) *
		ParcelInspectionTargetRotation).GetNormalized();
	const float Alpha = 1.f - FMath::Exp(-InspectionRotationSmoothness * DeltaTime);
	ParcelInspectionRotation = FQuat::Slerp(ParcelInspectionRotation, ParcelInspectionTargetRotation, Alpha);
}

void ABotanicusPlayerController::SendParcelInspectionOrientation(float DeltaTime)
{
	constexpr float SendInterval = .1f;
	ParcelInspectionSendElapsed += DeltaTime;
	if (ParcelInspectionSendElapsed < SendInterval)
		return;

	if (!LastSentParcelInspectionOrientation.IsSet() ||
		LastSentParcelInspectionOrientation.GetValue().AngularDistance(ParcelInspectionRotation) >= FMath::DegreesToRadians(1.0))
	{
		ParcelInspectionSendElapsed = 0.f;
		LastSentParcelInspectionOrientation = ParcelInspectionRotation;
		ServerSetParcelInspectionOrientation(LocalMovedDeliveryParcel, ParcelInspectionRotation.Rotator());
	}
}

void ABotanicusPlayerController::ServerSetParcelInspectionOrientation_Implementation(
	ABotanicusDeliveryParcelActor* Parcel, FRotator RelativeRotation)
{
	if (IsValid(Parcel) && ServerMovedDeliveryParcel == Parcel && GetPawn() && Parcel->GetCarrier() == GetPawn() &&
		!RelativeRotation.ContainsNaN())
		Parcel->SetCarriedInspectionOrientation(RelativeRotation);
}

void ABotanicusPlayerController::ServerSetParcelPlacementPreview_Implementation(ABotanicusDeliveryParcelActor* Parcel, bool bPreview)
{
	if (IsValid(Parcel) && ServerMovedDeliveryParcel == Parcel && GetPawn() && Parcel->GetCarrier() == GetPawn())
	{
		Parcel->SetCarriedPlacementPreview(bPreview);
	}
}

void ABotanicusPlayerController::UpdateLocalParcelInspection(float DeltaTime)
{
	auto* Parcel = LocalMovedDeliveryParcel.Get();
	if (ParcelCarryMode != EParcelCarryMode::Inspection || !IsValid(Parcel) || !IsValid(GetPawn()))
		return;

	UpdateParcelInspectionRotation(DeltaTime);
	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	const FQuat ViewRotation = View.Quaternion();
	const FTransform Sway = Parcel->UpdateCarrySway(
		*GetPawn(), FRotator(0.0, View.Yaw, 0.0).Quaternion(), DeltaTime, !bParcelInspectionRotating);
	const FVector Extent = Parcel->GetParcelHalfExtent() * Parcel->GetActorScale3D().GetAbs();
	const double Distance = FMath::Max<double>(InspectionDistance, Extent.Size() * 2.2);
	const FVector ViewOffset(Distance, InspectionHorizontalOffset, InspectionVerticalOffset);
	const FTransform CameraPivot(ViewRotation, Eye);
	LocalParcelPresentation = FTransform(Sway.GetRotation() * ParcelInspectionRotation, ViewOffset + Sway.GetLocation())
		* CameraPivot;
	Parcel->SetLocalParcelPresentation(LocalParcelPresentation);
	SendParcelInspectionOrientation(DeltaTime);
}

void ABotanicusPlayerController::ConfirmDeliveryParcelPlacement()
{
	if (!bLocalParcelPlacementValid)
	{
		ClientMessage(TEXT("Position invalide : le colis ne peut pas être posé ici."));
		return;
	}
	ParcelCarryMode = EParcelCarryMode::Confirming;
	DismissInteractionPromptAfterConfirmation();
	ServerConfirmDeliveryParcelMove(LocalMovedDeliveryParcel, LocalParcelPlacement.GetLocation(), ParcelPlacementYaw);
}

void ABotanicusPlayerController::ClientDeliveryParcelPlacementResult_Implementation(
	ABotanicusDeliveryParcelActor* Parcel, bool bAccepted, const FTransform& Placement)
{
	if (LocalMovedDeliveryParcel != Parcel || ParcelCarryMode != EParcelCarryMode::Confirming)
		return;
	if (!IsValid(Parcel))
	{
		ClearLocalDeliveryParcelCarry();
		return;
	}
	if (!bAccepted)
	{
		ParcelCarryMode = EParcelCarryMode::Placement;
		ParcelPreviewUpdateAccumulator = 1.f;
		ClientMessage(TEXT("Déplacement refusé : position du colis invalide."));
		return;
	}
	GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>()->ClearPlacementHologram(Parcel);
	LocalParcelPlacement = Placement;
	LocalParcelPresentation = Placement;
	Parcel->SetLocalParcelPresentation(Placement);
	ParcelSettlementRemaining = .4f;
	ParcelCarryMode = EParcelCarryMode::Settling;
}

void ABotanicusPlayerController::ClearLocalDeliveryParcelCarry()
{
	if (auto* Parcel = LocalMovedDeliveryParcel.Get(); IsValid(Parcel))
	{
		if (auto* Juice = GetWorld()->GetSubsystem<UBotanicusJuiceWorldSubsystem>())
			Juice->ClearPlacementHologram(Parcel);
		Parcel->ResetLocalParcelPresentation();
	}
	LocalMovedDeliveryParcel = nullptr;
	ParcelCarryMode = EParcelCarryMode::None;
	bLocalParcelPlacementValid = false;
	StopParcelInspectionRotation();
}

void ABotanicusPlayerController::ReleaseServerDeliveryParcelCarry()
{
	if (HasAuthority())
	{
		if (auto* Parcel = ServerMovedDeliveryParcel.Get(); IsValid(Parcel) && GetWorld() && !GetWorld()->bIsTearingDown)
			Parcel->EndCarry(Parcel->GetParcelSaveTransform());
		ServerMovedDeliveryParcel = nullptr;
	}
}

void ABotanicusPlayerController::ServerBeginDeliveryParcelMove_Implementation(ABotanicusDeliveryParcelActor* Parcel)
{
	if (!IsValid(Parcel) || IsValid(ServerMovedDeliveryParcel) || IsValid(ServerMovedPlaceableItem) ||
		!GetPawn() || !IsLookingAtWorldItem(Parcel, 450.f) || !Parcel->TryBeginCarry(GetPawn()))
	{
		ClientMessage(TEXT("Impossible de prendre ce colis."));
		return;
	}
	ServerMovedDeliveryParcel = Parcel;
	UBotanicusAudioWorldSubsystem::ConfirmPickup(GetPawn(), Parcel);
	ClientBeginDeliveryParcelInspection(Parcel);
}

void ABotanicusPlayerController::ServerConfirmDeliveryParcelMove_Implementation(
	ABotanicusDeliveryParcelActor* Parcel, FVector_NetQuantize10 RequestedLocation, float RequestedYaw)
{
	FTransform Placement;
	if (!IsValid(Parcel) || ServerMovedDeliveryParcel != Parcel || !GetPawn() || Parcel->GetCarrier() != GetPawn() ||
		!ResolveDeliveryParcelPlacement(Parcel, RequestedLocation, RequestedYaw, Placement))
	{
		ClientDeliveryParcelPlacementResult(Parcel, false, FTransform::Identity);
		return;
	}
	Parcel->EndCarry(Placement);
	ServerMovedDeliveryParcel = nullptr;
	ClientDeliveryParcelPlacementResult(Parcel, true, Placement);
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
		GameMode->ScheduleInventoryAutosave();
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
		Parcel->IsOpened() || RequestedLocation.ContainsNaN() || !FMath::IsFinite(RequestedYaw) ||
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
	if (const auto Furniture = ResolveFurniturePlacement(TEXT("DeliveryParcel"), RequestedYaw,
		[&BoxExtent] { return FBox(-BoxExtent, BoxExtent); }, Parcel->GetActorScale3D(), Parcel, OutTransform); Furniture.IsSet())
	{
		return Furniture.GetValue();
	}
	FCollisionQueryParams FloorQuery(
		SCENE_QUERY_STAT(BotanicusDeliveryParcelPlacementFloor),
		false);
	FloorQuery.AddIgnoredActor(ControlledPawn);
	FloorQuery.AddIgnoredActor(Parcel);

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
		PlacementLocation, Parcel->GetActorScale3D());
	if (!bFoundFloor || FloorHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}

	FVector Eye;
	FRotator ViewRotation;
	GetPlayerViewPoint(Eye, ViewRotation);
	if (World->LineTraceTestByChannel(Eye, PlacementLocation, ECC_Visibility, FloorQuery))
		return false;

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams OverlapQuery(
		SCENE_QUERY_STAT(BotanicusDeliveryParcelPlacementOverlap),
		false);
	OverlapQuery.AddIgnoredActor(ControlledPawn);
	OverlapQuery.AddIgnoredActor(Parcel);

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
