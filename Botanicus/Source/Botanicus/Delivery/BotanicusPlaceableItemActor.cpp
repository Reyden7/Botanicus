// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusPlaceableItemActor.h"

#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusPlaceableItemActor::ABotanicusPlaceableItemActor()
{
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.4f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		ValidPlacementMaterialFinder(
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_Can_Build.MI_Can_Build"));
	if (ValidPlacementMaterialFinder.Succeeded())
	{
		ValidPlacementMaterial = ValidPlacementMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		InvalidPlacementMaterialFinder(
			TEXT("/Game/EasyBuildingSystem/Materials/Instances/Dummy/MI_CanNot_Build.MI_CanNot_Build"));
	if (InvalidPlacementMaterialFinder.Succeeded())
	{
		InvalidPlacementMaterial = InvalidPlacementMaterialFinder.Object;
	}

	InteractionAction = FText::GetEmpty();
	InteractionName = FText::GetEmpty();
}

void ABotanicusPlaceableItemActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusPlaceableItemActor, ItemKey);
	DOREPLIFETIME(ABotanicusPlaceableItemActor, Quantity);
}

bool ABotanicusPlaceableItemActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return false;
}

void ABotanicusPlaceableItemActor::InitializePlacedItem(
	FName InItemKey,
	int32 InQuantity)
{
	if (!InItemKey.IsNone())
	{
		ItemKey = InItemKey;
	}
	Quantity = FMath::Max(1, InQuantity);
	ApplyItemDefinition();
	ForceNetUpdate();
}

void ABotanicusPlaceableItemActor::ConfigureAsLocalPreview(
	bool bIsValid)
{
	SetReplicates(false);
	SetActorEnableCollision(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetOverlayMaterial(
		bIsValid
			? ValidPlacementMaterial
			: InvalidPlacementMaterial);
}

FVector ABotanicusPlaceableItemActor::GetPlacementBoxExtent() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return FVector(20.0f);
	}

	return Mesh->GetStaticMesh()->GetBounds().BoxExtent *
		Mesh->GetComponentScale().GetAbs();
}

void ABotanicusPlaceableItemActor::OnRep_ItemKey()
{
	ApplyItemDefinition();
}

void ABotanicusPlaceableItemActor::ApplyItemDefinition()
{
	UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Definition)
	{
		return;
	}

	if (UStaticMesh* DefinitionMesh =
		Definition->WorldMesh.LoadSynchronous())
	{
		Mesh->SetStaticMesh(DefinitionMesh);
	}
	Mesh->SetRelativeScale3D(Definition->WorldScale);
}
