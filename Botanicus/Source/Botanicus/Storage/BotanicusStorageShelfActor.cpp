// Copyright Epic Games, Inc. All Rights Reserved.

#include "Storage/BotanicusStorageShelfActor.h"

#include "BotanicusCharacter.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName FloorShelfSmallKey(TEXT("StorageShelfFloorSmall"));
	const FName FloorShelfLargeKey(TEXT("StorageShelfFloorLarge"));
	const FName WallShelfSmallKey(TEXT("StorageShelfWallSmall"));
	const FName WallShelfLargeKey(TEXT("StorageShelfWallLarge"));
}

ABotanicusStorageShelfActor::ABotanicusStorageShelfActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetInteractionIndicatorVisibility(false);
	InteractionName =
		NSLOCTEXT("BotanicusStorage", "StorageShelf", "Etagere de stockage");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		GreenMarkerMaterialFinder(
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_Can_Build.MI_Can_Build"));
	UMaterialInterface* GreenMarkerMaterial =
		GreenMarkerMaterialFinder.Succeeded()
			? GreenMarkerMaterialFinder.Object
			: nullptr;
	if (Cube)
	{
		Mesh->SetStaticMesh(Cube);
	}
	const FVector DefaultSlotPositions[] =
	{
		FVector(-12.0f, -50.0f, -60.0f),
		FVector(-12.0f, 50.0f, -60.0f),
		FVector(-12.0f, -50.0f, -165.0f),
		FVector(-12.0f, 50.0f, -165.0f)
	};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Marker =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("StorageSlot%d"), Index + 1));
		Marker->SetupAttachment(SceneRoot);
		Marker->SetStaticMesh(Cube);
		Marker->SetRelativeLocation(DefaultSlotPositions[Index]);
		Marker->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.025f));
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Marker->SetVisibility(false);
		Marker->bEditableWhenInherited = true;
		Marker->ComponentTags.AddUnique(TEXT("StorageSlot"));
		if (GreenMarkerMaterial)
		{
			Marker->SetMaterial(0, GreenMarkerMaterial);
		}
		SlotMarkers.Add(Marker);
	}
}

void ABotanicusStorageShelfActor::OnConstruction(
	const FTransform& Transform)
{
	// Blueprint shelf children own their mesh, materials and transforms. Keep
	// native fallback shelves catalogue-driven for backwards compatibility.
	if (UsesBlueprintAuthoredSlots())
	{
		bUseBlueprintAppearance = true;
	}
	Super::OnConstruction(Transform);
	RefreshShelfConfiguration();
}

void ABotanicusStorageShelfActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshShelfConfiguration();
}

void ABotanicusStorageShelfActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APlayerController* LocalController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(
			LocalController ? LocalController->GetPawn() : nullptr);
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (Character &&
		QuickBar &&
		!QuickBar->GetSelectedSlot().IsEmpty() &&
		FVector::DistSquared(
			Character->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(380.0f) &&
		IsCatalogItemCompatible(
			this,
			QuickBar->GetSelectedSlot().ItemKey))
	{
		ShowAvailableSlotsForLocalPlayer(
			QuickBar->GetSelectedSlot().ItemKey);
	}

	RefreshSlotPreviewVisibility();
	if (HasAuthority() && GetWorld() &&
		GetWorld()->GetTimeSeconds() >= NextLegacySlotMigrationTime)
	{
		NextLegacySlotMigrationTime =
			GetWorld()->GetTimeSeconds() + 0.5f;
		MigrateLegacyCenteredItems();
	}
}

bool ABotanicusStorageShelfActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return (!HasStoredItems() || bMoveContentsWithFurniture) &&
		Super::CanInteract_Implementation(Interactor);
}

FBotanicusInteractionPrompt
ABotanicusStorageShelfActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt =
		Super::GetInteractionPrompt_Implementation(Interactor);
	if (HasStoredItems() && !bMoveContentsWithFurniture)
	{
		Prompt.ActionText =
			NSLOCTEXT("BotanicusStorage", "EmptyShelfFirst", "Vider d'abord");
		Prompt.bCanInteract = false;
	}
	return Prompt;
}

void ABotanicusStorageShelfActor::BeginPlacement(
	ABotanicusCharacter* Character)
{
	if (bMoveContentsWithFurniture &&
		MovingStoredItems.IsEmpty())
	{
		CaptureStoredItemTransforms();
	}
	Super::BeginPlacement(Character);
	ApplyStoredItemTransforms();
}

