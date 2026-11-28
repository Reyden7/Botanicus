#include "BotanicusPlayerController.h"

#include "Audio/BotanicusAudioWorldSubsystem.h"
#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "Delivery/BotanicusBotanistBookActor.h"
#include "Delivery/BotanicusDeliveryParcelActor.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "QuickBar/BotanicusQuickBarComponent.h"

namespace
{
	UBotanicusQuickBarComponent* BookQuickBar(const APawn* Pawn)
	{
		const auto* Character = Cast<ABotanicusCharacter>(Pawn);
		return Character ? Character->GetQuickBarComponent() : nullptr;
	}

	bool IsSelectedBook(const APawn* Pawn)
	{
		const auto* Bar = BookQuickBar(Pawn);
		if (!Bar)
			return false;
		const auto Slot = Bar->GetSelectedSlot();
		return Slot.ItemKey == ABotanicusBotanistBookActor::BookKey() &&
			Slot.InstanceId == ABotanicusBotanistBookActor::BookId() && Slot.Quantity == 1;
	}
}

bool ABotanicusPlayerController::IsPlacingBotanistBook() const
{
	return IsValid(LocalQuickBarItemPreview) && LocalQuickBarItemKey == ABotanicusBotanistBookActor::BookKey();
}

ABotanicusBotanistBookActor* ABotanicusPlayerController::FindAimedBotanistBook() const
{
	if (!GetWorld() || !GetPawn())
		return nullptr;
	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusBookAim), false, GetPawn());
	GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * MaximumWorldInteractionDistance,
		ECC_Visibility, Query);
	auto* Book = Cast<ABotanicusBotanistBookActor>(Hit.GetActor());
	return IsValid(Book) && !Book->IsHeldPresentation() &&
		FVector::DistSquared(Eye, Hit.ImpactPoint) <= FMath::Square(Book->BookInteractionRange) ? Book : nullptr;
}

bool ABotanicusPlayerController::CanReachBotanistBook(const ABotanicusBotanistBookActor* Book) const
{
	if (!IsValid(Book) || Book->IsHeldPresentation() || !GetPawn() || !GetWorld())
		return false;
	if (FindAimedBotanistBook() == Book)
		return true;
	if (IsLocalController())
		return false;

	// A remote camera update can arrive a frame after the interaction RPC.
	// Validate the server's viewing direction and line of sight to the thin book.
	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	const FVector Center = Book->GetActorTransform().TransformPosition(Book->GetBookLocalBounds().GetCenter());
	const FVector ToBook = Center - Eye;
	const float Range = Book->BookInteractionRange;
	const FVector LookEnd = Eye + GetControlRotation().Vector() * Range;
	const double AimTolerance = Book->GetPlacementBoxExtent().Size() + 6.;
	if (ToBook.SizeSquared() > FMath::Square(Range) ||
		FMath::PointDistToSegment(Center, Eye, LookEnd) > AimTolerance)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusBookReach), false, GetPawn());
	return GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, Query) &&
		Hit.GetActor() == Book;
}

bool ABotanicusPlayerController::HandleBotanistBookInput(const FInputKeyEventArgs& Params)
{
	if (bLocalBookSessionActive)
	{
		if ((Params.Key == EKeys::E || Params.Key == EKeys::Escape) && Params.Event == IE_Pressed)
			RequestCloseBotanistNotebook();
		return true;
	}
	if (Params.Key != EKeys::E || Params.Event != IE_Pressed || IsValid(LocalQuickBarItemPreview) ||
		IsValid(LocalLargeEquipmentPlacement) || IsValid(LocalMovedPlaceableItem) ||
		IsValid(LocalMovedDeliveryParcel) || HasCarriedTransplant())
	{
		return false;
	}

	const bool bHeld = IsSelectedBook(GetPawn());
	auto* WorldBook = bHeld ? nullptr : FindAimedBotanistBook();
	if (!bHeld && !WorldBook)
		return false;

	ServerUseBotanistBook(WorldBook, bHeld);
	return true;
}

