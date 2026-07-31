// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusPlantPotActor.h"

#include "Botanicus.h"
#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusPlantPotActor::ABotanicusPlantPotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	InteractionName =
		NSLOCTEXT("BotanicusGrowing", "PlantPot", "Pot de culture");
	InteractionAction =
		NSLOCTEXT("BotanicusGrowing", "MovePot", "Deplacer");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CylinderFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderFinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.34f));
	}

	SoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Soil"));
	SoilMesh->SetupAttachment(SceneRoot);
	SoilMesh->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	SoilMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 31.0f));
	SoilMesh->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.035f));
	SoilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StemMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Stem"));
	StemMesh->SetupAttachment(SceneRoot);
	StemMesh->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	StemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FoliageMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Foliage"));
	FoliageMesh->SetupAttachment(SceneRoot);
	FoliageMesh->SetStaticMesh(
		SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
	FoliageMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("GrowingStatus"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(15.0f);
	StatusText->SetTextRenderColor(FColor(110, 220, 255));
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RefreshVisuals();
}

void ABotanicusPlantPotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StatusText)
	{
		const UWorld* World = GetWorld();
		const APlayerController* LocalPlayerController =
			World ? World->GetFirstPlayerController() : nullptr;
		const APlayerCameraManager* CameraManager =
			LocalPlayerController
				? LocalPlayerController->PlayerCameraManager
				: nullptr;
		if (CameraManager)
		{
			StatusText->SetWorldRotation(
				(CameraManager->GetCameraLocation() -
				 StatusText->GetComponentLocation()).Rotation());
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	const bool bPrimaryUseChanged = UpdatePrimaryUse(DeltaSeconds);
	if (PlantKey.IsNone())
	{
		if (bPrimaryUseChanged)
		{
			RefreshVisuals();
			ForceNetUpdate();
		}
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* Definition =
		Plants ? Plants->FindPlant(PlantKey) : nullptr;
	if (!Definition)
	{
		return;
	}

	const float PreviousWater = WaterLevel;
	const float PreviousGrowth = GrowthProgress;
	WaterLevel = FMath::Clamp(
		WaterLevel -
			FMath::Max(0.0f, Definition->WaterConsumptionPerSecond) *
				DeltaSeconds,
		0.0f,
		1.0f);
	if (WaterLevel >= Definition->MinimumHealthyWater &&
		WaterLevel <= Definition->MaximumHealthyWater &&
		GrowthProgress < 1.0f)
	{
		GrowthProgress = FMath::Clamp(
			GrowthProgress +
				DeltaSeconds /
					FMath::Max(
						1.0f,
						Definition->GrowthDurationSeconds),
			0.0f,
			1.0f);
	}

	if (!FMath::IsNearlyEqual(PreviousWater, WaterLevel, 0.0001f) ||
		!FMath::IsNearlyEqual(PreviousGrowth, GrowthProgress, 0.0001f) ||
		bPrimaryUseChanged)
	{
		RefreshVisuals();
		ForceNetUpdate();
	}
}

void ABotanicusPlantPotActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusPlantPotActor, bHasSoil);
	DOREPLIFETIME(ABotanicusPlantPotActor, PlantKey);
	DOREPLIFETIME(ABotanicusPlantPotActor, WaterLevel);
	DOREPLIFETIME(ABotanicusPlantPotActor, GrowthProgress);
	DOREPLIFETIME(ABotanicusPlantPotActor, WateringCount);
	DOREPLIFETIME(ABotanicusPlantPotActor, SoilFillProgress);
	DOREPLIFETIME(ABotanicusPlantPotActor, bWateringActive);
}

FBotanicusInteractionPrompt
ABotanicusPlantPotActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	Prompt.bCanInteract = CanInteract_Implementation(Interactor);
	const FName SelectedItemKey = GetSelectedItemKey(Interactor);

	if (!bHasSoil)
	{
		Prompt.ActionText =
			SelectedItemKey == TEXT("PottingSoil")
				? NSLOCTEXT(
					"BotanicusGrowing",
					"FillPot",
					"Remplir de terreau")
				: NSLOCTEXT(
					"BotanicusGrowing",
					"NeedsSoil",
					"Sélectionner du terreau");
	}
	else if (PlantKey.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		Prompt.ActionText =
			Plants && Plants->FindPlantBySeed(SelectedItemKey)
				? NSLOCTEXT(
					"BotanicusGrowing",
					"PlantSeed",
					"Planter la graine")
				: NSLOCTEXT(
					"BotanicusGrowing",
					"NeedsSeed",
					"Sélectionner des graines");
	}
	else
	{
		Prompt.ActionText =
			SelectedItemKey == TEXT("WateringCan")
				? NSLOCTEXT(
					"BotanicusGrowing",
					"WaterPlant",
					"Arroser")
				: NSLOCTEXT(
					"BotanicusGrowing",
					"InspectPlant",
					"Observer");
	}
	// The generic prompt describes E. Pot contents are handled separately by
	// the controller's left-mouse action.
	Prompt.ActionText = InteractionAction;
	Prompt.bCanInteract = IsValid(Interactor);
	return Prompt;
}