void ABotanicusStorageShelfActor::UpdatePlacement(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	Super::UpdatePlacement(PlacementTransform, bIsValid);
	ApplyStoredItemTransforms();
}

void ABotanicusStorageShelfActor::ConfirmPlacement()
{
	Super::ConfirmPlacement();
	ApplyStoredItemTransforms();
	ClearStoredItemTransforms();
	bMoveContentsWithFurniture = false;
}

void ABotanicusStorageShelfActor::CancelPlacement()
{
	Super::CancelPlacement();
	ApplyStoredItemTransforms();
	ClearStoredItemTransforms();
	bMoveContentsWithFurniture = false;
}

void ABotanicusStorageShelfActor::SetLocalPlacementPreview(
	const FTransform& PlacementTransform,
	bool bIsValid)
{
	if (bMoveContentsWithFurniture &&
		MovingStoredItems.IsEmpty())
	{
		CaptureStoredItemTransforms();
	}
	Super::SetLocalPlacementPreview(PlacementTransform, bIsValid);
	ApplyStoredItemTransforms();
}

FVector ABotanicusStorageShelfActor::GetPlacementBoxExtent() const
{
	return ConfiguredExtent;
}

bool ABotanicusStorageShelfActor::IsCatalogItemCompatible(
	const UObject* Context,
	FName InItemKey)
{
	return GetStorageStackLimit(InItemKey) > 0;
}

int32 ABotanicusStorageShelfActor::GetStorageStackLimit(
	FName InItemKey)
{
	if (InItemKey.ToString().StartsWith(TEXT("SeedPacket_")))
	{
		return 20;
	}
	if (InItemKey == TEXT("PlantPot") ||
		InItemKey == TEXT("PlantPotSquare") ||
		InItemKey == TEXT("SalePot") ||
		InItemKey == TEXT("SalePotSquare"))
	{
		return 1;
	}
	return 0;
}

bool ABotanicusStorageShelfActor::IsWallMountedShelf() const
{
	if (GetItemKey() == WallShelfSmallKey ||
		GetItemKey() == WallShelfLargeKey)
	{
		return true;
	}
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UBotanicusItemCatalogSubsystem* Catalog =
				GameInstance->GetSubsystem<UBotanicusItemCatalogSubsystem>())
		{
			if (const FBotanicusItemDefinition* Definition =
					Catalog->FindItem(GetItemKey()))
			{
				return (Definition->AllowedPlacementSurfaces &
					static_cast<int32>(EBotanicusPlacementSurface::Wall)) != 0;
			}
		}
	}

	// Fallback used while a preview actor has not received its catalogue key.
	// Match the family name instead of one exact asset name so renamed bases
	// and all of their Blueprint children keep the correct behaviour.
	for (const UClass* Class = GetClass();
		 Class;
		 Class = Class->GetSuperClass())
	{
		if (Class->GetName().Contains(TEXT("StorageShelfWall")))
		{
			return true;
		}
	}
	return false;
}

int32 ABotanicusStorageShelfActor::GetStorageSlotCount() const
{
	if (UsesBlueprintAuthoredSlots())
	{
		TArray<UStaticMeshComponent*> AuthoredMarkers;
		GetOrderedBlueprintSlotMarkers(AuthoredMarkers);
		return AuthoredMarkers.Num();
	}
	return SlotBaseLocations.Num();
}

bool ABotanicusStorageShelfActor::UsesBlueprintAuthoredSlots() const
{
	// The native class retains its procedural fallback layout. Any Blueprint
	// child is authored directly in Unreal: its StorageSlot components are the
	// sole source of truth for both capacity and placement transforms.
	return GetClass() != ABotanicusStorageShelfActor::StaticClass();
}

void ABotanicusStorageShelfActor::GetOrderedBlueprintSlotMarkers(
	TArray<UStaticMeshComponent*>& OutMarkers) const
{
	OutMarkers.Reset();
	TArray<UStaticMeshComponent*> Components;
	GetComponents<UStaticMeshComponent>(Components);
	for (UStaticMeshComponent* Component : Components)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const FString ComponentName = Component->GetName();
		if (ComponentName.StartsWith(TEXT("StorageSlot")) ||
			Component->ComponentHasTag(TEXT("StorageSlot")))
		{
			OutMarkers.Add(Component);
		}
	}

	auto GetSortIndex = [](const UStaticMeshComponent& Component)
	{
		const FString Prefix(TEXT("StorageSlot"));
		const FString Name = Component.GetName();
		return Name.StartsWith(Prefix)
			? FCString::Atoi(*Name.RightChop(Prefix.Len()))
			: MAX_int32;
	};
	OutMarkers.Sort(
		[&GetSortIndex](
			const UStaticMeshComponent& Left,
			const UStaticMeshComponent& Right)
		{
			const int32 LeftIndex = GetSortIndex(Left);
			const int32 RightIndex = GetSortIndex(Right);
			return LeftIndex == RightIndex
				? Left.GetName().Compare(
					Right.GetName(), ESearchCase::IgnoreCase) < 0
				: LeftIndex < RightIndex;
		});
}

