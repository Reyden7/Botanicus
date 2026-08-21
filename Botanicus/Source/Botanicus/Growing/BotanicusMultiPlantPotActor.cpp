// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growing/BotanicusMultiPlantPotActor.h"

#include "BotanicusCharacter.h"
#include "Building/BotanicusElementalGreenhouseActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Environment/BotanicusEnvironmentSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Growing/BotanicusPlantSubsystem.h"
#include "Growing/BotanicusPlantCompatibility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/BotanicusPlantEnvironmentAlertWidget.h"
#include "UI/BotanicusPlantEnvironmentDebugWidget.h"

ABotanicusMultiPlantPotActor::ABotanicusMultiPlantPotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	InteractionName =
		NSLOCTEXT(
			"BotanicusGrowing",
			"MultiPlantPot",
			"Jardiniere de preparation");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		SoilMaterialFinder(
			TEXT("/Game/Botanicus/Materials/Soil/M_BotanicusSoilWetPatchesV2.M_BotanicusSoilWetPatchesV2"));
	if (SoilMaterialFinder.Succeeded())
	{
		MultiSoilMaterial = SoilMaterialFinder.Object;
	}

	TArray<UTextRenderComponent*> InheritedTexts;
	GetComponents(InheritedTexts);
	for (UTextRenderComponent* Text : InheritedTexts)
	{
		if (Text)
		{
			Text->SetVisibility(false);
		}
	}
	TArray<UStaticMeshComponent*> InheritedMeshes;
	GetComponents(InheritedMeshes);
	for (UStaticMeshComponent* Component : InheritedMeshes)
	{
		if (Component && Component != Mesh)
		{
			Component->SetVisibility(false);
		}
	}

	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.75f, 0.42f, 0.3f));
	}

	MultiSoilMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("MultiSoil"));
	MultiSoilMesh->SetupAttachment(SceneRoot);
	MultiSoilMesh->SetStaticMesh(
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
	MultiSoilMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	if (MultiSoilMaterial)
	{
		MultiSoilMesh->SetMaterial(0, MultiSoilMaterial);
	}

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Stem =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("MultiStem_%d"), Index));
		Stem->SetupAttachment(SceneRoot);
		Stem->SetStaticMesh(
			CylinderFinder.Succeeded()
				? CylinderFinder.Object
				: nullptr);
		Stem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MultiStemMeshes.Add(Stem);

		UStaticMeshComponent* Flower =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("MultiFlower_%d"), Index));
		Flower->SetupAttachment(SceneRoot);
		Flower->SetStaticMesh(
			SphereFinder.Succeeded() ? SphereFinder.Object : nullptr);
		Flower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MultiFlowerMeshes.Add(Flower);

		UWidgetComponent* AlertWidget =
			CreateDefaultSubobject<UWidgetComponent>(
				*FString::Printf(TEXT("EnvironmentAlerts_%d"), Index));
		AlertWidget->SetupAttachment(SceneRoot);
		AlertWidget->SetWidgetSpace(EWidgetSpace::World);
		AlertWidget->SetDrawSize(FVector2D(240.0f, 72.0f));
		AlertWidget->SetPivot(FVector2D(0.5f, 0.5f));
		AlertWidget->SetRelativeScale3D(FVector(0.16f));
		AlertWidget->SetTwoSided(true);
		AlertWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AlertWidget->SetWidgetClass(
			UBotanicusPlantEnvironmentAlertWidget::StaticClass());
		AlertWidget->SetVisibility(false);
		EnvironmentAlertWidgets.Add(AlertWidget);

		UWidgetComponent* DebugWidget =
			CreateDefaultSubobject<UWidgetComponent>(
				*FString::Printf(TEXT("EnvironmentDebug_%d"), Index));
		DebugWidget->SetupAttachment(SceneRoot);
		DebugWidget->SetWidgetSpace(EWidgetSpace::World);
		DebugWidget->SetDrawSize(FVector2D(260.0f, 108.0f));
		DebugWidget->SetPivot(FVector2D(0.5f, 0.5f));
		DebugWidget->SetRelativeScale3D(FVector(0.12f));
		DebugWidget->SetTwoSided(true);
		DebugWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DebugWidget->SetWidgetClass(
			UBotanicusPlantEnvironmentDebugWidget::StaticClass());
		DebugWidget->SetVisibility(false);
		EnvironmentDebugWidgets.Add(DebugWidget);
	}

	MultiStatusText =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("MultiPlantStatus"));
	MultiStatusText->SetupAttachment(SceneRoot);
	MultiStatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
	MultiStatusText->SetHorizontalAlignment(EHTA_Center);
	MultiStatusText->SetVerticalAlignment(EVRTA_TextCenter);
	MultiStatusText->SetWorldSize(13.0f);
	MultiStatusText->SetTextRenderColor(FColor(110, 220, 255));
	MultiStatusText->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	PlantSlots.SetNum(4);
	RefreshVisuals();
}

