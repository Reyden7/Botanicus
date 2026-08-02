// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sales/BotanicusSalePotActor.h"

#include "BotanicusCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Preparation/BotanicusPreparationWorkbenchActor.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FLinearColor SalePotPlantVisualColor(FName ColorTag)
{
	if (ColorTag == TEXT("Pink"))
	{
		return FLinearColor(0.95f, 0.20f, 0.55f);
	}
	if (ColorTag == TEXT("Purple"))
	{
		return FLinearColor(0.48f, 0.20f, 0.78f);
	}
	return FLinearColor(0.08f, 0.48f, 0.12f);
}

FString SalePotPlantQualityLabel(FName QualityTag)
{
	if (QualityTag == TEXT("Exceptional"))
	{
		return TEXT("EXCEPTIONNELLE");
	}
	if (QualityTag == TEXT("Beautiful"))
	{
		return TEXT("BELLE");
	}
	return TEXT("STANDARD");
}
}

ABotanicusSalePotActor::ABotanicusSalePotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CylinderFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderFinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.22f));
	}

	SoilVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Sale Pot Soil"));
	SoilVisual->SetupAttachment(SceneRoot);
	SoilVisual->SetStaticMesh(
		CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
	SoilVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 24.0f));
	SoilVisual->SetRelativeScale3D(FVector(0.23f, 0.23f, 0.025f));
	SoilVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlantVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Sale Pot Plant"));
	PlantVisual->SetupAttachment(SceneRoot);
	PlantVisual->SetStaticMesh(
		SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
	PlantVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 58.0f));
	PlantVisual->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.38f));
	PlantVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("Sale Pot Status"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(15.0f);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshVisuals();
}

void ABotanicusSalePotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Controller =
		World ? World->GetFirstPlayerController() : nullptr;
	if (StatusText && Controller && Controller->PlayerCameraManager)
	{
		StatusText->SetWorldRotation(
			(Controller->PlayerCameraManager->GetCameraLocation() -
			 StatusText->GetComponentLocation()).Rotation());
	}
	if (HasAuthority() && ActiveUser.IsValid())
	{
		UpdatePrimaryUse(DeltaSeconds);
		RefreshVisuals();
	}
}

void ABotanicusSalePotActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusSalePotActor, SoilItemKey);
	DOREPLIFETIME(ABotanicusSalePotActor, PlantItemKey);
}

void ABotanicusSalePotActor::ConfigureAsLocalPreview(bool bIsValid)
{
	Super::ConfigureAsLocalPreview(bIsValid);
	if (StatusText)
	{
		StatusText->SetVisibility(false);
	}
}

void ABotanicusSalePotActor::BeginPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() || ActiveUser.IsValid() || IsReadyForSale())
	{
		return;
	}
	if (!IsOnPreparationWorkbench())
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"Placez d'abord le pot de vente sur un établi de préparation."));
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
	const FName SelectedKey = QuickBar->GetSelectedSlot().ItemKey;
	if (!HasSoil())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusItemCatalogSubsystem* Catalog =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusItemCatalogSubsystem>()
				: nullptr;
		const FBotanicusItemDefinition* SoilDefinition =
			Catalog ? Catalog->FindItem(SelectedKey) : nullptr;
		if (!SoilDefinition || !SoilDefinition->bSaleSoil)
		{
			SendInteractorMessage(
				Interactor,
				TEXT("Selectionnez un terreau compatible."));
			return;
		}
		ActiveUser = Character;
		PendingSoilItemKey = SelectedKey;
		SoilFillProgress = 0.0f;
		SendInteractorMessage(
			Interactor,
			TEXT("Maintenez le clic gauche pour remplir le pot de vente."));
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	const FBotanicusPlantDefinition* Plant =
		Plants ? Plants->FindPlantByHarvestItem(SelectedKey) : nullptr;
	if (!Plant)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Selectionnez une plante entiere recoltee."));
		return;
	}
	if (Plant->CompatibleSaleSoilItemKey != SoilItemKey)
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Ce terreau n'est pas compatible avec cette plante."));
		return;
	}
	if (!QuickBar->ConsumeSelectedItem(1))
	{
		return;
	}
	PlantItemKey = SelectedKey;
	RefreshVisuals();
	ForceNetUpdate();
	SendInteractorMessage(
		Interactor,
		FString::Printf(
			TEXT(
				"%s rempote. Maintenez E pour placer le pot sur un presentoir."),
			*Plant->DisplayName.ToString()));
}

void ABotanicusSalePotActor::EndPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() ||
		(ActiveUser.IsValid() && ActiveUser.Get() != Interactor))
	{
		return;
	}
	ActiveUser.Reset();
	SoilFillProgress = 0.0f;
	PendingSoilItemKey = NAME_None;
	RefreshVisuals();
}