bool ABotanicusStorageShelfActor::HasStoredItems() const
{
	for (int32 SlotIndex = 0;
		 SlotIndex < GetStorageSlotCount();
		 ++SlotIndex)
	{
		if (GetStoredItemInSlot(SlotIndex))
		{
			return true;
		}
	}
	return false;
}

void ABotanicusStorageShelfActor::SetMoveContentsWithFurniture(
	bool bEnabled)
{
	bMoveContentsWithFurniture = bEnabled;
	if (bEnabled)
	{
		CaptureStoredItemTransforms();
	}
	else
	{
		ClearStoredItemTransforms();
	}
}

void ABotanicusStorageShelfActor::GetStoredItems(
	TArray<ABotanicusPlaceableItemActor*>& OutItems) const
{
	OutItems.Reset();
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<ABotanicusPlaceableItemActor> It(GetWorld());
		 It;
		 ++It)
	{
		if (It->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}
		if (FindStorageSlotIndexForItem(*It) != INDEX_NONE)
		{
			OutItems.AddUnique(*It);
		}
	}
}

bool ABotanicusStorageShelfActor::FindClosestAvailableStorageSlot(
	FName InItemKey,
	const FVector& ReferenceLocation,
	const FVector& ItemExtent,
	FTransform& OutTransform,
	const AActor* IgnoredItem,
	int32* OutSlotIndex,
	ABotanicusPlaceableItemActor** OutExistingStack,
	int32 InQuantity) const
{
	if (OutSlotIndex)
	{
		*OutSlotIndex = INDEX_NONE;
	}
	if (OutExistingStack)
	{
		*OutExistingStack = nullptr;
	}
	if (!IsCatalogItemCompatible(this, InItemKey))
	{
		return false;
	}

	bool bFound = false;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (int32 SlotIndex = 0;
		 SlotIndex < GetStorageSlotCount();
		 ++SlotIndex)
	{
		FTransform Candidate;
		ABotanicusPlaceableItemActor* ExistingStack = nullptr;
		if (!GetStorageSlotPlacement(
				SlotIndex,
				InItemKey,
				ItemExtent,
				Candidate,
				IgnoredItem,
				&ExistingStack,
				InQuantity))
		{
			continue;
		}
		const float DistanceSquared =
			FVector::DistSquared(
				ReferenceLocation,
				Candidate.GetLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			OutTransform = Candidate;
			bFound = true;
			if (OutSlotIndex)
			{
				*OutSlotIndex = SlotIndex;
			}
			if (OutExistingStack)
			{
				*OutExistingStack = ExistingStack;
			}
		}
	}
	return bFound;
}

