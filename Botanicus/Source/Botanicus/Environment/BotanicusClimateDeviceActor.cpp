// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/BotanicusClimateDeviceActor.h"

#include "BotanicusGameMode.h"
#include "BotanicusCharacter.h"
#include "BotanicusPlayerController.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusClimateDeviceActor::ABotanicusClimateDeviceActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	DeviceVisualRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("ClimateDeviceVisualRoot"));
	DeviceVisualRoot->SetupAttachment(SceneRoot);
	AddDevicePart(
		TEXT("DeviceBase"),
		FVector(0.0f, 0.0f, 12.5f),
		FVector(100.0f, 100.0f, 25.0f));
	StatusLight = CreateDefaultSubobject<UPointLightComponent>(
		TEXT("ClimateStatusLight"));
	StatusLight->SetupAttachment(DeviceVisualRoot);
	StatusLight->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
	StatusLight->SetAttenuationRadius(260.0f);
	StatusLight->SetIntensity(1800.0f);
	StatusLight->SetCastShadows(false);
	for (int32 Index = 0; Index < 24; ++Index)
	{
		UStaticMeshComponent* ZonePart = AddDevicePart(
			FName(*FString::Printf(TEXT("InfluenceZonePart%02d"), Index)),
			FVector::ZeroVector,
			FVector(100.0f, 5.0f, 2.0f),
			false);
		ZonePart->SetVisibility(false);
		InfluenceZoneParts.Add(ZonePart);
	}
}

void ABotanicusClimateDeviceActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshInfluenceZoneGeometry();
	RefreshDeviceVisuals();
}

void ABotanicusClimateDeviceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bShowZone = IsInPlacementMode() ||
		bFurnitureModeInfluenceZoneVisible;
	for (UStaticMeshComponent* ZonePart : InfluenceZoneParts)
	{
		if (ZonePart)
		{
			ZonePart->SetVisibility(bShowZone);
		}
	}
}

void ABotanicusClimateDeviceActor::
	SetFurnitureModeInfluenceZoneVisible(bool bVisible)
{
	bFurnitureModeInfluenceZoneVisible = bVisible;
	const bool bShowZone = IsInPlacementMode() || bVisible;
	for (UStaticMeshComponent* ZonePart : InfluenceZoneParts)
	{
		if (ZonePart)
		{
			ZonePart->SetVisibility(bShowZone);
		}
	}
}

FVector ABotanicusClimateDeviceActor::GetPlacementBoxExtent() const
{
	switch (DeviceType)
	{
	case EBotanicusClimateDeviceType::GrowLight:
		return FVector(125.0f, 50.0f, 110.0f);
	case EBotanicusClimateDeviceType::Shade:
		return FVector(115.0f, 115.0f, 120.0f);
	case EBotanicusClimateDeviceType::Cooler:
		return FVector(55.0f, 45.0f, 75.0f);
	case EBotanicusClimateDeviceType::Dehumidifier:
		return FVector(55.0f, 45.0f, 75.0f);
	default:
		return FVector(50.0f, 40.0f, 90.0f);
	}
}

void ABotanicusClimateDeviceActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusClimateDeviceActor, InfluenceRadius);
	DOREPLIFETIME(ABotanicusClimateDeviceActor, MaximumEffectStrength);
	DOREPLIFETIME(ABotanicusClimateDeviceActor, bEnabled);
	DOREPLIFETIME(ABotanicusClimateDeviceActor, PowerLevel);
	DOREPLIFETIME(ABotanicusClimateDeviceActor, DeviceLevel);
}

FBotanicusInteractionPrompt
ABotanicusClimateDeviceActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = GetDeviceDisplayName();
	Prompt.ActionText = bEnabled
		? NSLOCTEXT("BotanicusClimate", "TurnOffDevice", "Eteindre")
		: NSLOCTEXT("BotanicusClimate", "TurnOnDevice", "Allumer");
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	return Prompt;
}

bool ABotanicusClimateDeviceActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	const APawn* InteractingPawn = Cast<APawn>(Interactor);
	const ABotanicusPlayerController* Controller = InteractingPawn
		? Cast<ABotanicusPlayerController>(InteractingPawn->GetController())
		: nullptr;
	if (Controller &&
		Controller->IsFurnitureMoveModeActiveForGameplay())
	{
		return false;
	}
	return !IsInPlacementMode() &&
		!IsValid(GetCarrier()) &&
		Super::CanInteract_Implementation(Interactor);
}

void ABotanicusClimateDeviceActor::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor))
	{
		return;
	}
	SetClimateDeviceEnabled(!bEnabled);
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		if (APlayerController* Controller =
			Cast<APlayerController>(Pawn->GetController()))
		{
			Controller->ClientMessage(
				bEnabled ? TEXT("Appareil allume.") : TEXT("Appareil eteint."));
		}
	}
	if (ABotanicusGameMode* GameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<ABotanicusGameMode>() : nullptr)
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

float ABotanicusClimateDeviceActor::GetInfluenceStrengthAtLocation(
	const FVector& WorldLocation) const
{
	if (!bEnabled || IsInPlacementMode() || IsValid(GetCarrier()))
	{
		return 0.0f;
	}
	const float SafeRadius = FMath::Max(1.0f, InfluenceRadius);
	const float Distance = FVector::Dist2D(GetActorLocation(), WorldLocation);
	// Device sliders express the actual effect applied to every plant inside
	// the displayed zone: 20% means 20 points and 100% means 100 points.
	// A distance falloff made the shown percentage misleading and prevented
	// plants near the edge from receiving the requested setting.
	return Distance <= SafeRadius ? 1.0f : 0.0f;
}

void ABotanicusClimateDeviceActor::GetEnvironmentDeltasAtLocation(
	const FVector& WorldLocation,
	float& OutTemperatureDelta,
	float& OutHumidityDelta,
	float& OutLuminosityDelta) const
{
	OutTemperatureDelta = 0.0f;
	OutHumidityDelta = 0.0f;
	OutLuminosityDelta = 0.0f;
	const float Effect =
		MaximumEffectStrength * PowerLevel *
		GetInfluenceStrengthAtLocation(WorldLocation);
	switch (DeviceType)
	{
	case EBotanicusClimateDeviceType::Heater:
		OutTemperatureDelta = FMath::Abs(Effect);
		break;
	case EBotanicusClimateDeviceType::Cooler:
		OutTemperatureDelta = -FMath::Abs(Effect);
		break;
	case EBotanicusClimateDeviceType::GrowLight:
		OutLuminosityDelta = FMath::Abs(Effect);
		break;
	case EBotanicusClimateDeviceType::Mister:
		OutHumidityDelta = FMath::Abs(Effect);
		break;
	case EBotanicusClimateDeviceType::Shade:
		OutLuminosityDelta = -FMath::Abs(Effect);
		break;
	case EBotanicusClimateDeviceType::Dehumidifier:
		OutHumidityDelta = -FMath::Abs(Effect);
		break;
	default:
		break;
	}
}

void ABotanicusClimateDeviceActor::SetClimateDeviceEnabled(bool bInEnabled)
{
	if (!HasAuthority())
	{
		return;
	}
	bEnabled = bInEnabled;
	RefreshDeviceVisuals();
	ForceNetUpdate();
}

void ABotanicusClimateDeviceActor::SetClimateDevicePowerLevel(
	float InPowerLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	PowerLevel = FMath::Clamp(InPowerLevel, 0.0f, GetMaximumPowerLevel());
	RefreshDeviceVisuals();
	ForceNetUpdate();
}