void ABotanicusMultiPlantPotActor::BeginPlay()
{
	Super::BeginPlay();
	if (MultiSoilMesh && MultiSoilMaterial)
	{
		MultiSoilDynamicMaterial = UMaterialInstanceDynamic::Create(
			MultiSoilMaterial,
			this);
		if (MultiSoilDynamicMaterial)
		{
			MultiSoilMesh->SetMaterial(0, MultiSoilDynamicMaterial);
		}
	}
	UClass* WidgetClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Botanicus/UI/Plant/Environment/WBP_PlantEnvironmentAlerts.WBP_PlantEnvironmentAlerts_C"));
	for (UWidgetComponent* AlertWidget : EnvironmentAlertWidgets)
	{
		if (!AlertWidget)
		{
			continue;
		}
		if (WidgetClass)
		{
			AlertWidget->SetWidgetClass(WidgetClass);
		}
		AlertWidget->InitWidget();
	}
	UClass* DebugWidgetClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Botanicus/UI/Plant/Environment/WBP_PlantEnvironmentDebug.WBP_PlantEnvironmentDebug_C"));
	for (UWidgetComponent* DebugWidget : EnvironmentDebugWidgets)
	{
		if (!DebugWidget)
		{
			continue;
		}
		if (DebugWidgetClass)
		{
			DebugWidget->SetWidgetClass(DebugWidgetClass);
		}
		DebugWidget->InitWidget();
	}
	RefreshVisuals();
}

int32 ABotanicusMultiPlantPotActor::GetPlantCapacity() const
{
	if (GetItemKey() == TEXT("PreparationPlanter4"))
	{
		return 4;
	}
	if (GetItemKey() == TEXT("PreparationPlanter3"))
	{
		return 3;
	}
	return 2;
}

void ABotanicusMultiPlantPotActor::Tick(float DeltaSeconds)
{
	// Skip the one-plant simulation implemented by the parent class.
	ABotanicusPlaceableItemActor::Tick(DeltaSeconds);
	RefreshSeedInteractionPreview();

	if (MultiStatusText)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const APlayerController* Controller =
				World->GetFirstPlayerController())
			{
				if (Controller->PlayerCameraManager)
				{
					const FVector CameraLocation =
						Controller->PlayerCameraManager->GetCameraLocation();
					MultiStatusText->SetWorldRotation(
						(CameraLocation -
						 MultiStatusText->GetComponentLocation()).
							Rotation());
					for (UWidgetComponent* AlertWidget : EnvironmentAlertWidgets)
					{
						if (AlertWidget)
						{
							AlertWidget->SetWorldRotation(
								(CameraLocation -
								 AlertWidget->GetComponentLocation()).Rotation());
						}
					}
					FVector CameraRight =
						Controller->PlayerCameraManager->GetCameraRotation().
							RotateVector(FVector::RightVector);
					CameraRight.Z = 0.0f;
					CameraRight = CameraRight.GetSafeNormal();
					for (int32 Index = 0;
						 Index < EnvironmentDebugWidgets.Num();
						 ++Index)
					{
						UWidgetComponent* DebugWidget =
							EnvironmentDebugWidgets[Index];
						if (!DebugWidget)
						{
							continue;
						}
						const FVector PlantWorldLocation =
							GetActorTransform().TransformPosition(
								GetSlotLocalLocation(Index));
						DebugWidget->SetWorldLocation(
							PlantWorldLocation + FVector::UpVector * 90.0f +
							CameraRight * 62.0f);
						DebugWidget->SetWorldRotation(
							(CameraLocation -
							 DebugWidget->GetComponentLocation()).Rotation());
					}
				}
			}
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	bool bChanged = UpdateActiveUse(DeltaSeconds);
	bChanged |= ProcessElementalInteractions();
	const int32 Capacity = GetPlantCapacity();
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		FBotanicusMultiPlantSlotState& Slot = PlantSlots[Index];
		if (Slot.PlantKey.IsNone())
		{
			continue;
		}
		const FBotanicusPlantDefinition* Definition =
			FindPlant(Slot.PlantKey);
		if (!Definition)
		{
			continue;
		}
		const FBotanicusPlantEnvironmentState NewEnvironmentState =
			EvaluateEnvironmentState(*Definition);
		if (!Slot.EnvironmentState.IsNearlyEqual(NewEnvironmentState))
		{
			Slot.EnvironmentState = NewEnvironmentState;
			bChanged = true;
		}
		if (Slot.bElementalDead)
		{
			continue;
		}

		const float PreviousWater = Slot.WaterLevel;
		const float PreviousGrowth = Slot.GrowthProgress;
		const float PreviousCareScore = Slot.CareScore;
		Slot.WaterLevel = FMath::Clamp(
			Slot.WaterLevel -
				FMath::Max(
					0.0f,
					Definition->WaterConsumptionPerSecond) *
					DeltaSeconds,
			0.0f,
			1.0f);
		if (Definition->Element == EBotanicusPlantElement::Fire &&
			Slot.WaterLevel > Definition->MaximumHealthyWater)
		{
			const float ExcessRange = FMath::Max(
				0.01f,
				1.0f - Definition->MaximumHealthyWater);
			const float ExcessSeverity = FMath::Clamp(
				(Slot.WaterLevel - Definition->MaximumHealthyWater) /
					ExcessRange,
				0.0f,
				1.0f);
			Slot.CareScore = FMath::Clamp(
				Slot.CareScore -
					FMath::Max(
						0.0f,
						Definition->FireOverwateringCareLossPerSecond) *
					DeltaSeconds *
					FMath::Lerp(0.25f, 1.0f, ExcessSeverity),
				0.0f,
				1.0f);
		}
		if (Slot.EnvironmentState.bEnvironmentAvailable &&
			Slot.WaterLevel >= Definition->MinimumHealthyWater &&
			Slot.WaterLevel <= Definition->MaximumHealthyWater &&
			Slot.GrowthProgress < 1.0f)
		{
			const float GrowthAdded =
				DeltaSeconds /
				FMath::Max(
					1.0f,
					Definition->GrowthDurationSeconds) *
				Slot.EnvironmentState.GrowthRateMultiplier *
				EvaluateBotanicusPlantCompatibility(
					GetWorld(),
					GetActorTransform().TransformPosition(
						GetSlotLocalLocation(Index)),
					this,
					Index,
					*Definition).GrowthMultiplier;
			Slot.GrowthProgress = FMath::Clamp(
				Slot.GrowthProgress + GrowthAdded,
				0.0f,
				1.0f);
			const float WaterMidpoint =
				(Definition->MinimumHealthyWater +
				 Definition->MaximumHealthyWater) * 0.5f;
			const float WaterHalfRange = FMath::Max(
				0.01f,
				(Definition->MaximumHealthyWater -
				 Definition->MinimumHealthyWater) * 0.5f);
			const float WaterCare = 1.0f - FMath::Clamp(
				FMath::Abs(Slot.WaterLevel - WaterMidpoint) /
					WaterHalfRange,
				0.0f,
				1.0f);
			Slot.CareScore = FMath::Clamp(
				Slot.CareScore +
					(Slot.GrowthProgress - PreviousGrowth) *
						WaterCare * Slot.EnvironmentState.OverallComfort,
				0.0f,
				1.0f);
		}
		bChanged |=
			!FMath::IsNearlyEqual(
				PreviousWater,
				Slot.WaterLevel,
				0.0001f) ||
			!FMath::IsNearlyEqual(
				PreviousGrowth,
				Slot.GrowthProgress,
				0.0001f) ||
			!FMath::IsNearlyEqual(
				PreviousCareScore,
				Slot.CareScore,
				0.0001f);
	}

	if (bChanged)
	{
		RefreshVisuals();
		ForceNetUpdate();
	}
}

