// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/BotanicusRefundZoneActor.h"

#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 RefundPercent = 80;

	bool IsProtectedGameplayItem(
		const FBotanicusItemDefinition& Definition)
	{
		return Definition.Category == EBotanicusItemCategory::Tool ||
			Definition.ItemKey == TEXT("CashRegister") ||
			Definition.ItemKey == TEXT("PreparationWorkbench") ||
			Definition.ItemKey == TEXT("CommandComputer");
	}
}

ABotanicusRefundZoneActor::ABotanicusRefundZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(
		TEXT("RefundZoneBounds"));
	SetRootComponent(ZoneBounds);
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ZoneVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("RefundZoneVisual"));
	ZoneVisual->SetupAttachment(ZoneBounds);
	ZoneVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		ZoneVisual->SetStaticMesh(CubeFinder.Object);
	}

	ZoneLabel = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("RefundZoneLabel"));
	ZoneLabel->SetupAttachment(ZoneBounds);
	ZoneLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
	ZoneLabel->SetHorizontalAlignment(EHTA_Center);
	ZoneLabel->SetVerticalAlignment(EVRTA_TextCenter);
	ZoneLabel->SetWorldSize(22.0f);
	ZoneLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RefreshVisuals();
}

void ABotanicusRefundZoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	RefundScanAccumulator += DeltaSeconds;
	if (RefundScanAccumulator >= 0.25f)
	{
		RefundScanAccumulator = 0.0f;
		ProcessRefundableObjects();
	}
}

void ABotanicusRefundZoneActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusRefundZoneActor, BoxExtent);
}

void ABotanicusRefundZoneActor::InitializeZone(
	const FVector& InBoxExtent)
{
	if (!HasAuthority())
	{
		return;
	}

	BoxExtent = FVector(
		FMath::Max(100.0f, InBoxExtent.X),
		FMath::Max(100.0f, InBoxExtent.Y),
		FMath::Max(2.0f, InBoxExtent.Z));
	DynamicMaterial = nullptr;
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusRefundZoneActor::OnRep_ZoneConfiguration()
{
	DynamicMaterial = nullptr;
	RefreshVisuals();
}

void ABotanicusRefundZoneActor::RefreshVisuals()
{
	if (ZoneBounds)
	{
		ZoneBounds->SetBoxExtent(BoxExtent);
	}
	if (ZoneVisual)
	{
		ZoneVisual->SetRelativeScale3D(
			FVector(
				BoxExtent.X / 50.0f,
				BoxExtent.Y / 50.0f,
				BoxExtent.Z / 50.0f));
		if (!DynamicMaterial)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(
				ZoneVisual->GetMaterial(0),
				this);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(
					TEXT("Color"),
					FLinearColor(0.95f, 0.25f, 0.03f, 1.0f));
				ZoneVisual->SetMaterial(0, DynamicMaterial);
			}
		}
	}
	if (ZoneLabel)
	{
		ZoneLabel->SetText(
			FText::FromString(
				TEXT("REMBOURSEMENT OBJET\n80 % DU PRIX")));
		ZoneLabel->SetTextRenderColor(
			FLinearColor(1.0f, 0.32f, 0.04f, 1.0f).ToFColor(true));
	}
}

bool ABotanicusRefundZoneActor::IsObjectFullyInside(
	const AActor* Actor,
	const FVector& ObjectExtent) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const FVector ActorLocation = Actor->GetActorLocation();
	const float MaximumVerticalDistance =
		FMath::Max(80.0f, ObjectExtent.Z + BoxExtent.Z + 35.0f);
	if (FMath::Abs(ActorLocation.Z - GetActorLocation().Z) >
		MaximumVerticalDistance)
	{
		return false;
	}

	const FVector Forward = Actor->GetActorForwardVector();
	const FVector Right = Actor->GetActorRightVector();
	const FVector Corners[] = {
		ActorLocation + Forward * ObjectExtent.X + Right * ObjectExtent.Y,
		ActorLocation + Forward * ObjectExtent.X - Right * ObjectExtent.Y,
		ActorLocation - Forward * ObjectExtent.X + Right * ObjectExtent.Y,
		ActorLocation - Forward * ObjectExtent.X - Right * ObjectExtent.Y
	};
	for (const FVector& Corner : Corners)
	{
		const FVector LocalCorner =
			GetActorTransform().InverseTransformPosition(Corner);
		if (FMath::Abs(LocalCorner.X) > BoxExtent.X ||
			FMath::Abs(LocalCorner.Y) > BoxExtent.Y)
		{
			return false;
		}
	}
	return true;
}

void ABotanicusRefundZoneActor::ProcessRefundableObjects()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSet<TWeakObjectPtr<AActor>> ObjectsCurrentlyInside;
	for (TActorIterator<ABotanicusPlaceableItemActor> It(World); It; ++It)
	{
		ABotanicusPlaceableItemActor* Item = *It;
		if (!IsValid(Item) ||
			Item->IsActorBeingDestroyed() ||
			Item->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
			!IsObjectFullyInside(Item, Item->GetPlacementBoxExtent().GetAbs()))
		{
			continue;
		}
		ObjectsCurrentlyInside.Add(Item);
		TryRefundObject(
			Item,
			Item->GetItemKey(),
			FMath::Max(1, Item->GetQuantity()));
	}

	for (TActorIterator<ABotanicusLargeEquipmentActor> It(World); It; ++It)
	{
		ABotanicusLargeEquipmentActor* Equipment = *It;
		if (!IsValid(Equipment) ||
			Equipment->IsActorBeingDestroyed() ||
			Equipment->ActorHasTag(TEXT("BotanicusPlacementPreview")) ||
			Equipment->GetCarrier() ||
			Equipment->GetHelper() ||
			Equipment->IsInPlacementMode() ||
			!IsObjectFullyInside(
				Equipment,
				Equipment->GetPlacementBoxExtent().GetAbs()))
		{
			continue;
		}
		ObjectsCurrentlyInside.Add(Equipment);
		TryRefundObject(Equipment, Equipment->GetItemKey(), 1);
	}

	for (auto It = RejectedActors.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || !ObjectsCurrentlyInside.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
}

bool ABotanicusRefundZoneActor::TryRefundObject(
	AActor* Actor,
	FName ItemKey,
	int32 Quantity)
{
	if (!IsValid(Actor) || ItemKey.IsNone())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Definition ||
		Definition->Price <= 0 ||
		IsProtectedGameplayItem(*Definition))
	{
		if (!RejectedActors.Contains(Actor))
		{
			RejectedActors.Add(Actor);
			NotifyPlayers(
				TEXT(
					"Objet indispensable ou non remboursable : vente impossible."));
		}
		return false;
	}

	const int32 Refund =
		FMath::Max(
			1,
			Definition->Price *
				FMath::Max(1, Quantity) *
				RefundPercent /
				100);
	ABotanicusGameState* GameState =
		GetWorld() ? GetWorld()->GetGameState<ABotanicusGameState>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	GameState->AddSharedFunds(Refund);
	NotifyPlayers(
		FString::Printf(
			TEXT("%s rembourse : +%d credits."),
			*Definition->DisplayName.ToString(),
			Refund));
	Actor->Destroy();

	if (ABotanicusGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
	return true;
}

void ABotanicusRefundZoneActor::NotifyPlayers(
	const FString& Message) const
{
	if (!GetWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It =
			 GetWorld()->GetPlayerControllerIterator();
		 It;
		 ++It)
	{
		if (APlayerController* Controller = It->Get())
		{
			Controller->ClientMessage(Message);
		}
	}
}
