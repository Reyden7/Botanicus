// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decoration/BotanicusBrokenFlowerPotActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusBrokenFlowerPotActor::ABotanicusBrokenFlowerPotActor()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PotOneFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/propsDeco/pot1/pot.pot"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PotTwoFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/propsDeco/pot2/pot.pot"));

	PotMeshOne = PotOneFinder.Object;
	PotMeshTwo = PotTwoFinder.Object;
	if (PotMeshOne)
	{
		Mesh->SetStaticMesh(PotMeshOne);
	}
	Mesh->SetRelativeScale3D(FVector::OneVector);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetGenerateOverlapEvents(false);

	Tags.AddUnique(TEXT("BotanicusAmbientBrokenPot"));
}

void ABotanicusBrokenFlowerPotActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && PotVariant == INDEX_NONE)
	{
		PotVariant = FMath::RandRange(0, 1);
		ForceNetUpdate();
	}
	ApplyPotVariant();
}

void ABotanicusBrokenFlowerPotActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusBrokenFlowerPotActor, PotVariant);
}

void ABotanicusBrokenFlowerPotActor::SetPotVariant(int32 InVariant)
{
	if (!HasAuthority())
	{
		return;
	}
	PotVariant = FMath::Clamp(InVariant, 0, 1);
	ApplyPotVariant();
	ForceNetUpdate();
}

void ABotanicusBrokenFlowerPotActor::ApplyItemDefinition()
{
	Super::ApplyItemDefinition();
	ApplyPotVariant();
}

void ABotanicusBrokenFlowerPotActor::OnRep_PotVariant()
{
	ApplyPotVariant();
}

void ABotanicusBrokenFlowerPotActor::ApplyPotVariant()
{
	if (!Mesh)
	{
		return;
	}
	UStaticMesh* DesiredMesh = PotVariant == 1 ? PotMeshTwo : PotMeshOne;
	if (DesiredMesh)
	{
		Mesh->SetStaticMesh(DesiredMesh);
	}
	Mesh->SetRelativeScale3D(FVector::OneVector);
}