void ABotanicusStorageShelfActor::ShowAvailableSlotsForLocalPlayer(
	FName InItemKey,
	const AActor* IgnoredItem)
{
	if (!IsCatalogItemCompatible(this, InItemKey))
	{
		return;
	}
	LocallyPreviewedItemKey = InItemKey;
	LocallyIgnoredItem = IgnoredItem;
	LastLocalPreviewTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void ABotanicusStorageShelfActor::OnEquipmentDefinitionApplied()
{
	RefreshShelfConfiguration();
}

void ABotanicusStorageShelfActor::RefreshShelfConfiguration()
{
	const FName Key = GetItemKey();
	const bool bWall = IsWallMountedShelf();
	const bool bUsesAuthoredSlots = UsesBlueprintAuthoredSlots();
	const bool bCustomVisual =
		UsesBlueprintAppearance() || IsUsingItemDataMesh();
	const bool bLarge =
		Key == FloorShelfLargeKey || Key == WallShelfLargeKey;

	SlotBaseLocations.Reset();
	if (bWall)
	{
		ConfiguredExtent = bLarge
			? FVector(18.0f, 95.0f, 48.0f)
			: FVector(16.0f, 65.0f, 25.0f);
		const int32 Columns = 3;
		const int32 Rows = bLarge ? 2 : 1;
		const float SpacingY = bLarge ? 58.0f : 45.0f;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				SlotBaseLocations.Add(
					FVector(
						ConfiguredExtent.X + 8.0f,
						(Column - 1) * SpacingY,
						(Row - (Rows - 1) * 0.5f) * 58.0f));
			}
		}
	}
	else
	{
		ConfiguredExtent =
			Key == FloorShelfSmallKey
			? FVector(45.0f, 103.0f, 100.0f)
			: bLarge
				? FVector(35.0f, 125.0f, 90.0f)
				: FVector(30.0f, 85.0f, 75.0f);
		const int32 Columns = bLarge ? 4 : 2;
		const int32 Rows = 2;
		const float SpacingY = bLarge
			? 58.0f
			: Key == FloorShelfSmallKey
				? 100.0f
				: 70.0f;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				SlotBaseLocations.Add(
					FVector(
						// The authored shelf has a solid back.  Its usable
						// display surface is toward local +X, so keeping the
						// item at X=0 embeds it in the cabinet and makes it
						// look as if it vanished after being stored.
						Key == FloorShelfSmallKey ? 36.0f : 8.0f,
						(Column - (Columns - 1) * 0.5f) * SpacingY,
						Key == FloorShelfSmallKey
							? -42.0f + Row * 74.0f
							: -25.0f + Row * 75.0f));
			}
		}
	}

	if (bUsesAuthoredSlots)
	{
		SlotBaseLocations.Reset();
		TArray<UStaticMeshComponent*> AuthoredMarkers;
		GetOrderedBlueprintSlotMarkers(AuthoredMarkers);
		for (UStaticMeshComponent* SlotMarker : AuthoredMarkers)
		{
			SlotMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SlotBaseLocations.Add(SlotMarker->GetRelativeLocation());
		}
	}

	// For a Blueprint shelf, Mesh is fully controlled from the Blueprint
	// viewport. Only the native fallback receives a generated transform.
	if (!UsesBlueprintAppearance())
	{
		Mesh->SetRelativeLocation(
			bCustomVisual
				? FVector::ZeroVector
				: FVector(-ConfiguredExtent.X + 5.0f, 0.0f, 0.0f));
		Mesh->SetRelativeRotation(FRotator::ZeroRotator);
		Mesh->SetRelativeScale3D(
			bCustomVisual
				? FVector::OneVector
				: FVector(
					0.10f,
					ConfiguredExtent.Y / 50.0f,
					ConfiguredExtent.Z / 50.0f));
	}

	TArray<UStaticMeshComponent*> VisibleMarkers;
	if (bUsesAuthoredSlots)
	{
		GetOrderedBlueprintSlotMarkers(VisibleMarkers);
	}
	else
	{
		for (UStaticMeshComponent* Marker : SlotMarkers)
		{
			VisibleMarkers.Add(Marker);
		}
	}
	for (int32 Index = 0; Index < VisibleMarkers.Num(); ++Index)
	{
		VisibleMarkers[Index]->SetVisibility(bShowSlotMarkersInGame);
		if (!bUsesAuthoredSlots &&
			SlotBaseLocations.IsValidIndex(Index))
		{
			VisibleMarkers[Index]->SetRelativeLocation(
				SlotBaseLocations[Index]);
		}
	}
}

void ABotanicusStorageShelfActor::RefreshSlotPreviewVisibility()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bPreviewActive =
		Now - LastLocalPreviewTime <= 0.15f &&
		IsCatalogItemCompatible(this, LocallyPreviewedItemKey);
	TArray<UStaticMeshComponent*> PreviewMarkers;
	if (UsesBlueprintAuthoredSlots())
	{
		GetOrderedBlueprintSlotMarkers(PreviewMarkers);
	}
	else
	{
		for (UStaticMeshComponent* Marker : SlotMarkers)
		{
			PreviewMarkers.Add(Marker);
		}
	}
	for (int32 SlotIndex = 0; SlotIndex < PreviewMarkers.Num(); ++SlotIndex)
	{
		const bool bAvailable =
			bPreviewActive &&
			SlotIndex < GetStorageSlotCount() &&
			CanSlotAcceptItem(
				SlotIndex,
				LocallyPreviewedItemKey,
				LocallyIgnoredItem.Get(),
				1);
		PreviewMarkers[SlotIndex]->SetVisibility(
			bShowSlotMarkersInGame || bAvailable);
	}
}

