// Copyright Epic Games, Inc. All Rights Reserved.

#include "Storage/BotanicusStorageShelfActor.h"

#include "BotanicusCharacter.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Board =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("StorageShelfBoard%d"), Index));
		Board->SetupAttachment(SceneRoot);
		Board->SetStaticMesh(Cube);
		Board->SetCollisionProfileName(TEXT("BlockAll"));
		ShelfBoards.Add(Board);
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UStaticMeshComponent* Panel =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("StorageShelfSide%d"), Index));
		Panel->SetupAttachment(SceneRoot);
		Panel->SetStaticMesh(Cube);
		Panel->SetCollisionProfileName(TEXT("BlockAll"));
		SidePanels.Add(Panel);
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		UStaticMeshComponent* Marker =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("StorageSlot%d"), Index));
		Marker->SetupAttachment(SceneRoot);
		Marker->SetStaticMesh(Cube);
		Marker->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.025f));
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Marker->SetVisibility(false);
		if (GreenMarkerMaterial)
		{
			Marker->SetMaterial(0, GreenMarkerMaterial);
		}
		SlotMarkers.Add(Marker);
	}

	ShelfLabel =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("StorageShelfLabel"));
	ShelfLabel->SetupAttachment(SceneRoot);
	ShelfLabel->SetHorizontalAlignment(EHTA_Center);
	ShelfLabel->SetVerticalAlignment(EVRTA_TextCenter);
	ShelfLabel->SetWorldSize(14.0f);
	ShelfLabel->SetTextRenderColor(FColor(80, 255, 120));
	ShelfLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
	RefreshLocalStockLabel();
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
	return GetItemKey() == WallShelfSmallKey ||
		GetItemKey() == WallShelfLargeKey;
}

int32 ABotanicusStorageShelfActor::GetStorageSlotCount() const
{
	return SlotBaseLocations.Num();
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
	const bool bCustomVisual = IsUsingItemDataMesh();
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
		ConfiguredExtent = bLarge
			? FVector(35.0f, 125.0f, 90.0f)
			: FVector(30.0f, 85.0f, 75.0f);
		const int32 Columns = bLarge ? 4 : 2;
		const int32 Rows = 2;
		const float SpacingY = bLarge ? 58.0f : 70.0f;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				SlotBaseLocations.Add(
					FVector(
						8.0f,
						(Column - (Columns - 1) * 0.5f) * SpacingY,
						-25.0f + Row * 75.0f));
			}
		}
	}

	Mesh->SetRelativeLocation(
		bCustomVisual
			? FVector::ZeroVector
			: FVector(-ConfiguredExtent.X + 5.0f, 0.0f, 0.0f));
	Mesh->SetRelativeScale3D(
		bCustomVisual
			? FVector::OneVector
			: FVector(
				0.10f,
				ConfiguredExtent.Y / 50.0f,
				ConfiguredExtent.Z / 50.0f));

	for (int32 Index = 0; Index < ShelfBoards.Num(); ++Index)
	{
		const bool bVisible =
			!bCustomVisual &&
			Index < (bWall ? (bLarge ? 2 : 1) : 3);
		ShelfBoards[Index]->SetVisibility(bVisible);
		ShelfBoards[Index]->SetCollisionEnabled(
			bVisible
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
		if (bVisible)
		{
			const float BoardZ = bWall
				? (-ConfiguredExtent.Z + 10.0f + Index * 58.0f)
				: (-ConfiguredExtent.Z + 45.0f + Index * 75.0f);
			ShelfBoards[Index]->SetRelativeLocation(
				FVector(6.0f, 0.0f, BoardZ));
			ShelfBoards[Index]->SetRelativeScale3D(
				FVector(
					ConfiguredExtent.X / 50.0f,
					ConfiguredExtent.Y / 50.0f,
					0.06f));
		}
	}
	for (int32 Index = 0; Index < SidePanels.Num(); ++Index)
	{
		SidePanels[Index]->SetVisibility(!bCustomVisual);
		SidePanels[Index]->SetCollisionEnabled(
			bCustomVisual
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryAndPhysics);
		SidePanels[Index]->SetRelativeLocation(
			FVector(
				0.0f,
				Index == 0
					? ConfiguredExtent.Y - 4.0f
					: -ConfiguredExtent.Y + 4.0f,
				0.0f));
		SidePanels[Index]->SetRelativeScale3D(
			FVector(
				ConfiguredExtent.X / 50.0f,
				0.08f,
				ConfiguredExtent.Z / 50.0f));
	}

	for (int32 Index = 0; Index < SlotMarkers.Num(); ++Index)
	{
		SlotMarkers[Index]->SetVisibility(false);
		if (SlotBaseLocations.IsValidIndex(Index))
		{
			SlotMarkers[Index]->SetRelativeLocation(
				SlotBaseLocations[Index] + FVector(0.0f, 0.0f, 2.0f));
		}
	}
	if (ShelfLabel)
	{
		ShelfLabel->SetRelativeLocation(
			FVector(
				ConfiguredExtent.X + 10.0f,
				0.0f,
				ConfiguredExtent.Z + 25.0f));
		ShelfLabel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("%s - %d PLACES"),
					bWall ? TEXT("ETAGERE MURALE") : TEXT("ETAGERE AU SOL"),
					GetStorageSlotCount())));
	}
}