void ABotanicusClimateDeviceActor::RestoreClimateDeviceState(
	bool bInEnabled,
	float InInfluenceRadius,
	float InMaximumEffectStrength,
	float InPowerLevel)
{
	if (!HasAuthority())
	{
		return;
	}
	bEnabled = bInEnabled;
	InfluenceRadius = FMath::Clamp(InInfluenceRadius, 50.0f, 3000.0f);
	// Percentage-based devices now use a direct one-point-per-percent scale.
	// Force the new value when loading older saves that stored 45 or 70.
	const bool bUsesDirectPercentageEffect =
		DeviceType == EBotanicusClimateDeviceType::GrowLight ||
		DeviceType == EBotanicusClimateDeviceType::Mister ||
		DeviceType == EBotanicusClimateDeviceType::Shade ||
		DeviceType == EBotanicusClimateDeviceType::Dehumidifier;
	MaximumEffectStrength = bUsesDirectPercentageEffect
		? 100.0f
		: FMath::Max(0.0f, InMaximumEffectStrength);
	PowerLevel = FMath::Clamp(InPowerLevel, 0.0f, GetMaximumPowerLevel());
	RefreshInfluenceZoneGeometry();
	RefreshDeviceVisuals();
	ForceNetUpdate();
}

void ABotanicusClimateDeviceActor::ConfigureDevice(
	EBotanicusClimateDeviceType InType,
	float InMaximumEffectStrength,
	const FLinearColor& InColor)
{
	DeviceType = InType;
	MaximumEffectStrength = InMaximumEffectStrength;
	DeviceColor = InColor;
	if (DeviceVisualRoot)
	{
		DeviceVisualRoot->SetRelativeLocation(
			FVector(0.0f, 0.0f, -GetPlacementBoxExtent().Z));
	}
}

UStaticMeshComponent* ABotanicusClimateDeviceActor::AddDevicePart(
	FName ComponentName,
	const FVector& Location,
	const FVector& Size,
	bool bCollisionEnabled)
{
	UStaticMeshComponent* Part =
		CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
	Part->SetupAttachment(DeviceVisualRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Part->SetStaticMesh(CubeFinder.Object);
	}
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Size / 100.0f);
	Part->SetCollisionEnabled(
		bCollisionEnabled
			? ECollisionEnabled::QueryAndPhysics
			: ECollisionEnabled::NoCollision);
	return Part;
}

void ABotanicusClimateDeviceActor::OnEquipmentDefinitionApplied()
{
	ApplyLevelFromItemKey();
	// Catalogue cube is only a placement proxy; actor/BP components own visuals.
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
	}
	RefreshDeviceVisuals();
}

float ABotanicusClimateDeviceActor::GetMaximumPowerLevel() const
{
	if (DeviceType == EBotanicusClimateDeviceType::Heater)
	{
		// Heating models use their own Celsius ranges. PowerLevel remains
		// normalized against the level-4 maximum of 1000 degrees.
		switch (FMath::Clamp(DeviceLevel, 1, 4))
		{
		case 1:
			return 10.0f / 1000.0f;
		case 2:
			return 20.0f / 1000.0f;
		case 3:
			return 200.0f / 1000.0f;
		default:
			return 1.0f;
		}
	}
	switch (FMath::Clamp(DeviceLevel, 1, 4))
	{
	case 1:
		return 0.20f;
	case 2:
		return 0.50f;
	case 3:
		return 0.70f;
	default:
		return 1.00f;
	}
}

void ABotanicusClimateDeviceActor::ApplyLevelFromItemKey()
{
	const FString Key = GetItemKey().ToString();
	for (int32 Level = 1; Level <= 4; ++Level)
	{
		if (Key.EndsWith(FString::Printf(TEXT("_Level%d"), Level)))
		{
			DeviceLevel = Level;
			PowerLevel = FMath::Min(PowerLevel, GetMaximumPowerLevel());
			return;
		}
	}
	// Legacy items without a suffix retain their former full-power behaviour.
	DeviceLevel = 4;
}