bool ABotanicusMultiPlantPotActor::CanPreviewSeedInteractionZone(
	AActor* Interactor) const
{
	const int32 SlotIndex = ResolveAimedSlot(Interactor);
	return SoilUnits >= GetPlantCapacity() &&
		PlantSlots.IsValidIndex(SlotIndex) &&
		PlantSlots[SlotIndex].PlantKey.IsNone();
}

void ABotanicusMultiPlantPotActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusMultiPlantPotActor, SoilUnits);
	DOREPLIFETIME(ABotanicusMultiPlantPotActor, PlantSlots);
}

FBotanicusInteractionPrompt
ABotanicusMultiPlantPotActor::
	GetInteractionPrompt_Implementation(
		AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	const int32 SlotIndex = ResolveAimedSlot(Interactor);
	const int32 Capacity = GetPlantCapacity();
	if (SoilUnits < Capacity)
	{
		Prompt.ActionText =
			GetSelectedItemKey(Interactor) == TEXT("PottingSoil")
				? FText::FromString(
					FString::Printf(
						TEXT("Maintenir clic gauche : terreau %d/%d"),
						SoilUnits,
						Capacity))
				: FText::FromString(TEXT("Selectionner du terreau"));
	}
	else if (PlantSlots.IsValidIndex(SlotIndex) &&
		PlantSlots[SlotIndex].PlantKey.IsNone())
	{
		Prompt.ActionText =
			FText::FromString(
				FString::Printf(
					TEXT("Planter dans l'emplacement %d"),
					SlotIndex + 1));
	}
	else if (IsMature(SlotIndex))
	{
		Prompt.ActionText = CanHarvest(SlotIndex, Interactor)
			? FText::FromString(
				FString::Printf(
					TEXT(
						"Maintenir clic gauche : recolter la plante %d"),
					SlotIndex + 1))
			: FText::FromString(
				TEXT("Petite pelle requise pour recolter"));
	}
	else
	{
		Prompt.ActionText =
			FText::FromString(
				FString::Printf(
					TEXT("Arroser / observer l'emplacement %d"),
					SlotIndex + 1));
	}
	Prompt.bCanInteract = false;
	return Prompt;
}

bool ABotanicusMultiPlantPotActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return CanHarvest(ResolveAimedSlot(Interactor), Interactor);
}