FVector ABotanicusStorageShelfActor::GetSlotBaseLocalLocation(
	int32 SlotIndex) const
{
	if (UsesBlueprintAuthoredSlots())
	{
		TArray<UStaticMeshComponent*> AuthoredMarkers;
		GetOrderedBlueprintSlotMarkers(AuthoredMarkers);
		if (AuthoredMarkers.IsValidIndex(SlotIndex))
		{
			return AuthoredMarkers[SlotIndex]->GetRelativeLocation();
		}
	}
	return SlotBaseLocations.IsValidIndex(SlotIndex)
		? SlotBaseLocations[SlotIndex]
		: FVector::ZeroVector;
}

FTransform ABotanicusStorageShelfActor::GetStorageSlotTransform(
	int32 SlotIndex,
	const FVector& ItemExtent) const
{
	// The Blueprint marker represents the supporting surface, not the item's
	// centre.  Lift the actor by its half-height so the bottom of every stored
	// mesh rests on the marker plate.
	const FVector LocalLocation =
		GetSlotBaseLocalLocation(SlotIndex);
	FQuat WorldRotation = GetActorQuat();
	TArray<UStaticMeshComponent*> AuthoredMarkers;
	if (UsesBlueprintAuthoredSlots())
	{
		GetOrderedBlueprintSlotMarkers(AuthoredMarkers);
	}
	if (AuthoredMarkers.IsValidIndex(SlotIndex))
	{
		WorldRotation =
			GetActorQuat() *
			AuthoredMarkers[SlotIndex]->GetRelativeRotation().Quaternion();
	}
	const FVector SurfaceLocation =
		GetActorTransform().TransformPosition(LocalLocation);
	const FVector ItemLocation =
		SurfaceLocation +
		WorldRotation.GetUpVector() * FMath::Max(0.0f, ItemExtent.Z);
	return FTransform(WorldRotation, ItemLocation);
}

int32 ABotanicusStorageShelfActor::FindStorageSlotIndexForItem(
	const ABotanicusPlaceableItemActor* Item) const
{
	if (!IsValid(Item) ||
		!IsCatalogItemCompatible(this, Item->GetItemKey()) ||
		Item->ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		return INDEX_NONE;
	}

	const FVector ItemExtent =
		Item->GetPlacementBoxExtent().GetAbs();
	int32 BestSlotIndex = INDEX_NONE;
	float BestDistanceSquared = FMath::Square(70.0f);
	for (int32 SlotIndex = 0;
		 SlotIndex < GetStorageSlotCount();
		 ++SlotIndex)
	{
		const float DistanceSquared = FVector::DistSquared(
			Item->GetActorLocation(),
			GetStorageSlotTransform(
				SlotIndex,
				ItemExtent).GetLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestSlotIndex = SlotIndex;
		}
	}
	return BestSlotIndex;
}

int32 ABotanicusStorageShelfActor::FindClosestStorageSlotIndex(
	const FVector& WorldLocation,
	float MaximumDistance) const
{
	int32 BestSlotIndex = INDEX_NONE;
	float BestDistanceSquared = FMath::Square(MaximumDistance);
	for (int32 SlotIndex = 0;
		 SlotIndex < GetStorageSlotCount();
		 ++SlotIndex)
	{
		const FVector SlotLocation =
			GetActorTransform().TransformPosition(
				GetSlotBaseLocalLocation(SlotIndex));
		const float DistanceSquared = FVector::DistSquared(
			WorldLocation,
			SlotLocation);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestSlotIndex = SlotIndex;
		}
	}
	return BestSlotIndex;
}

FVector ABotanicusStorageShelfActor::GetStorageSlotAimLocation(
	int32 SlotIndex) const
{
	return GetActorTransform().TransformPosition(
		GetSlotBaseLocalLocation(SlotIndex));
}

bool ABotanicusStorageShelfActor::GetStorageSlotPlacement(
	int32 SlotIndex,
	FName InItemKey,
	const FVector& ItemExtent,
	FTransform& OutTransform,
	const AActor* IgnoredItem,
	ABotanicusPlaceableItemActor** OutExistingStack,
	int32 InQuantity) const
{
	if (OutExistingStack)
	{
		*OutExistingStack = nullptr;
	}
	if (SlotIndex < 0 || SlotIndex >= GetStorageSlotCount() ||
		!CanSlotAcceptItem(
			SlotIndex,
			InItemKey,
			IgnoredItem,
			InQuantity))
	{
		return false;
	}

	ABotanicusPlaceableItemActor* ExistingStack =
		GetStoredItemInSlot(SlotIndex, IgnoredItem);
	// The slot anchor remains authoritative even for an existing stack.
	// Otherwise a stack created with an older/bad layout permanently keeps
	// that stale transform and every later deposit appears to vanish there.
	OutTransform = GetStorageSlotTransform(SlotIndex, ItemExtent);
	if (OutExistingStack)
	{
		*OutExistingStack = ExistingStack;
	}
	return true;
}