void ABotanicusClimateDeviceActor::OnRep_DeviceState()
{
	RefreshDeviceVisuals();
}

void ABotanicusClimateDeviceActor::RefreshDeviceVisuals()
{
	if (StatusLight)
	{
		StatusLight->SetLightColor(DeviceColor);
		StatusLight->SetVisibility(bEnabled);
		StatusLight->SetIntensity(bEnabled ? 400.0f + 1400.0f * PowerLevel : 0.0f);
	}
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UStaticMeshComponent* MeshComponent : Meshes)
	{
		if (MeshComponent)
		{
			const FLinearColor VisualColor = bEnabled
				? DeviceColor
				: FLinearColor(0.12f, 0.12f, 0.12f, 1.0f);
			MeshComponent->SetVectorParameterValueOnMaterials(
				TEXT("Color"),
				FVector(VisualColor.R, VisualColor.G, VisualColor.B));
		}
	}
}

void ABotanicusClimateDeviceActor::RefreshInfluenceZoneGeometry()
{
	const int32 SegmentCount = InfluenceZoneParts.Num();
	if (SegmentCount <= 0)
	{
		return;
	}
	const float SafeRadius = FMath::Max(50.0f, InfluenceRadius);
	const float SegmentLength =
		2.0f * PI * SafeRadius / static_cast<float>(SegmentCount) * 0.86f;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		UStaticMeshComponent* ZonePart = InfluenceZoneParts[Index];
		if (!ZonePart)
		{
			continue;
		}
		const float Angle =
			2.0f * PI * static_cast<float>(Index) /
			static_cast<float>(SegmentCount);
		ZonePart->SetRelativeLocation(FVector(
			FMath::Cos(Angle) * SafeRadius,
			FMath::Sin(Angle) * SafeRadius,
			3.0f));
		ZonePart->SetRelativeRotation(FRotator(
			0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f));
		ZonePart->SetRelativeScale3D(FVector(
			SegmentLength / 100.0f, 0.05f, 0.02f));
	}
}

FText ABotanicusClimateDeviceActor::GetDeviceDisplayName() const
{
	FText BaseName;
	switch (DeviceType)
	{
	case EBotanicusClimateDeviceType::Cooler:
		BaseName = NSLOCTEXT("BotanicusClimate", "CoolerName", "Refroidisseur");
		break;
	case EBotanicusClimateDeviceType::GrowLight:
		BaseName = NSLOCTEXT("BotanicusClimate", "GrowLightName", "Lampe horticole");
		break;
	case EBotanicusClimateDeviceType::Mister:
		BaseName = NSLOCTEXT("BotanicusClimate", "MisterName", "Brumisateur");
		break;
	case EBotanicusClimateDeviceType::Shade:
		BaseName = NSLOCTEXT("BotanicusClimate", "ShadeName", "Ombrière");
		break;
	case EBotanicusClimateDeviceType::Dehumidifier:
		BaseName = NSLOCTEXT(
			"BotanicusClimate", "DehumidifierName", "Déshumidificateur");
		break;
	default:
		BaseName = NSLOCTEXT("BotanicusClimate", "HeaterName", "Chauffage");
		break;
	}
	return FText::Format(
		NSLOCTEXT("BotanicusClimate", "LeveledDeviceName", "{0} - Niv. {1}"),
		BaseName,
		FText::AsNumber(DeviceLevel));
}

ABotanicusHeatingDeviceActor::ABotanicusHeatingDeviceActor()
{
	ConfigureDevice(
		EBotanicusClimateDeviceType::Heater,
		1000.0f,
		FLinearColor(1.0f, 0.16f, 0.02f, 1.0f));
	AddDevicePart(TEXT("HeaterBody"), FVector(0.0f, 0.0f, 82.5f),
		FVector(76.0f, 48.0f, 115.0f));
	AddDevicePart(TEXT("HeaterTop"), FVector(0.0f, 0.0f, 150.0f),
		FVector(86.0f, 58.0f, 20.0f), false);
}