void ABotanicusMultiPlantPotActor::BeginPrimaryUse(
	AActor* Interactor)
{
	if (!HasAuthority() || ActiveUser.IsValid())
	{
		return;
	}
	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!Character || !QuickBar)
	{
		return;
	}

	const int32 Capacity = GetPlantCapacity();
	const FName SelectedKey =
		QuickBar->GetSelectedSlot().ItemKey;
	if (SoilUnits < Capacity)
	{
		if (SelectedKey != TEXT("PottingSoil"))
		{
			SendMessage(
				Interactor,
				TEXT("Selectionnez une dose de terreau."));
			return;
		}
		ActiveUser = Character;
		ActiveUseMode = EUseMode::FillSoil;
		ActiveProgress = 0.0f;
		return;
	}

	const int32 SlotIndex = ResolveAimedSlot(Interactor);
	if (!PlantSlots.IsValidIndex(SlotIndex))
	{
		return;
	}
	FBotanicusMultiPlantSlotState& Slot = PlantSlots[SlotIndex];
	if (Slot.bElementalDead)
	{
		SendMessage(
			Interactor,
			TEXT("Cette plante est morte a cause d'une reaction elementaire."));
		return;
	}
	if (Slot.PlantKey.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UBotanicusPlantSubsystem* Plants =
			GameInstance
				? GameInstance->GetSubsystem<
					UBotanicusPlantSubsystem>()
				: nullptr;
		const FBotanicusPlantDefinition* Definition =
			Plants ? Plants->FindPlantBySeed(SelectedKey) : nullptr;
		if (!Definition || !QuickBar->ConsumeSelectedItem(1))
		{
			SendMessage(
				Interactor,
				TEXT("Selectionnez des graines compatibles."));
			return;
		}
		Slot.PlantKey = Definition->PlantKey;
		Slot.bElementalDead = false;
		Slot.GrowthProgress = 0.02f;
		Slot.CareScore = 0.01f;
		SendMessage(
			Interactor,
			FString::Printf(
				TEXT("%s plante dans l'emplacement %d."),
				*Definition->DisplayName.ToString(),
				SlotIndex + 1));
		RefreshVisuals();
		ForceNetUpdate();
		return;
	}

	if (CanHarvest(SlotIndex, Interactor))
	{
		ActiveUser = Character;
		ActiveUseMode = EUseMode::Harvest;
		ActiveSlotIndex = SlotIndex;
		ActiveProgress = 0.0f;
		return;
	}
	if (QuickBar->HasSelectedWateringCan())
	{
		if (QuickBar->GetSelectedWateringCanWaterLevel() <=
			KINDA_SMALL_NUMBER)
		{
			SendMessage(Interactor, TEXT("L'arrosoir est vide."));
			return;
		}
		ActiveUser = Character;
		ActiveUseMode = EUseMode::Water;
		ActiveSlotIndex = SlotIndex;
		++Slot.WateringCount;
		return;
	}
	SendMessage(
		Interactor,
		FString::Printf(
			TEXT("Plante %d : croissance %d%%, eau %d%%."),
			SlotIndex + 1,
			FMath::RoundToInt(Slot.GrowthProgress * 100.0f),
			FMath::RoundToInt(Slot.WaterLevel * 100.0f)));
}

void ABotanicusMultiPlantPotActor::EndPrimaryUse(
	AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}
	ActiveUser.Reset();
	ActiveUseMode = EUseMode::None;
	ActiveSlotIndex = INDEX_NONE;
	ActiveProgress = 0.0f;
	RefreshVisuals();
	ForceNetUpdate();
}

bool ABotanicusMultiPlantPotActor::UpdateActiveUse(
	float DeltaSeconds)
{
	if (ActiveUseMode == EUseMode::None)
	{
		return false;
	}
	ABotanicusCharacter* Character = ActiveUser.Get();
	if (!IsValid(Character))
	{
		EndPrimaryUse(Character);
		return true;
	}

	if (ActiveUseMode == EUseMode::FillSoil)
	{
		if (GetSelectedItemKey(Character) != TEXT("PottingSoil") ||
			!IsInteractorStillTargeting(
				Character,
				ResolveAimedSlot(Character)))
		{
			EndPrimaryUse(Character);
			return true;
		}
		ActiveProgress += DeltaSeconds;
		if (ActiveProgress >= 1.0f)
		{
			ActiveProgress = 0.0f;
			if (Character->GetQuickBarComponent()->
				ConsumeSelectedItem(1))
			{
				SoilUnits = FMath::Min(
					SoilUnits + 1,
					GetPlantCapacity());
				SendMessage(
					Character,
					FString::Printf(
						TEXT("Terreau : %d/%d."),
						SoilUnits,
						GetPlantCapacity()));
			}
			if (SoilUnits >= GetPlantCapacity())
			{
				EndPrimaryUse(Character);
			}
		}
		return true;
	}

	if (!PlantSlots.IsValidIndex(ActiveSlotIndex) ||
		!IsInteractorStillTargeting(
			Character,
			ActiveSlotIndex))
	{
		EndPrimaryUse(Character);
		return true;
	}
	FBotanicusMultiPlantSlotState& Slot =
		PlantSlots[ActiveSlotIndex];
	const FBotanicusPlantDefinition* Definition =
		FindPlant(Slot.PlantKey);
	if (!Definition)
	{
		EndPrimaryUse(Character);
		return true;
	}

	if (ActiveUseMode == EUseMode::Water)
	{
		UBotanicusQuickBarComponent* QuickBar =
			Character ? Character->GetQuickBarComponent() : nullptr;
		if (!QuickBar || !QuickBar->HasSelectedWateringCan() ||
			QuickBar->GetSelectedWateringCanWaterLevel() <=
				KINDA_SMALL_NUMBER)
		{
			EndPrimaryUse(Character);
			return true;
		}
		if (Slot.WaterLevel >= 1.0f - KINDA_SMALL_NUMBER)
		{
			SendMessage(
				Character,
				TEXT(
					"Cette plante est suffisamment arrosee : eau 100%."));
			EndPrimaryUse(Character);
			return true;
		}
		const float RequestedWater =
			FMath::Max(
				0.0f,
				Definition->WaterAddedPerUse) *
				DeltaSeconds;
		const float PreviousWaterLevel = Slot.WaterLevel;
		Slot.WaterLevel = FMath::Clamp(
			Slot.WaterLevel + RequestedWater,
			0.0f,
			1.0f);
		const float WaterActuallyAdded =
			Slot.WaterLevel - PreviousWaterLevel;
		if (WaterActuallyAdded > KINDA_SMALL_NUMBER &&
			RequestedWater > KINDA_SMALL_NUMBER)
		{
			QuickBar->ConsumeSelectedWateringCanWater(
				0.12f * DeltaSeconds *
					(WaterActuallyAdded / RequestedWater));
		}
		if (Slot.WaterLevel >= 1.0f - KINDA_SMALL_NUMBER)
		{
			SendMessage(
				Character,
				TEXT(
					"Cette plante est suffisamment arrosee : eau 100%."));
			EndPrimaryUse(Character);
		}
		return true;
	}

	if (!CanHarvest(ActiveSlotIndex, Character))
	{
		EndPrimaryUse(Character);
		return true;
	}
	ActiveProgress +=
		DeltaSeconds /
			FMath::Max(0.1f, Definition->HarvestDurationSeconds);
	if (ActiveProgress >= 1.0f)
	{
		int32 AddedSlotIndex = INDEX_NONE;
		if (!Character->GetQuickBarComponent()->AddItem(
				GetHarvestItemKey(ActiveSlotIndex, *Definition),
				FMath::Max(1, Definition->HarvestQuantity),
				AddedSlotIndex))
		{
			SendMessage(
				Character,
				TEXT("Liberez une place dans la hotbar."));
			EndPrimaryUse(Character);
			return true;
		}
		Slot = FBotanicusMultiPlantSlotState();
		EndPrimaryUse(Character);
	}
	return true;
}

