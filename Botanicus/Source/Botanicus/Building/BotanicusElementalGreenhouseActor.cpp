// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/BotanicusElementalGreenhouseActor.h"

#include "BotanicusGameMode.h"
#include "BotanicusGameState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float WallThickness = 35.0f;
	constexpr float FloorThickness = 40.0f;
	constexpr float RoofThickness = 30.0f;
	constexpr float DoorWidth = 260.0f;
}

ABotanicusElementalGreenhouseActor::
	ABotanicusElementalGreenhouseActor()
{
	FloorPart = AddBuildingPart(
		TEXT("ElementalFloor"),
		FVector::ZeroVector,
		FVector(100.0f));
	BackWallPart = AddBuildingPart(
		TEXT("ElementalBackWall"),
		FVector::ZeroVector,
		FVector(100.0f));
	LeftWallPart = AddBuildingPart(
		TEXT("ElementalLeftWall"),
		FVector::ZeroVector,
		FVector(100.0f));
	RightWallPart = AddBuildingPart(
		TEXT("ElementalRightWall"),
		FVector::ZeroVector,
		FVector(100.0f));
	FrontWallLeftPart = AddBuildingPart(
		TEXT("ElementalFrontWallLeft"),
		FVector::ZeroVector,
		FVector(100.0f));
	FrontWallRightPart = AddBuildingPart(
		TEXT("ElementalFrontWallRight"),
		FVector::ZeroVector,
		FVector(100.0f));
	RoofPart = AddBuildingPart(
		TEXT("ElementalRoof"),
		FVector::ZeroVector,
		FVector(100.0f));

	GrowingVolume = CreateDefaultSubobject<UBoxComponent>(
		TEXT("ElementalGrowingVolume"));
	GrowingVolume->SetupAttachment(BuildingRoot);
	GrowingVolume->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("ElementalGreenhouseStatus"));
	StatusText->SetupAttachment(BuildingRoot);
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetWorldSize(32.0f);
	StatusText->SetTextRenderColor(FColor(255, 175, 60));
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RefreshGeometry();
}

void ABotanicusElementalGreenhouseActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshGeometry();
}

void ABotanicusElementalGreenhouseActor::
	GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusElementalGreenhouseActor,
		GreenhouseLevel);
}

FBotanicusInteractionPrompt
ABotanicusElementalGreenhouseActor::
	GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = FText::FromString(
		FString::Printf(
			TEXT("Serre %s - niveau %d"),
			*GetElementLabel(),
			GreenhouseLevel));
	if (GreenhouseLevel >= 3)
	{
		Prompt.ActionText = FText::FromString(
			TEXT("Taille maximale atteinte"));
		Prompt.bCanInteract = false;
	}
	else
	{
		Prompt.ActionText = FText::FromString(
			FString::Printf(
				TEXT("Ameliorer au niveau %d - %d credits"),
				GreenhouseLevel + 1,
				GetNextUpgradeCost()));
		Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	}
	return Prompt;
}

bool ABotanicusElementalGreenhouseActor::
	CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && GreenhouseLevel < 3;
}