ABotanicusPlaceableItemActor*
ABotanicusStorageShelfActor::GetStoredItemInSlot(
	int32 SlotIndex,
	const AActor* IgnoredItem) const
{
	if (!GetWorld() || SlotIndex < 0 ||
		SlotIndex >= GetStorageSlotCount())
	{
		return nullptr;
	}
	for (TActorIterator<ABotanicusPlaceableItemActor> It(GetWorld());
		 It;
		 ++It)
	{
		if (*It == IgnoredItem ||
			It->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}
		if (FindStorageSlotIndexForItem(*It) == SlotIndex)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ABotanicusStorageShelfActor::CanSlotAcceptItem(
	int32 SlotIndex,
	FName InItemKey,
	const AActor* IgnoredItem,
	int32 InQuantity) const
{
	const int32 StackLimit =
		GetStorageStackLimit(InItemKey);
	if (StackLimit <= 0)
	{
		return false;
	}
	const ABotanicusPlaceableItemActor* ExistingItem =
		GetStoredItemInSlot(SlotIndex, IgnoredItem);
	return !ExistingItem ||
		(ExistingItem->GetItemKey() == InItemKey &&
		 ExistingItem->GetQuantity() +
			 FMath::Max(1, InQuantity) <=
			 StackLimit);
}

void ABotanicusStorageShelfActor::CaptureStoredItemTransforms()
{
	MovingStoredItems.Reset();
	MovingStoredItemRelativeTransforms.Reset();
	TArray<ABotanicusPlaceableItemActor*> StoredItems;
	GetStoredItems(StoredItems);
	for (ABotanicusPlaceableItemActor* Item : StoredItems)
	{
		if (!IsValid(Item))
		{
			continue;
		}
		MovingStoredItems.Add(Item);
		MovingStoredItemRelativeTransforms.Add(
			Item->GetActorTransform().GetRelativeTransform(
				GetActorTransform()));
	}
}

void ABotanicusStorageShelfActor::ApplyStoredItemTransforms()
{
	for (int32 Index = 0;
		 MovingStoredItems.IsValidIndex(Index) &&
		 MovingStoredItemRelativeTransforms.IsValidIndex(Index);
		 ++Index)
	{
		ABotanicusPlaceableItemActor* Item =
			MovingStoredItems[Index].Get();
		if (!IsValid(Item))
		{
			continue;
		}
		Item->SetActorTransform(
			MovingStoredItemRelativeTransforms[Index] *
				GetActorTransform(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (HasAuthority())
		{
			Item->SetNetDormancy(DORM_Awake);
			Item->FlushNetDormancy();
			Item->ForceNetUpdate();
		}
	}
}

void ABotanicusStorageShelfActor::MigrateLegacyCenteredItems()
{
	if (!GetWorld() || !UsesBlueprintAuthoredSlots())
	{
		return;
	}

	for (TActorIterator<ABotanicusPlaceableItemActor> It(GetWorld());
		 It;
		 ++It)
	{
		ABotanicusPlaceableItemActor* Item = *It;
		if (!IsValid(Item) ||
			!IsCatalogItemCompatible(this, Item->GetItemKey()) ||
			Item->ActorHasTag(TEXT("BotanicusPlacementPreview")))
		{
			continue;
		}

		const int32 SlotIndex = FindStorageSlotIndexForItem(Item);
		if (SlotIndex == INDEX_NONE)
		{
			continue;
		}

		const FTransform DesiredTransform = GetStorageSlotTransform(
			SlotIndex,
			Item->GetPlacementBoxExtent().GetAbs());
		if (Item->GetActorTransform().Equals(DesiredTransform, 0.5f))
		{
			continue;
		}

		Item->SetActorTransform(
			DesiredTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Item->SetNetDormancy(DORM_Awake);
		Item->FlushNetDormancy();
		Item->ForceNetUpdate();
	}
}

void ABotanicusStorageShelfActor::ClearStoredItemTransforms()
{
	MovingStoredItems.Reset();
	MovingStoredItemRelativeTransforms.Reset();
}