bool ABotanicusPlantPotActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return false;
}

void ABotanicusPlantPotActor::Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"E sert a deplacer le pot. Utilisez le clic gauche pour agir."));
	}
}

void ABotanicusPlantPotActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (StatusText)
	{
		StatusText->SetVisibility(false);
	}
}

void ABotanicusPlantPotActor::BeginPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!QuickBar)
	{
		return;
	}
	if (ActivePrimaryUser.IsValid())
	{
		if (ActivePrimaryUser.Get() != Character)
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Ce pot est deja utilise par un autre joueur."));
		}
		return;
	}
	const FName SelectedItemKey =
		QuickBar->GetSelectedSlot().ItemKey;
	UE_LOG(
		LogBotanicus,
		Display,
		TEXT(
			"Plant pot interaction: selected=%s soil=%s plant=%s water=%.2f waterings=%d."),
		*SelectedItemKey.ToString(),
		bHasSoil ? TEXT("yes") : TEXT("no"),
		PlantKey.IsNone() ? TEXT("none") : *PlantKey.ToString(),
		WaterLevel,
		WateringCount);

	if (!bHasSoil)
	{
		if (SelectedItemKey != TEXT("PottingSoil"))
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Sélectionnez une dose de terreau dans la hotbar."));
			return;
		}
		ActivePrimaryUser = Character;
		PrimaryUseMode = EPrimaryUseMode::FillSoil;
		SoilFillProgress = 0.0f;
		SendInteractorMessage(
			Interactor,
			TEXT("Maintenez le clic gauche pour verser le terreau."));
	}
	else if (PlantKey.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		const FBotanicusPlantDefinition* Definition =
			Plants ? Plants->FindPlantBySeed(SelectedItemKey) : nullptr;
		if (!Definition || !QuickBar->ConsumeSelectedItem(1))
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Sélectionnez un sachet de graines compatible."));
			return;
		}
		PlantKey = Definition->PlantKey;
		GrowthProgress = 0.02f;
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("%s planté. Il faut maintenant arroser."),
				*Definition->DisplayName.ToString()));
	}
	else if (SelectedItemKey == TEXT("WateringCan"))
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		const FBotanicusPlantDefinition* Definition =
			Plants ? Plants->FindPlant(PlantKey) : nullptr;
		if (Definition)
		{
			ActivePrimaryUser = Character;
			PrimaryUseMode = EPrimaryUseMode::Water;
			bWateringActive = true;
			++WateringCount;
			SendInteractorMessage(
				Interactor,
				FString::Printf(
					TEXT("Pot arrosé : humidité %d%%."),
					FMath::RoundToInt(WaterLevel * 100.0f)));
		}
	}
	else
	{
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Croissance %d%% — humidité %d%%."),
				FMath::RoundToInt(GrowthProgress * 100.0f),
				FMath::RoundToInt(WaterLevel * 100.0f)));
		return;
	}

	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusPlantPotActor::EndPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	if (ActivePrimaryUser.IsValid() &&
		ActivePrimaryUser.Get() != Character)
	{
		return;
	}

	if (PrimaryUseMode == EPrimaryUseMode::FillSoil &&
		SoilFillProgress > 0.0f &&
		!bHasSoil)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Remplissage du pot annule."));
	}
	else if (PrimaryUseMode == EPrimaryUseMode::Water)
	{
		SendInteractorMessage(
			Interactor,
			FString::Printf(
				TEXT("Arrosage arrete : eau %d%%."),
				FMath::RoundToInt(WaterLevel * 100.0f)));
	}

	ActivePrimaryUser.Reset();
	PrimaryUseMode = EPrimaryUseMode::None;
	SoilFillProgress = 0.0f;
	bWateringActive = false;
	RefreshVisuals();
	ForceNetUpdate();
}