int32 ABotanicusMultiPlantPotActor::ResolveAimedSlot(
	AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* Controller =
		Pawn
			? Cast<APlayerController>(Pawn->GetController())
			: nullptr;
	if (!Controller)
	{
		return 0;
	}
	const FVector ViewLocation = Pawn->GetPawnViewLocation();
	const FRotator ViewRotation =
		Controller->GetControlRotation();
	FHitResult Hit;
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(BotanicusMultiPlanterAim),
		false,
		Pawn);
	FVector TargetLocation = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * 500.0f,
			ECC_Visibility,
			Params) &&
		Hit.GetActor() == this)
	{
		TargetLocation = Hit.ImpactPoint;
	}
	const FVector Local =
		GetActorTransform().InverseTransformPosition(TargetLocation);
	int32 BestIndex = 0;
	float BestDistance = MAX_flt;
	for (int32 Index = 0; Index < GetPlantCapacity(); ++Index)
	{
		const float Distance =
			FMath::Abs(
				Local.X - GetSlotLocalLocation(Index).X);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

FVector ABotanicusMultiPlantPotActor::GetSlotLocalLocation(
	int32 SlotIndex) const
{
	return FVector(
		(static_cast<float>(SlotIndex) -
		 (static_cast<float>(GetPlantCapacity() - 1) * 0.5f)) *
			55.0f,
		0.0f,
		18.0f);
}

const FBotanicusPlantDefinition*
ABotanicusMultiPlantPotActor::FindPlant(FName InPlantKey) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusPlantSubsystem* Plants =
		GameInstance
			? GameInstance->GetSubsystem<UBotanicusPlantSubsystem>()
			: nullptr;
	return Plants ? Plants->FindPlant(InPlantKey) : nullptr;
}

bool ABotanicusMultiPlantPotActor::IsMature(
	int32 SlotIndex) const
{
	return PlantSlots.IsValidIndex(SlotIndex) &&
		!PlantSlots[SlotIndex].PlantKey.IsNone() &&
		PlantSlots[SlotIndex].GrowthProgress >= 0.999f;
}

bool ABotanicusMultiPlantPotActor::CanHarvest(
	int32 SlotIndex,
	AActor* Interactor) const
{
	if (!IsMature(SlotIndex))
	{
		return false;
	}
	const FBotanicusPlantDefinition* Definition =
		FindPlant(PlantSlots[SlotIndex].PlantKey);
	return Definition &&
		GetSelectedItemKey(Interactor) ==
			Definition->HarvestToolItemKey;
}