ABotanicusCoolingDeviceActor::ABotanicusCoolingDeviceActor()
{
	ConfigureDevice(
		EBotanicusClimateDeviceType::Cooler,
		12.0f,
		FLinearColor(0.02f, 0.45f, 1.0f, 1.0f));
	AddDevicePart(TEXT("CoolerBody"), FVector(0.0f, 0.0f, 80.0f),
		FVector(92.0f, 68.0f, 110.0f));
	AddDevicePart(TEXT("CoolerVent"), FVector(0.0f, -39.0f, 92.0f),
		FVector(68.0f, 12.0f, 55.0f), false);
}

ABotanicusGrowLightDeviceActor::ABotanicusGrowLightDeviceActor()
{
	ConfigureDevice(
		EBotanicusClimateDeviceType::GrowLight,
		100.0f,
		FLinearColor(1.0f, 0.78f, 0.08f, 1.0f));
	AddDevicePart(TEXT("LampPole"), FVector(0.0f, 0.0f, 122.5f),
		FVector(20.0f, 20.0f, 195.0f));
	AddDevicePart(TEXT("LampArm"), FVector(40.0f, 0.0f, 207.0f),
		FVector(100.0f, 20.0f, 20.0f), false);
	AddDevicePart(TEXT("LampHead"), FVector(88.0f, 0.0f, 185.0f),
		FVector(70.0f, 70.0f, 24.0f), false);
}

ABotanicusMisterDeviceActor::ABotanicusMisterDeviceActor()
{
	ConfigureDevice(
		EBotanicusClimateDeviceType::Mister,
		100.0f,
		FLinearColor(0.02f, 0.85f, 0.92f, 1.0f));
	AddDevicePart(TEXT("MisterTank"), FVector(0.0f, 0.0f, 72.5f),
		FVector(72.0f, 72.0f, 95.0f));
	AddDevicePart(TEXT("MisterNeck"), FVector(0.0f, 0.0f, 145.0f),
		FVector(20.0f, 20.0f, 50.0f), false);
	AddDevicePart(TEXT("MisterNozzle"), FVector(32.0f, 0.0f, 170.0f),
		FVector(78.0f, 24.0f, 24.0f), false);
}

ABotanicusShadeDeviceActor::ABotanicusShadeDeviceActor()
{
	ConfigureDevice(
		EBotanicusClimateDeviceType::Shade,
		100.0f,
		FLinearColor(0.16f, 0.28f, 0.12f, 1.0f));
	AddDevicePart(TEXT("ShadePole"), FVector(0.0f, 0.0f, 112.5f),
		FVector(18.0f, 18.0f, 205.0f));
	AddDevicePart(TEXT("ShadeCanopyX"), FVector(0.0f, 0.0f, 220.0f),
		FVector(220.0f, 90.0f, 14.0f), false);
	AddDevicePart(TEXT("ShadeCanopyY"), FVector(0.0f, 0.0f, 221.0f),
		FVector(90.0f, 220.0f, 14.0f), false);
}

ABotanicusDehumidifierDeviceActor::ABotanicusDehumidifierDeviceActor()
{
	ConfigureDevice(
		EBotanicusClimateDeviceType::Dehumidifier,
		100.0f,
		FLinearColor(0.20f, 0.72f, 0.88f, 1.0f));
	AddDevicePart(
		TEXT("DehumidifierBody"),
		FVector(0.0f, 0.0f, 77.5f),
		FVector(86.0f, 66.0f, 105.0f));
	AddDevicePart(
		TEXT("DehumidifierIntake"),
		FVector(0.0f, -37.0f, 100.0f),
		FVector(62.0f, 10.0f, 38.0f),
		false);
	AddDevicePart(
		TEXT("DehumidifierTank"),
		FVector(0.0f, -35.0f, 45.0f),
		FVector(52.0f, 8.0f, 28.0f),
		false);
}