bool ABotanicusPlantPotActor::UpdatePrimaryUse(float DeltaSeconds)
{
	if (PrimaryUseMode == EPrimaryUseMode::None)
	{
		return false;
	}

	ABotanicusCharacter* Character = ActivePrimaryUser.Get();
	const bool bCharacterValid =
		IsValid(Character) &&
		FVector::DistSquared(
			Character->GetActorLocation(),
			GetActorLocation()) <=
			FMath::Square(450.0f);
	const FName SelectedItemKey =
		bCharacterValid
			? GetSelectedItemKey(Character)
			: NAME_None;

	if (PrimaryUseMode == EPrimaryUseMode::FillSoil)
	{
		if (!bCharacterValid ||
			SelectedItemKey != TEXT("PottingSoil") ||
			bHasSoil)
		{
			EndPrimaryUse(Character);
			return true;
		}

		SoilFillProgress = FMath::Clamp(
			SoilFillProgress +
				DeltaSeconds /
					FMath::Max(0.1f, SoilFillDuration),
			0.0f,
			1.0f);
		if (SoilFillProgress >= 1.0f)
		{
			UBotanicusQuickBarComponent* QuickBar =
				Character->GetQuickBarComponent();
			if (QuickBar &&
				QuickBar->ConsumeSelectedItem(1))
			{
				bHasSoil = true;
				SendInteractorMessage(
					Character,
					TEXT("Le pot est rempli de terreau."));
			}
			ActivePrimaryUser.Reset();
			PrimaryUseMode = EPrimaryUseMode::None;
			SoilFillProgress = 0.0f;
		}
		return true;
	}

	if (!bCharacterValid ||
		SelectedItemKey != TEXT("WateringCan") ||
		PlantKey.IsNone())
	{
		EndPrimaryUse(Character);
		return true;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* Definition =
		Plants ? Plants->FindPlant(PlantKey) : nullptr;
	if (!Definition)
	{
		EndPrimaryUse(Character);
		return true;
	}

	WaterLevel = FMath::Clamp(
		WaterLevel +
			FMath::Max(0.0f, Definition->WaterAddedPerUse) *
				DeltaSeconds,
		0.0f,
		1.0f);
	return true;
}

void ABotanicusPlantPotActor::RestoreWateringCount(
	int32 InWateringCount)
{
	if (!HasAuthority())
	{
		return;
	}
	WateringCount = FMath::Max(0, InWateringCount);
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusPlantPotActor::RestoreGrowingState(
	bool bInHasSoil,
	FName InPlantKey,
	float InWaterLevel,
	float InGrowthProgress)
{
	if (!HasAuthority())
	{
		return;
	}
	bHasSoil = bInHasSoil;
	PlantKey = InPlantKey;
	WaterLevel = FMath::Clamp(InWaterLevel, 0.0f, 1.0f);
	GrowthProgress = FMath::Clamp(InGrowthProgress, 0.0f, 1.0f);
	RefreshVisuals();
	ForceNetUpdate();
}

FName ABotanicusPlantPotActor::GetSelectedItemKey(
	AActor* Interactor) const
{
	const ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	return QuickBar
		? QuickBar->GetSelectedSlot().ItemKey
		: NAME_None;
}

void ABotanicusPlantPotActor::RefreshVisuals()
{
	if (SoilMesh)
	{
		SoilMesh->SetVisibility(bHasSoil);
	}
	const bool bHasPlant = !PlantKey.IsNone();
	const float VisualGrowth =
		FMath::Clamp(GrowthProgress, 0.02f, 1.0f);
	const float StemHeight = FMath::Lerp(8.0f, 80.0f, VisualGrowth);
	if (StemMesh)
	{
		StemMesh->SetVisibility(bHasPlant);
		StemMesh->SetRelativeLocation(
			FVector(0.0f, 0.0f, 34.0f + StemHeight * 0.5f));
		StemMesh->SetRelativeScale3D(
			FVector(0.035f, 0.035f, StemHeight / 100.0f));
	}
	if (FoliageMesh)
	{
		FoliageMesh->SetVisibility(bHasPlant);
		FoliageMesh->SetRelativeLocation(
			FVector(0.0f, 0.0f, 34.0f + StemHeight));
		const float FoliageScale =
			FMath::Lerp(0.06f, 0.32f, VisualGrowth);
		FoliageMesh->SetRelativeScale3D(
			FVector(FoliageScale, FoliageScale, FoliageScale * 0.7f));
	}
	if (StatusText)
	{
		FString ActionStatus = TEXT("AUCUNE");
		if (SoilFillProgress > 0.0f)
		{
			ActionStatus = FString::Printf(
				TEXT("TERREAU %d%%"),
				FMath::RoundToInt(SoilFillProgress * 100.0f));
		}
		else if (bWateringActive)
		{
			ActionStatus = TEXT("ARROSAGE EN COURS");
		}
		StatusText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"TERREAU : %s\nGRAINE : %s\nARROSAGES : %d\nEAU : %d%%\nCROISSANCE : %d%%\nACTION : %s"),
					bHasSoil ? TEXT("OUI") : TEXT("NON"),
					bHasPlant ? TEXT("OUI") : TEXT("NON"),
					WateringCount,
					FMath::RoundToInt(WaterLevel * 100.0f),
					FMath::RoundToInt(GrowthProgress * 100.0f),
					*ActionStatus)));
		StatusText->SetTextRenderColor(
			bHasSoil
				? FColor(120, 255, 150)
				: FColor(255, 190, 80));
	}
}

void ABotanicusPlantPotActor::SendInteractorMessage(
	AActor* Interactor,
	const FString& Message) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller)
	{
		Controller->ClientMessage(Message);
	}
}

void ABotanicusPlantPotActor::OnRep_GrowingState()
{
	RefreshVisuals();
}