FName ABotanicusMultiPlantPotActor::GetSelectedItemKey(
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

FName ABotanicusMultiPlantPotActor::GetHarvestItemKey(
	int32 SlotIndex,
	const FBotanicusPlantDefinition& Definition) const
{
	FString Key = Definition.HarvestItemKey.ToString();
	const FBotanicusMultiPlantSlotState& Slot =
		PlantSlots[SlotIndex];
	const float Rating =
		Slot.CareScore /
			FMath::Max(0.02f, Slot.GrowthProgress);
	if (Rating >= 0.8f)
	{
		Key += TEXT("_Exceptional");
	}
	else if (Rating >= 0.5f)
	{
		Key += TEXT("_Beautiful");
	}
	return FName(*Key);
}

bool ABotanicusMultiPlantPotActor::IsInteractorStillTargeting(
	AActor* Interactor,
	int32 SlotIndex) const
{
	return ResolveAimedSlot(Interactor) == SlotIndex &&
		FVector::DistSquared(
			Interactor->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(450.0f);
}

void ABotanicusMultiPlantPotActor::RefreshVisuals()
{
	const int32 Capacity = GetPlantCapacity();
	const float LengthScale =
		0.45f + static_cast<float>(Capacity) * 0.3f;
	Mesh->SetRelativeScale3D(
		FVector(LengthScale, 0.42f, 0.3f));
	MultiSoilMesh->SetRelativeLocation(
		FVector(0.0f, 0.0f, GetSoilMaximumHeight()));
	MultiSoilMesh->SetRelativeScale3D(
		FVector(
			LengthScale * 0.88f,
			0.34f,
			0.035f));
	MultiSoilMesh->SetVisibility(SoilUnits > 0);

	int32 PlantedCount = 0;
	int32 DeadCount = 0;
	float TotalSoilWetness = 0.0f;
	bool bOutsideGreenhouse = false;
	float TotalEnvironmentalComfort = 0.0f;
	int32 EnvironmentPlantCount = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const bool bActive = Index < Capacity;
		const FBotanicusMultiPlantSlotState& Slot =
			PlantSlots.IsValidIndex(Index)
				? PlantSlots[Index]
				: FBotanicusMultiPlantSlotState();
		const bool bPlanted =
			bActive && !Slot.PlantKey.IsNone();
		const FBotanicusPlantDefinition* Definition =
			bPlanted ? FindPlant(Slot.PlantKey) : nullptr;
		PlantedCount += bPlanted ? 1 : 0;
		TotalSoilWetness += bPlanted
			? FMath::Clamp(Slot.WaterLevel, 0.0f, 1.0f)
			: 0.0f;
		DeadCount += Slot.bElementalDead ? 1 : 0;
		if (bPlanted && !Slot.bElementalDead)
		{
			bOutsideGreenhouse |=
				!Slot.EnvironmentState.bInsideGreenhouse;
			TotalEnvironmentalComfort +=
				Slot.EnvironmentState.OverallComfort;
			++EnvironmentPlantCount;
		}
		const float Growth =
			FMath::Clamp(Slot.GrowthProgress, 0.02f, 1.0f);
		const FVector BaseLocation = GetSlotLocalLocation(Index);
		TSoftObjectPtr<UStaticMesh> StageMeshReference;
		if (Definition)
		{
			if (Slot.GrowthProgress >= 1.0f - KINDA_SMALL_NUMBER)
			{
				StageMeshReference = Definition->MatureGrowthMesh;
			}
			else if (Slot.GrowthProgress >= 0.70f)
			{
				StageMeshReference = Definition->LargeGrowthMesh;
			}
			else if (Slot.GrowthProgress >= 0.30f)
			{
				StageMeshReference = Definition->MediumGrowthMesh;
			}
			else
			{
				StageMeshReference = Definition->SmallGrowthMesh;
			}
		}
		UStaticMesh* StageMesh = StageMeshReference.IsNull()
			? nullptr
			: StageMeshReference.LoadSynchronous();
		const bool bUsesStageMesh = StageMesh != nullptr;
		const bool bShadowClosed =
			Definition &&
			Definition->Element == EBotanicusPlantElement::Shadow &&
			Slot.EnvironmentState.bEnvironmentAvailable &&
			Slot.EnvironmentState.LuminosityCondition ==
				EBotanicusPlantEnvironmentCondition::TooHigh;
		const float BehaviourHorizontalScale = bShadowClosed
			? FMath::Clamp(
				Definition->ShadowClosedHorizontalScale, 0.1f, 1.0f)
			: 1.0f;
		const float BehaviourVerticalScale = bShadowClosed
			? FMath::Clamp(
				Definition->ShadowClosedVerticalScale, 0.1f, 1.0f)
			: 1.0f;
		MultiStemMeshes[Index]->SetVisibility(
			bPlanted && !bUsesStageMesh);
		MultiFlowerMeshes[Index]->SetVisibility(bPlanted);
		if (bUsesStageMesh)
		{
			if (MultiFlowerMeshes[Index]->GetStaticMesh() != StageMesh)
			{
				MultiFlowerMeshes[Index]->SetStaticMesh(StageMesh);
				MultiFlowerMeshes[Index]->EmptyOverrideMaterials();
			}
			const FBox Bounds = StageMesh->GetBoundingBox();
			const float MeshHeight =
				FMath::Max(0.01f, Bounds.GetSize().Z);
			const float MinimumHeight = FMath::Max(
				0.1f, Definition->MinimumGrowthVisualHeight);
			const float MatureHeight = FMath::Max(
				MinimumHeight, Definition->MatureGrowthVisualHeight);
			const float DesiredHeight = FMath::Lerp(
				MinimumHeight, MatureHeight, Growth);
			const float UniformScale = DesiredHeight / MeshHeight;
			const FVector Centre = Bounds.GetCenter();
			const FVector BehaviourScale(
				UniformScale * BehaviourHorizontalScale,
				UniformScale * BehaviourHorizontalScale,
				UniformScale * BehaviourVerticalScale);
			MultiFlowerMeshes[Index]->SetRelativeScale3D(
				BehaviourScale);
			MultiFlowerMeshes[Index]->SetRelativeLocation(
				BaseLocation + FVector(
					-Centre.X * BehaviourScale.X,
					-Centre.Y * BehaviourScale.Y,
					-Bounds.Min.Z * BehaviourScale.Z));
		}
		else
		{
			static UStaticMesh* FallbackFlowerMesh =
				LoadObject<UStaticMesh>(
					nullptr,
					TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			if (FallbackFlowerMesh &&
				MultiFlowerMeshes[Index]->GetStaticMesh() !=
					FallbackFlowerMesh)
			{
				MultiFlowerMeshes[Index]->SetStaticMesh(
					FallbackFlowerMesh);
				MultiFlowerMeshes[Index]->EmptyOverrideMaterials();
			}
			MultiStemMeshes[Index]->SetRelativeLocation(
				BaseLocation + FVector(0.0f, 0.0f, 9.0f * Growth));
			MultiStemMeshes[Index]->SetRelativeScale3D(
				FVector(0.045f, 0.045f, 0.18f * Growth));
			MultiFlowerMeshes[Index]->SetRelativeLocation(
				BaseLocation +
					FVector(0.0f, 0.0f, 18.0f * Growth + 8.0f));
			MultiFlowerMeshes[Index]->SetRelativeScale3D(
				FVector(
					0.16f * Growth * BehaviourHorizontalScale,
					0.16f * Growth * BehaviourHorizontalScale,
					0.16f * Growth * BehaviourVerticalScale));
		}
		if (EnvironmentAlertWidgets.IsValidIndex(Index) &&
			EnvironmentAlertWidgets[Index])
		{
			UWidgetComponent* AlertComponent =
				EnvironmentAlertWidgets[Index];
			AlertComponent->SetRelativeLocation(
				BaseLocation + FVector(0.0f, 0.0f, 145.0f));
			if (!AlertComponent->GetWidget())
			{
				AlertComponent->InitWidget();
			}
			UBotanicusPlantEnvironmentAlertWidget* AlertWidget =
				Cast<UBotanicusPlantEnvironmentAlertWidget>(
					AlertComponent->GetWidget());
			if (AlertWidget)
			{
				AlertWidget->SetEnvironmentState(Slot.EnvironmentState);
			}
			AlertComponent->SetVisibility(
				bPlanted && AlertWidget && AlertWidget->HasAnyAlert() &&
				!ActorHasTag(TEXT("BotanicusPlacementPreview")));
		}
		if (EnvironmentDebugWidgets.IsValidIndex(Index) &&
			EnvironmentDebugWidgets[Index])
		{
			UWidgetComponent* DebugComponent =
				EnvironmentDebugWidgets[Index];
			if (!DebugComponent->GetWidget())
			{
				DebugComponent->InitWidget();
			}
			UBotanicusPlantEnvironmentDebugWidget* DebugWidget =
				Cast<UBotanicusPlantEnvironmentDebugWidget>(
					DebugComponent->GetWidget());
			if (DebugWidget)
			{
				DebugWidget->SetEnvironmentState(Slot.EnvironmentState);
			}
			DebugComponent->SetVisibility(
				bPlanted && DebugWidget &&
				!ActorHasTag(TEXT("BotanicusPlacementPreview")));
		}
	}
	if (MultiSoilDynamicMaterial)
	{
		const float AverageWetness = PlantedCount > 0
			? TotalSoilWetness / static_cast<float>(PlantedCount)
			: 0.0f;
		MultiSoilDynamicMaterial->SetScalarParameterValue(
			TEXT("Wetness"),
			FMath::Sqrt(AverageWetness));
	}
	if (MultiStatusText)
	{
		const FString EnvironmentStatus = EnvironmentPlantCount > 0
				? FString::Printf(
					TEXT("%s | CONFORT MOYEN %d%%"),
					bOutsideGreenhouse ? TEXT("EXTERIEUR") : TEXT("SERRE"),
					FMath::RoundToInt(
						TotalEnvironmentalComfort /
							static_cast<float>(EnvironmentPlantCount) * 100.0f))
				: TEXT("-");
		MultiStatusText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT(
						"JARDINIERE %d PLACES\nTERREAU : %d/%d\nPLANTES : %d/%d\nMORTES : %d\nENVIRONNEMENT : %s"),
					Capacity,
					SoilUnits,
					Capacity,
					PlantedCount,
					Capacity,
					DeadCount,
					*EnvironmentStatus)));
	}
}