void ABotanicusPlayerController::ServerUseBotanistBook_Implementation(
	ABotanicusBotanistBookActor* WorldBook, bool bUseHeldBook)
{
	APawn* ControlledPawn = GetPawn();
	if (AreRobotEmoteControlsLocked() || ServerBookSession.IsValid() || !ControlledPawn ||
		IsValid(ServerMovedDeliveryParcel) || IsValid(ServerMovedPlaceableItem) || HasCarriedTransplant())
	{
		return;
	}

	auto* Book = bUseHeldBook ? ServerHeldBook.Get() : WorldBook;
	if (!IsValid(Book) || (bUseHeldBook ? !IsSelectedBook(ControlledPawn) : !CanReachBotanistBook(Book)) ||
		!Book->IsAvailable())
	{
		return;
	}

	if (!bUseHeldBook)
	{
		auto* Bar = BookQuickBar(ControlledPawn);
		if (!Bar || ServerHeldBook.IsValid())
			return;

		// Adding to the selected slot can broadcast immediately. Reuse the world
		// actor so that the equipment handler never creates a second presentation.
		ServerHeldBook = Book;
		int32 AddedSlot = INDEX_NONE;
		if (!Bar->AddUniqueItem(ABotanicusBotanistBookActor::BookKey(), FBotanicusCarriedItemState{},
			AddedSlot, ABotanicusBotanistBookActor::BookId()))
		{
			ServerHeldBook.Reset();
			ClientMessage(TEXT("Hotbar pleine : impossible de prendre le carnet."));
			return;
		}

		UBotanicusAudioWorldSubsystem::ConfirmPickup(ControlledPawn, Book);
		Book->SetHeldPresentation(*ControlledPawn, true);
		Bar->SelectSlotAuthoritative(AddedSlot);
		return;
	}

	ServerBookSession = Book;
	if (Book->BeginReading(*this))
	{
		ClientUpdateBotanistBookSession(true, false);
	}
	else
	{
		ServerBookSession.Reset();
	}
}

void ABotanicusPlayerController::ShowBotanistBookSession(ABotanicusBotanistBookActor* Book)
{
	if (HasAuthority() && ServerBookSession.Get() == Book)
		ClientUpdateBotanistBookSession(true, true);
}

void ABotanicusPlayerController::FinishBotanistBookSession(ABotanicusBotanistBookActor* Book)
{
	if (HasAuthority() && ServerBookSession.Get(true) == Book)
	{
		ServerBookSession.Reset();
		ClientUpdateBotanistBookSession(false, false);
	}
}

void ABotanicusPlayerController::RequestCloseBotanistNotebook()
{
	if (bLocalBookSessionActive)
		ServerCloseBotanistBook();
}

void ABotanicusPlayerController::ServerCloseBotanistBook_Implementation()
{
	if (auto* Book = ServerBookSession.Get(); Book && Book->RequestClose(*this))
		ClientUpdateBotanistBookSession(true, false);
}

void ABotanicusPlayerController::ClientUpdateBotanistBookSession_Implementation(bool bActive, bool bShow)
{
	if (!IsLocalPlayerController())
		return;
	if (bLocalBookSessionActive != bActive)
	{
		bLocalBookSessionActive = bActive;
		SetIgnoreMoveInput(bActive);
		SetIgnoreLookInput(bActive);
		RotationInput = FRotator::ZeroRotator;
	}
	SetBotanistNotebookVisible(bShow);
}

void ABotanicusPlayerController::BindBotanistBookQuickBar()
{
	if (!HasAuthority())
		return;
	if (auto* Bar = BookQuickBar(GetPawn()))
	{
		Bar->OnSelectedSlotChanged.AddUniqueDynamic(this, &ABotanicusPlayerController::RefreshHeldBotanistBook);
		RefreshHeldBotanistBook(Bar->GetSelectedSlotIndex(), Bar->GetSelectedSlot());
	}
}

