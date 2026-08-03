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
	PrimaryActorTick.bCanEverTick = true;
	// Derived placeable actors use Tick for continuous gameplay actions
	// (watering, filling soil, shelf feedback, checkout state, etc.).
	// Keep it enabled even when this base class is not currently simulating
	// a thrown item.
	PrimaryActorTick.bStartWithTickEnabled = true;

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

void ABotanicusPlaceableItemActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !Mesh || !Mesh->IsSimulatingPhysics())
	{
		return;
	}

	ThrowElapsedTime += DeltaSeconds;
	const bool bHasSettled =
		ThrowElapsedTime >= 0.35f &&
		(Mesh->IsAnyRigidBodyAwake() == false ||
		 Mesh->GetPhysicsLinearVelocity().SizeSquared() <
			 FMath::Square(8.0f));
	if (!bHasSettled && ThrowElapsedTime < 12.0f)
	{
		return;
	}

	const FTransform SettledMeshTransform =
		Mesh->GetComponentTransform();
	Mesh->SetSimulatePhysics(false);
	SetActorLocationAndRotation(
		SettledMeshTransform.GetLocation(),
		SettledMeshTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Mesh->AttachToComponent(
		SceneRoot,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Mesh->SetRelativeLocationAndRotation(
		FVector::ZeroVector,
		FRotator::ZeroRotator);
	ForceNetUpdate();
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

void ABotanicusPlaceableItemActor::ConfigureAsLocalInspection()
{
	SetReplicates(false);
	SetActorEnableCollision(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetOverlayMaterial(nullptr);
}

void ABotanicusPlaceableItemActor::LaunchItem(
	const FVector& InitialVelocity)
{
	if (!HasAuthority() || !Mesh ||
		InitialVelocity.IsNearlyZero())
	{
		return;
	}

	SetNetDormancy(DORM_Awake);
	FlushNetDormancy();
	Mesh->SetIsReplicated(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();
	Mesh->SetPhysicsLinearVelocity(InitialVelocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(
		FVector(0.0f, 180.0f, 120.0f));
	ThrowElapsedTime = 0.0f;
	ForceNetUpdate();
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
