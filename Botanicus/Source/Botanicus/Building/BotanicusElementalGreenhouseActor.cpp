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

ABotanicusGreenhouseActor::ABotanicusGreenhouseActor()
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

void ABotanicusGreenhouseActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshGeometry();
}

void ABotanicusGreenhouseActor::
	GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusGreenhouseActor,
		GreenhouseLevel);
	DOREPLIFETIME(
		ABotanicusGreenhouseActor,
		TemperatureCelsius);
	DOREPLIFETIME(
		ABotanicusGreenhouseActor,
		AirHumidityPercent);
	DOREPLIFETIME(
		ABotanicusGreenhouseActor,
		LuminosityPercent);
}

FBotanicusInteractionPrompt
ABotanicusGreenhouseActor::
	GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = FText::FromString(
		FString::Printf(
			TEXT("Serre principale - niveau %d"),
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

bool ABotanicusGreenhouseActor::
	CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && GreenhouseLevel < 3;
}

void ABotanicusGreenhouseActor::
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
				TEXT("Serre principale amelioree au niveau %d."),
				GreenhouseLevel));
	}
	if (ABotanicusGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

bool ABotanicusGreenhouseActor::ContainsWorldLocation(
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

float ABotanicusGreenhouseActor::GetTemperatureCelsius() const
{
	float Temperature = TemperatureCelsius;
	float Humidity = AirHumidityPercent;
	float Luminosity = LuminosityPercent;
	GetEnvironmentAtLocation(
		GetActorLocation(), Temperature, Humidity, Luminosity);
	return Temperature;
}

float ABotanicusGreenhouseActor::GetAirHumidityPercent() const
{
	float Temperature = TemperatureCelsius;
	float Humidity = AirHumidityPercent;
	float Luminosity = LuminosityPercent;
	GetEnvironmentAtLocation(
		GetActorLocation(), Temperature, Humidity, Luminosity);
	return Humidity;
}

float ABotanicusGreenhouseActor::GetLuminosityPercent() const
{
	float Temperature = TemperatureCelsius;
	float Humidity = AirHumidityPercent;
	float Luminosity = LuminosityPercent;
	GetEnvironmentAtLocation(
		GetActorLocation(), Temperature, Humidity, Luminosity);
	return Luminosity;
}

void ABotanicusGreenhouseActor::GetEnvironmentAtLocation(
	const FVector& WorldLocation,
	float& OutTemperatureCelsius,
	float& OutAirHumidityPercent,
	float& OutLuminosityPercent) const
{
	OutTemperatureCelsius = TemperatureCelsius;
	OutAirHumidityPercent = AirHumidityPercent;
	OutLuminosityPercent = LuminosityPercent;
	const ABotanicusGameState* GameState =
		GetWorld()
			? GetWorld()->GetGameState<ABotanicusGameState>()
			: nullptr;
	if (GameState)
	{
		OutTemperatureCelsius =
			GameState->GetOutdoorTemperatureCelsius();
		OutAirHumidityPercent =
			GameState->GetOutdoorAirHumidityPercent();
		OutLuminosityPercent =
			GameState->GetOutdoorLuminosityPercent();
	}
	// WorldLocation is intentionally part of the API: upcoming climate devices
	// will add their distance-based influence to these seasonal base values.
	(void)WorldLocation;
}

void ABotanicusGreenhouseActor::SetEnvironmentValues(
	float InTemperatureCelsius,
	float InAirHumidityPercent,
	float InLuminosityPercent)
{
	if (!HasAuthority())
	{
		return;
	}
	TemperatureCelsius = FMath::Clamp(
		InTemperatureCelsius, -50.0f, 100.0f);
	AirHumidityPercent = FMath::Clamp(
		InAirHumidityPercent, 0.0f, 100.0f);
	LuminosityPercent = FMath::Clamp(
		InLuminosityPercent, 0.0f, 100.0f);
	RefreshGeometry();
	ForceNetUpdate();
}

void ABotanicusGreenhouseActor::RestoreGreenhouseState(
	int32 InLevel,
	float InTemperatureCelsius,
	float InAirHumidityPercent,
	float InLuminosityPercent)
{
	if (!HasAuthority())
	{
		return;
	}
	GreenhouseLevel = FMath::Clamp(InLevel, 1, 3);
	TemperatureCelsius = FMath::Clamp(
		InTemperatureCelsius, -50.0f, 100.0f);
	AirHumidityPercent = FMath::Clamp(
		InAirHumidityPercent, 0.0f, 100.0f);
	LuminosityPercent = FMath::Clamp(
		InLuminosityPercent, 0.0f, 100.0f);
	RefreshGeometry();
	ForceNetUpdate();
}

ABotanicusGreenhouseActor*
ABotanicusGreenhouseActor::FindGreenhouseAtLocation(
		const UWorld* World,
		const FVector& WorldLocation)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABotanicusGreenhouseActor> It(
			 const_cast<UWorld*>(World));
		 It;
		 ++It)
	{
		if (It->ContainsWorldLocation(WorldLocation))
		{
			return *It;
		}
	}
	return nullptr;
}

void ABotanicusGreenhouseActor::OnRep_GreenhouseState()
{
	RefreshGeometry();
}

void ABotanicusGreenhouseActor::RefreshGeometry()
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
					TEXT("SERRE PRINCIPALE - NIVEAU %d\nZONE : %.0f x %.0f m\nTEMPERATURE : %.1f C | HUMIDITE : %.0f%% | LUMINOSITE : %.0f%%"),
					GreenhouseLevel,
					Width / 100.0f,
					Depth / 100.0f,
					GetTemperatureCelsius(),
					GetAirHumidityPercent(),
					GetLuminosityPercent())));
	}
}

FVector ABotanicusGreenhouseActor::GetLevelSize() const
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

int32 ABotanicusGreenhouseActor::GetNextUpgradeCost() const
{
	return GreenhouseLevel <= 1 ? 1200 : 2500;
}