void ABotanicusPlayerController::ClearBotanistBookEquipment()
{
	if (HasAuthority())
	{
		if (auto* Bar = BookQuickBar(GetPawn()))
			Bar->OnSelectedSlotChanged.RemoveDynamic(this, &ABotanicusPlayerController::RefreshHeldBotanistBook);
		if (auto* Book = ServerBookSession.Get())
			Book->AbandonReading(*this);
		ServerBookSession.Reset();
		if (auto* Book = ServerHeldBook.Get())
			Book->Destroy();
		ServerHeldBook.Reset();
	}
	if (bLocalBookSessionActive)
		ClientUpdateBotanistBookSession_Implementation(false, false);
}

void ABotanicusPlayerController::RefreshHeldBotanistBook(int32 SlotIndex, FBotanicusQuickBarSlot Slot)
{
	if (!HasAuthority())
		return;
	if (!IsSelectedBook(GetPawn()))
	{
		if (auto* Book = ServerHeldBook.Get())
			Book->Destroy();
		ServerHeldBook.Reset();
		return;
	}
	if (ServerHeldBook.IsValid())
		return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Book = GetWorld()->SpawnActor<ABotanicusBotanistBookActor>(
		ABotanicusBotanistBookActor::ResolveBookClass(), GetPawn()->GetActorTransform(), Params);
	if (Book)
	{
		ServerHeldBook = Book;
		Book->SetHeldPresentation(*GetPawn());
	}
}

void ABotanicusPlayerController::Destroyed()
{
	if (auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ABotanicusGameMode>() : nullptr)
		Mode->ReturnBotanistBookFrom(this);
	ClearBotanistBookEquipment();
	Super::Destroyed();
}

bool ABotanicusPlayerController::ResolveBotanistBookPlacement(const FVector& RequestedLocation,
	float RequestedYaw, FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	if (!GetPawn() || !GetWorld() || RequestedLocation.ContainsNaN() || !FMath::IsFinite(RequestedYaw))
		return false;

	const auto* Defaults = ABotanicusBotanistBookActor::ResolveBookClass()->GetDefaultObject<ABotanicusBotanistBookActor>();
	const FBox Bounds = Defaults->GetBookLocalBounds();
	if (const auto Furniture = ResolveFurniturePlacement(ABotanicusBotanistBookActor::BookKey(),
		RequestedYaw, [&Bounds] { return Bounds; }, FVector::OneVector, nullptr, OutTransform); Furniture.IsSet())
	{
		return Furniture.GetValue();
	}
	const FQuat Rotation = FRotator(0, RequestedYaw, 0).Quaternion();
	OutTransform = FTransform(Rotation, RequestedLocation);
	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BotanicusBookPlacement), false, GetPawn());
	if (IsValid(LocalQuickBarItemPreview))
		Query.AddIgnoredActor(LocalQuickBarItemPreview);
	FHitResult Hit;
	const bool bSurface = GetWorld()->LineTraceSingleByChannel(Hit, Eye,
		Eye + View.Vector() * MaximumQuickBarItemPlacementDistance, ECC_Visibility, Query) &&
		Hit.ImpactNormal.Z >= 0.65f &&
		!Cast<ABotanicusPlaceableItemActor>(Hit.GetActor());
	if (!bSurface)
		return false;
	OutTransform.SetLocation(Hit.ImpactPoint + FVector(0, 0, 0.5 - Bounds.Min.Z));
	const FVector HalfExtent = (Bounds.GetExtent() - FVector(0.25)).ComponentMax(FVector(0.25));
	const FVector Center = OutTransform.TransformPosition(Bounds.GetCenter());
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	return !GetWorld()->OverlapAnyTestByObjectType(Center, Rotation, Objects,
		FCollisionShape::MakeBox(HalfExtent), Query);
}