void ABotanicusStorageShelfActor::RefreshSlotPreviewVisibility()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bPreviewActive =
		Now - LastLocalPreviewTime <= 0.15f &&
		IsCatalogItemCompatible(this, LocallyPreviewedItemKey);
	for (int32 SlotIndex = 0; SlotIndex < SlotMarkers.Num(); ++SlotIndex)
	{
		const bool bAvailable =
			bPreviewActive &&
			SlotIndex < GetStorageSlotCount() &&
			CanSlotAcceptItem(
				SlotIndex,
				LocallyPreviewedItemKey,
				LocallyIgnoredItem.Get(),
				1);
		SlotMarkers[SlotIndex]->SetVisibility(bAvailable);
	}
}

void ABotanicusStorageShelfActor::RefreshLocalStockLabel()
{
	if (!ShelfLabel || !GetWorld())
	{
		return;
	}

	const APlayerController* LocalController =
		GetWorld()->GetFirstPlayerController();
	const APawn* LocalPawn =
		LocalController ? LocalController->GetPawn() : nullptr;
	bool bShowStock = false;
	if (LocalController && LocalPawn &&
		FVector::DistSquared(
			LocalPawn->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(650.0f))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		LocalController->GetPlayerViewPoint(
			ViewLocation,
			ViewRotation);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(BotanicusShelfStockLabel),
			false,
			LocalPawn);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(
				Hit,
				ViewLocation,
				ViewLocation + ViewRotation.Vector() * 650.0f,
				ECC_Visibility,
				QueryParams))
		{
			if (Hit.GetActor() == this)
			{
				bShowStock = true;
			}
			else if (const ABotanicusPlaceableItemActor* HitItem =
				Cast<ABotanicusPlaceableItemActor>(
					Hit.GetActor()))
			{
				bShowStock =
					FindStorageSlotIndexForItem(HitItem) !=
						INDEX_NONE;
			}
		}
	}

	const FString ShelfTitle = FString::Printf(
		TEXT("%s - %d PLACES"),
		IsWallMountedShelf()
			? TEXT("ETAGERE MURALE")
			: TEXT("ETAGERE AU SOL"),
		GetStorageSlotCount());
	if (!bShowStock)
	{
		ShelfLabel->SetWorldSize(14.0f);
		ShelfLabel->SetText(FText::FromString(ShelfTitle));
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	FString StockText = ShelfTitle + TEXT("\n");
	bool bHasStock = false;
	for (int32 SlotIndex = 0;
		 SlotIndex < GetStorageSlotCount();
		 ++SlotIndex)
	{
		const ABotanicusPlaceableItemActor* Item =
			GetStoredItemInSlot(SlotIndex);
		if (!Item)
		{
			continue;
		}

		bHasStock = true;
		const FBotanicusItemDefinition* Definition =
			Catalog ? Catalog->FindItem(Item->GetItemKey()) : nullptr;
		const FString ItemName = Definition
			? Definition->DisplayName.ToString()
			: Item->GetItemKey().ToString();
		StockText += FString::Printf(
			TEXT("\n%d. %s  x%d/%d"),
			SlotIndex + 1,
			*ItemName,
			Item->GetQuantity(),
			GetStorageStackLimit(Item->GetItemKey()));
	}
	if (!bHasStock)
	{
		StockText += TEXT("\n\nVIDE");
	}
	ShelfLabel->SetWorldSize(11.0f);
	ShelfLabel->SetText(FText::FromString(StockText));
}

FVector ABotanicusStorageShelfActor::GetSlotBaseLocalLocation(
	int32 SlotIndex) const
{
	return SlotBaseLocations.IsValidIndex(SlotIndex)
		? SlotBaseLocations[SlotIndex]
		: FVector::ZeroVector;
}

FTransform ABotanicusStorageShelfActor::GetStorageSlotTransform(
	int32 SlotIndex,
	const FVector& ItemExtent) const
{
	const FVector LocalLocation =
		GetSlotBaseLocalLocation(SlotIndex) +
		FVector(0.0f, 0.0f, FMath::Max(3.0f, ItemExtent.Z) + 3.0f);
	return FTransform(
		GetActorRotation(),
		GetActorTransform().TransformPosition(LocalLocation));
}

int32 ABotanicusStorageShelfActor::FindStorageSlotIndexForItem(
	const ABotanicusPlaceableItemActor* Item) const
{
	if (!IsValid(Item) ||
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
		GetSlotBaseLocalLocation(SlotIndex) +
			FVector(0.0f, 0.0f, 28.0f));
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
	if (!SlotBaseLocations.IsValidIndex(SlotIndex) ||
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
	OutTransform = ExistingStack
		? ExistingStack->GetActorTransform()
		: GetStorageSlotTransform(SlotIndex, ItemExtent);
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
	if (!GetWorld() || !SlotBaseLocations.IsValidIndex(SlotIndex))
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

void ABotanicusStorageShelfActor::ClearStoredItemTransforms()
{
	MovingStoredItems.Reset();
	MovingStoredItemRelativeTransforms.Reset();
}