void ABotanicusElementalGreenhouseActor::
	Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() ||
		!CanInteract_Implementation(Interactor))
	{
		return;
	}

	ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	const int32 Cost = GetNextUpgradeCost();
	APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* Controller =
		Pawn
			? Cast<APlayerController>(Pawn->GetController())
			: nullptr;
	if (!GameState || !GameState->TrySpendSharedFunds(Cost))
	{
		if (Controller)
		{
			Controller->ClientMessage(
				TEXT("Credits insuffisants pour agrandir la serre."));
		}
		return;
	}

	++GreenhouseLevel;
	RefreshGeometry();
	ForceNetUpdate();
	if (Controller)
	{
		Controller->ClientMessage(
			*FString::Printf(
				TEXT("Serre %s amelioree au niveau %d."),
				*GetElementLabel(),
				GreenhouseLevel));
	}
	if (ABotanicusGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

bool ABotanicusElementalGreenhouseActor::ContainsWorldLocation(
	const FVector& WorldLocation) const
{
	const FVector Local =
		GetActorTransform().InverseTransformPosition(WorldLocation);
	const FVector Size = GetLevelSize();
	return FMath::Abs(Local.X) <= Size.X * 0.5f - WallThickness &&
		FMath::Abs(Local.Y) <= Size.Y * 0.5f - WallThickness &&
		Local.Z >= 0.0f &&
		Local.Z <= Size.Z;
}

void ABotanicusElementalGreenhouseActor::RestoreGreenhouseLevel(
	int32 InLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	GreenhouseLevel = FMath::Clamp(InLevel, 1, 3);
	RefreshGeometry();
	ForceNetUpdate();
}

EBotanicusPlantElement
ABotanicusElementalGreenhouseActor::
	FindGreenhouseElementAtLocation(
		const UWorld* World,
		const FVector& WorldLocation)
{
	if (!World)
	{
		return EBotanicusPlantElement::Normal;
	}
	for (TActorIterator<ABotanicusElementalGreenhouseActor> It(
			 const_cast<UWorld*>(World));
		 It;
		 ++It)
	{
		if (It->ContainsWorldLocation(WorldLocation))
		{
			return It->GetElement();
		}
	}
	return EBotanicusPlantElement::Normal;
}

void ABotanicusElementalGreenhouseActor::OnRep_GreenhouseLevel()
{
	RefreshGeometry();
}

void ABotanicusElementalGreenhouseActor::RefreshGeometry()
{
	const FVector Size = GetLevelSize();
	const float Width = Size.X;
	const float Depth = Size.Y;
	const float Height = Size.Z;
	const float FrontWidth =
		FMath::Max(100.0f, (Width - DoorWidth) * 0.5f);

	const auto SetPart =
		[](UStaticMeshComponent* Part,
		   const FVector& Location,
		   const FVector& PartSize)
		{
			if (!Part)
			{
				return;
			}
			Part->SetRelativeLocation(Location);
			Part->SetRelativeScale3D(PartSize / 100.0f);
		};
	SetPart(
		FloorPart,
		FVector(0.0f, 0.0f, FloorThickness * 0.5f),
		FVector(Width, Depth, FloorThickness));
	SetPart(
		BackWallPart,
		FVector(0.0f, Depth * 0.5f, Height * 0.5f),
		FVector(Width, WallThickness, Height));
	SetPart(
		LeftWallPart,
		FVector(-Width * 0.5f, 0.0f, Height * 0.5f),
		FVector(WallThickness, Depth, Height));
	SetPart(
		RightWallPart,
		FVector(Width * 0.5f, 0.0f, Height * 0.5f),
		FVector(WallThickness, Depth, Height));
	SetPart(
		FrontWallLeftPart,
		FVector(
			-(DoorWidth + FrontWidth) * 0.5f,
			-Depth * 0.5f,
			Height * 0.5f),
		FVector(FrontWidth, WallThickness, Height));
	SetPart(
		FrontWallRightPart,
		FVector(
			(DoorWidth + FrontWidth) * 0.5f,
			-Depth * 0.5f,
			Height * 0.5f),
		FVector(FrontWidth, WallThickness, Height));
	SetPart(
		RoofPart,
		FVector(0.0f, 0.0f, Height + RoofThickness * 0.5f),
		FVector(Width + 80.0f, Depth + 80.0f, RoofThickness));

	if (GrowingVolume)
	{
		GrowingVolume->SetRelativeLocation(
			FVector(0.0f, 0.0f, Height * 0.5f));
		GrowingVolume->SetBoxExtent(
			FVector(
				Width * 0.5f - WallThickness,
				Depth * 0.5f - WallThickness,
				Height * 0.5f));
	}
	if (StatusText)
	{
		StatusText->SetRelativeLocation(
			FVector(0.0f, -Depth * 0.5f - 10.0f, Height + 70.0f));
		StatusText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("SERRE %s - NIVEAU %d\nZONE DE CROISSANCE : %.0f x %.0f m"),
					*GetElementLabel().ToUpper(),
					GreenhouseLevel,
					Width / 100.0f,
					Depth / 100.0f)));
	}
}

FVector ABotanicusElementalGreenhouseActor::GetLevelSize() const
{
	switch (GreenhouseLevel)
	{
	case 2:
		return FVector(1800.0f, 1200.0f, 620.0f);
	case 3:
		return FVector(2400.0f, 1600.0f, 700.0f);
	default:
		return FVector(1200.0f, 900.0f, 520.0f);
	}
}

int32 ABotanicusElementalGreenhouseActor::GetNextUpgradeCost() const
{
	return GreenhouseLevel <= 1 ? 1200 : 2500;
}

FString ABotanicusElementalGreenhouseActor::GetElementLabel() const
{
	switch (Element)
	{
	case EBotanicusPlantElement::Fire:
		return TEXT("Feu");
	case EBotanicusPlantElement::Water:
		return TEXT("Eau");
	case EBotanicusPlantElement::Ice:
		return TEXT("Glace");
	case EBotanicusPlantElement::Shadow:
		return TEXT("Tenebres");
	default:
		return TEXT("Normale");
	}
}

ABotanicusFireGreenhouseActor::ABotanicusFireGreenhouseActor()
{
	Element = EBotanicusPlantElement::Fire;
}

ABotanicusWaterGreenhouseActor::ABotanicusWaterGreenhouseActor()
{
	Element = EBotanicusPlantElement::Water;
}

ABotanicusIceGreenhouseActor::ABotanicusIceGreenhouseActor()
{
	Element = EBotanicusPlantElement::Ice;
}

ABotanicusShadowGreenhouseActor::ABotanicusShadowGreenhouseActor()
{
	Element = EBotanicusPlantElement::Shadow;
}