void ABotanicusSalePotActor::RestoreSalePotState(
	FName InSoilItemKey,
	FName InPlantItemKey)
{
	if (!HasAuthority())
	{
		return;
	}
	SoilItemKey = InSoilItemKey;
	PlantItemKey = InPlantItemKey;
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusSalePotActor::UpdatePrimaryUse(float DeltaSeconds)
{
	ABotanicusCharacter* Character = ActiveUser.Get();
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!IsValid(Character) || !QuickBar ||
		!IsOnPreparationWorkbench() ||
		!IsInteractorStillTargeting(Character) ||
		QuickBar->GetSelectedSlot().ItemKey != PendingSoilItemKey ||
		HasSoil())
	{
		EndPrimaryUse(Character);
		return;
	}
	SoilFillProgress = FMath::Clamp(
		SoilFillProgress +
			DeltaSeconds / FMath::Max(0.1f, SoilFillDuration),
		0.0f,
		1.0f);
	if (SoilFillProgress >= 1.0f)
	{
		if (QuickBar->ConsumeSelectedItem(1))
		{
			SoilItemKey = PendingSoilItemKey;
			SendInteractorMessage(
				Character,
				TEXT("Terreau ajoute au pot de vente."));
		}
		ActiveUser.Reset();
		SoilFillProgress = 0.0f;
		PendingSoilItemKey = NAME_None;
		ForceNetUpdate();
	}
}

bool ABotanicusSalePotActor::IsOnPreparationWorkbench() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ABotanicusPreparationWorkbenchActor> WorkbenchIt(
			 World);
		 WorkbenchIt;
		 ++WorkbenchIt)
	{
		if (WorkbenchIt->IsLocationOnPreparationSlot(
				GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

bool ABotanicusSalePotActor::IsInteractorStillTargeting(
	AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* Controller =
		Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Pawn || !Controller ||
		FVector::DistSquared(
			Pawn->GetActorLocation(),
			GetActorLocation()) > FMath::Square(450.0f))
	{
		return false;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Query(
		SCENE_QUERY_STAT(BotanicusSalePotUse),
		false,
		Pawn);
	return GetWorld()->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			GetActorLocation(),
			ECC_Visibility,
			Query) &&
		Hit.GetActor() == this;
}

void ABotanicusSalePotActor::RefreshVisuals()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(PlantItemKey) : nullptr;
	if (SoilVisual)
	{
		SoilVisual->SetVisibility(HasSoil());
	}
	if (PlantVisual)
	{
		PlantVisual->SetVisibility(IsReadyForSale());
		FVector PlantScale(0.25f, 0.25f, 0.38f);
		if (Definition)
		{
			if (Definition->PlantTypeTag == TEXT("Flowering"))
			{
				PlantScale = FVector(0.22f, 0.22f, 0.48f);
			}
			else if (Definition->PlantTypeTag == TEXT("Foliage"))
			{
				PlantScale = FVector(0.38f, 0.34f, 0.30f);
			}
			else if (Definition->PlantColorTag == TEXT("Purple"))
			{
				PlantScale = FVector(0.20f, 0.20f, 0.50f);
			}
			if (!PlantMaterial)
			{
				PlantMaterial =
					PlantVisual->CreateAndSetMaterialInstanceDynamic(0);
			}
			if (PlantMaterial)
			{
				PlantMaterial->SetVectorParameterValue(
					TEXT("Color"),
					SalePotPlantVisualColor(
						Definition->PlantColorTag));
			}
		}
		PlantVisual->SetRelativeScale3D(PlantScale);
	}
	if (!StatusText)
	{
		return;
	}
	const FString SoilStatus = HasSoil()
		? TEXT("OUI")
		: TEXT("NON");
	const FString PlantStatus = IsReadyForSale()
		? Definition
			? Definition->DisplayName.ToString().ToUpper()
			: PlantItemKey.ToString().ToUpper()
		: TEXT("NON");
	const FString QualityStatus = Definition
		? SalePotPlantQualityLabel(Definition->PlantQualityTag)
		: TEXT("-");
	const FString Progress =
		ActiveUser.IsValid()
			? FString::Printf(
				TEXT("\nREMPLISSAGE : %d%%"),
				FMath::RoundToInt(SoilFillProgress * 100.0f))
			: FString();
	StatusText->SetText(
		FText::FromString(
			FString::Printf(
				TEXT(
					"POT DE VENTE\nETABLI : %s\nTERREAU COMPATIBLE : %s\nPLANTE : %s\nQUALITE : %s%s"),
				IsOnPreparationWorkbench() ? TEXT("OUI") : TEXT("NON"),
				*SoilStatus,
				*PlantStatus,
				*QualityStatus,
				*Progress)));
	StatusText->SetTextRenderColor(
		IsReadyForSale()
			? FColor(80, 255, 110)
			: FColor(245, 225, 160));
}

void ABotanicusSalePotActor::SendInteractorMessage(
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

void ABotanicusSalePotActor::OnRep_SalePotState()
{
	RefreshVisuals();
}
