// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusPlaceableItemActor.h"

#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "ItemDataAsset.h"
#include "ItemDataSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void GatherPreviewMeshComponents(
	AActor* RootActor,
	TArray<UMeshComponent*>& OutMeshComponents,
	TSet<const AActor*>& VisitedActors)
{
	if (!IsValid(RootActor) || VisitedActors.Contains(RootActor))
	{
		return;
	}
	VisitedActors.Add(RootActor);

	TInlineComponentArray<UMeshComponent*> DirectMeshComponents(RootActor);
	for (UMeshComponent* MeshComponent : DirectMeshComponents)
	{
		OutMeshComponents.AddUnique(MeshComponent);
	}

	TArray<AActor*> AttachedActors;
	RootActor->GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		GatherPreviewMeshComponents(
			AttachedActor,
			OutMeshComponents,
			VisitedActors);
	}
}

void GatherPreviewMeshComponents(
	AActor* RootActor,
	TArray<UMeshComponent*>& OutMeshComponents)
{
	TSet<const AActor*> VisitedActors;
	GatherPreviewMeshComponents(
		RootActor,
		OutMeshComponents,
		VisitedActors);
}
}

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
			TEXT("/Game/Botanicus/Materials/Silhouette/M_Silhouette_Hologram_Blue.M_Silhouette_Hologram_Blue"));
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
	const bool bIsPlacementSilhouette =
		ActorHasTag(TEXT("BotanicusPlacementPreview"));
	UMaterialInterface* PreviewMaterial = bIsPlacementSilhouette
		? (bIsValid ? ValidPlacementMaterial : InvalidPlacementMaterial)
		: nullptr;
	if (PreviewMaterial)
	{
		ApplyPlacementMaterial(PreviewMaterial);
	}
	else
	{
		RestorePlacementMaterials();
	}
	TArray<UMeshComponent*> MeshComponents;
	GatherPreviewMeshComponents(this, MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(
				ECollisionEnabled::NoCollision);
			MeshComponent->SetOverlayMaterial(nullptr);
			MeshComponent->SetRenderCustomDepth(bIsPlacementSilhouette);
			MeshComponent->SetCustomDepthStencilValue(bIsValid ? 1 : 2);
		}
	}
}

void ABotanicusPlaceableItemActor::ConfigureAsLocalInspection()
{
	SetReplicates(false);
	SetActorEnableCollision(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RestorePlacementMaterials();
	TArray<UMeshComponent*> MeshComponents;
	GatherPreviewMeshComponents(this, MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->SetOverlayMaterial(nullptr);
			MeshComponent->SetRenderCustomDepth(false);
		}
	}
}

void ABotanicusPlaceableItemActor::ApplyPlacementMaterial(
	UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}

	if (PreviewMaterialMeshes.IsEmpty())
	{
		TArray<UMeshComponent*> MeshComponents;
		GatherPreviewMeshComponents(this, MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent)
			{
				continue;
			}
			const int32 MaterialCount = MeshComponent->GetNumMaterials();
			PreviewMaterialMeshes.Add(MeshComponent);
			PreviewMaterialCounts.Add(MaterialCount);
			for (int32 Index = 0; Index < MaterialCount; ++Index)
			{
				PreviewOriginalMaterials.Add(MeshComponent->GetMaterial(Index));
			}
		}
	}

	for (int32 MeshIndex = 0; MeshIndex < PreviewMaterialMeshes.Num(); ++MeshIndex)
	{
		UMeshComponent* MeshComponent = PreviewMaterialMeshes[MeshIndex];
		if (!MeshComponent)
		{
			continue;
		}
		for (int32 Index = 0; Index < PreviewMaterialCounts[MeshIndex]; ++Index)
		{
			MeshComponent->SetMaterial(Index, Material);
		}
		MeshComponent->SetOverlayMaterial(nullptr);
	}
}

void ABotanicusPlaceableItemActor::RestorePlacementMaterials()
{
	int32 MaterialOffset = 0;
	for (int32 MeshIndex = 0; MeshIndex < PreviewMaterialMeshes.Num(); ++MeshIndex)
	{
		UMeshComponent* MeshComponent = PreviewMaterialMeshes[MeshIndex];
		const int32 MaterialCount = PreviewMaterialCounts.IsValidIndex(MeshIndex)
			? PreviewMaterialCounts[MeshIndex]
			: 0;
		if (MeshComponent)
		{
			for (int32 Index = 0; Index < MaterialCount; ++Index)
			{
				if (PreviewOriginalMaterials.IsValidIndex(MaterialOffset + Index))
				{
					MeshComponent->SetMaterial(
						Index,
						PreviewOriginalMaterials[MaterialOffset + Index]);
				}
			}
			MeshComponent->SetOverlayMaterial(nullptr);
		}
		MaterialOffset += MaterialCount;
	}
	PreviewMaterialMeshes.Reset();
	PreviewOriginalMaterials.Reset();
	PreviewMaterialCounts.Reset();
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

float ABotanicusPlaceableItemActor::
	GetPlacementPivotToBottomOffset() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return GetPlacementBoxExtent().Z;
	}

	const FBoxSphereBounds WorldBounds = Mesh->CalcBounds(
		Mesh->GetComponentTransform());
	const float MeshBottom =
		WorldBounds.Origin.Z - WorldBounds.BoxExtent.Z;
	// Keep this value signed. A Blueprint may intentionally move its visual
	// mesh above or below the actor pivot; clamping negative values discarded
	// that designer-authored offset and made the object float after moving it.
	return GetActorLocation().Z - MeshBottom;
}

void ABotanicusPlaceableItemActor::OnRep_ItemKey()
{
	ApplyItemDefinition();
}

void ABotanicusPlaceableItemActor::ApplyItemDefinition()
{
	if (UsesBlueprintAppearance())
	{
		return;
	}

	UStaticMesh* ResolvedMesh = nullptr;
	bool bUsesItemDataMesh = false;

	if (!ItemKey.IsNone())
	{
		const UItemDataAsset* ItemData =
			UItemDataSubsystem::Get(this).GetItemDataAsset(ItemKey);
		if (ItemData)
		{
			ResolvedMesh =
				ItemData->GetItemStaticMesh().LoadSynchronous();
			bUsesItemDataMesh = IsValid(ResolvedMesh);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;

	if (!ResolvedMesh && Definition)
	{
		ResolvedMesh = Definition->WorldMesh.LoadSynchronous();
	}

	if (ResolvedMesh)
	{
		Mesh->SetStaticMesh(ResolvedMesh);
	}

	// Imported ItemData meshes are authored at their real in-game size. The
	// native catalog scale only exists to resize its primitive fallback meshes.
	Mesh->SetRelativeScale3D(
		bUsesItemDataMesh || !Definition
			? FVector::OneVector
			: Definition->WorldScale);
}