void ABotanicusMultiPlantPotActor::ApplyElementalInfluence(
	EBotanicusPlantElement SourceElement,
	const FVector& SourceLocation)
{
	if (!HasAuthority() ||
		FVector::DistSquared(
			GetActorLocation(),
			SourceLocation) > FMath::Square(500.0f))
	{
		return;
	}
	bool bChanged = false;
	for (FBotanicusMultiPlantSlotState& Slot : PlantSlots)
	{
		if (Slot.bElementalDead || Slot.PlantKey.IsNone())
		{
			continue;
		}
		const FBotanicusPlantDefinition* Definition =
			FindPlant(Slot.PlantKey);
		if (!Definition)
		{
			continue;
		}
		const bool bBurnedByFire =
			SourceElement == EBotanicusPlantElement::Fire &&
			Definition->Element == EBotanicusPlantElement::Normal;
		const bool bExtinguishedByWater =
			SourceElement == EBotanicusPlantElement::Water &&
			Definition->Element == EBotanicusPlantElement::Fire;
		if (bBurnedByFire || bExtinguishedByWater)
		{
			Slot.bElementalDead = true;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		EndPrimaryUse(ActiveUser.Get());
		RefreshVisuals();
		ForceNetUpdate();
	}
}

bool ABotanicusMultiPlantPotActor::ProcessElementalInteractions()
{
	TArray<EBotanicusPlantElement> Sources;
	for (const FBotanicusMultiPlantSlotState& Slot : PlantSlots)
	{
		if (Slot.bElementalDead || Slot.PlantKey.IsNone())
		{
			continue;
		}
		const FBotanicusPlantDefinition* Definition =
			FindPlant(Slot.PlantKey);
		if (Definition &&
			(Definition->Element == EBotanicusPlantElement::Fire ||
			 Definition->Element == EBotanicusPlantElement::Water))
		{
			Sources.AddUnique(Definition->Element);
		}
	}
	if (Sources.Num() == 0)
	{
		return false;
	}

	int32 DeadBefore = 0;
	for (const FBotanicusMultiPlantSlotState& Slot : PlantSlots)
	{
		DeadBefore += Slot.bElementalDead ? 1 : 0;
	}
	const FVector SourceLocation = GetActorLocation();
	for (EBotanicusPlantElement Source : Sources)
	{
		ApplyElementalInfluence(Source, SourceLocation);
		for (TActorIterator<ABotanicusPlantPotActor> It(GetWorld());
			 It;
			 ++It)
		{
			if (*It == this ||
				FVector::DistSquared(
					It->GetActorLocation(),
					SourceLocation) > FMath::Square(500.0f))
			{
				continue;
			}
			if (ABotanicusMultiPlantPotActor* OtherMulti =
				Cast<ABotanicusMultiPlantPotActor>(*It))
			{
				OtherMulti->ApplyElementalInfluence(
					Source,
					SourceLocation);
			}
			else
			{
				It->ApplyElementalInfluence(
					Source,
					SourceLocation);
			}
		}
	}
	int32 DeadAfter = 0;
	for (const FBotanicusMultiPlantSlotState& Slot : PlantSlots)
	{
		DeadAfter += Slot.bElementalDead ? 1 : 0;
	}
	return DeadAfter != DeadBefore;
}

FBotanicusPlantEnvironmentState
ABotanicusMultiPlantPotActor::EvaluateEnvironmentState(
	const FBotanicusPlantDefinition& Definition) const
{
	const UBotanicusEnvironmentSubsystem* EnvironmentSubsystem =
		GetWorld()
			? GetWorld()->GetSubsystem<UBotanicusEnvironmentSubsystem>()
			: nullptr;
	float Temperature = 0.0f;
	float Humidity = 0.0f;
	float Luminosity = 0.0f;
	bool bInsideGreenhouse = false;
	const bool bEnvironmentAvailable = EnvironmentSubsystem &&
		EnvironmentSubsystem->GetEnvironmentAtLocation(
			GetActorLocation(),
			Temperature,
			Humidity,
			Luminosity,
			bInsideGreenhouse);
	return EvaluateBotanicusPlantEnvironment(
		Definition.Environment,
		bEnvironmentAvailable,
		bInsideGreenhouse,
		Temperature,
		Humidity,
		Luminosity);
}

FBotanicusPlantEnvironmentState
ABotanicusMultiPlantPotActor::GetSlotEnvironmentState(
	int32 SlotIndex) const
{
	return PlantSlots.IsValidIndex(SlotIndex)
		? PlantSlots[SlotIndex].EnvironmentState
		: FBotanicusPlantEnvironmentState();
}

void ABotanicusMultiPlantPotActor::SendMessage(
	AActor* Interactor,
	const FString& Message) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (APlayerController* Controller =
		Pawn
			? Cast<APlayerController>(Pawn->GetController())
			: nullptr)
	{
		Controller->ClientMessage(Message);
	}
}

void ABotanicusMultiPlantPotActor::RestoreMultiPlantState(
	int32 InSoilUnits,
	const TArray<FBotanicusMultiPlantSlotState>& InSlots)
{
	if (!HasAuthority())
	{
		return;
	}
	SoilUnits = FMath::Clamp(
		InSoilUnits,
		0,
		GetPlantCapacity());
	PlantSlots.SetNum(4);
	for (int32 Index = 0;
		 Index < FMath::Min(4, InSlots.Num());
		 ++Index)
	{
		PlantSlots[Index] = InSlots[Index];
	}
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusMultiPlantPotActor::OnRep_MultiPlantState()
{
	if (PlantSlots.Num() < 4)
	{
		PlantSlots.SetNum(4);
	}
	RefreshVisuals();
}

void ABotanicusMultiPlantPotActor::ConfigureAsLocalPreview(
	bool bIsValid)
{
	ABotanicusPlaceableItemActor::ConfigureAsLocalPreview(bIsValid);
	if (MultiStatusText)
	{
		MultiStatusText->SetVisibility(false);
	}
}
