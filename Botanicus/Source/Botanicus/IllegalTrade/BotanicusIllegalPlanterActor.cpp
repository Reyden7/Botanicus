// Copyright Epic Games, Inc. All Rights Reserved.

#include "IllegalTrade/BotanicusIllegalPlanterActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IllegalTrade/BotanicusIllegalTradeSettings.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UI/BotanicusPlantGrowthWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Visuals/BotanicusPotSoilVisualActor.h"

namespace
{
	constexpr int32 MaximumVisualSlots = 12;

	UBotanicusQuickBarComponent* GetQuickBar(AActor* Interactor)
	{
		const ABotanicusCharacter* Character = Cast<ABotanicusCharacter>(Interactor);
		return Character ? Character->GetQuickBarComponent() : nullptr;
	}

	int32 GetGrowthStage(float Progress)
	{
		return Progress >= 0.70f ? 3 : Progress >= 0.30f ? 2 : 1;
	}
}

ABotanicusIllegalPlanterActor::ABotanicusIllegalPlanterActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.25f;

	// The inherited mesh is the bottom of the planter. Four separate wall
	// components leave the centre open so the rising soil surface stays visible.
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
	Mesh->SetRelativeScale3D(FVector(2.4f, 0.75f, 0.08f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));

	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	const auto CreateWall = [this, CubeMesh](
		const TCHAR* Name, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Wall->SetupAttachment(SceneRoot);
		Wall->SetRelativeLocation(Location);
		Wall->SetRelativeScale3D(Scale);
		Wall->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		if (CubeMesh) Wall->SetStaticMesh(CubeMesh);
		return Wall;
	};
	LeftWallVisual = CreateWall(TEXT("LeftWallVisual"), FVector(0.0f, -33.5f, 22.0f), FVector(2.4f, 0.08f, 0.30f));
	RightWallVisual = CreateWall(TEXT("RightWallVisual"), FVector(0.0f, 33.5f, 22.0f), FVector(2.4f, 0.08f, 0.30f));
	FrontWallVisual = CreateWall(TEXT("FrontWallVisual"), FVector(116.0f, 0.0f, 22.0f), FVector(0.08f, 0.60f, 0.30f));
	BackWallVisual = CreateWall(TEXT("BackWallVisual"), FVector(-116.0f, 0.0f, 22.0f), FVector(0.08f, 0.60f, 0.30f));

	SoilShapeVisual = CreateDefaultSubobject<UChildActorComponent>(TEXT("SoilShapeVisual"));
	SoilShapeVisual->SetupAttachment(SceneRoot);
	SoilShapeVisual->SetChildActorClass(ABotanicusPotSoilVisualActor::StaticClass());
	SoilShapeVisual->SetRelativeLocation(FVector(0.0f, 0.0f, SoilBottomHeight));

	for (int32 Index = 0; Index < MaximumVisualSlots; ++Index)
	{
		UStaticMeshComponent* Visual = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("IllegalPlantVisual_%02d"), Index));
		Visual->SetupAttachment(SceneRoot);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (ConeFinder.Succeeded())
		{
			Visual->SetStaticMesh(ConeFinder.Object);
		}
		PlantVisuals.Add(Visual);

		UWidgetComponent* GrowthWidget =
			CreateDefaultSubobject<UWidgetComponent>(
				*FString::Printf(TEXT("IllegalPlantGrowthWidget_%02d"), Index));
		GrowthWidget->SetupAttachment(SceneRoot);
		GrowthWidget->SetWidgetSpace(EWidgetSpace::World);
		GrowthWidget->SetDrawSize(FVector2D(360.0f, 180.0f));
		GrowthWidget->SetPivot(FVector2D(0.5f, 0.5f));
		GrowthWidget->SetRelativeScale3D(FVector(PlantGrowthWidgetScale));
		GrowthWidget->SetTintColorAndOpacity(
			FLinearColor(1.5f, 1.5f, 1.5f, 1.0f));
		GrowthWidget->SetTwoSided(true);
		GrowthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GrowthWidget->SetWidgetClass(
			UBotanicusPlantGrowthWidget::StaticClass());
		GrowthWidget->SetVisibility(false);
		PlantGrowthWidgets.Add(GrowthWidget);
	}

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 105.0f));
	StatusText->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetWorldSize(18.0f);
	StatusText->SetTextRenderColor(FColor(170, 110, 255));
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionName = NSLOCTEXT("BotanicusIllegalTrade", "IllegalPlanterName", "Jardinière clandestine");
}

void ABotanicusIllegalPlanterActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureSlotCount();
	UClass* GrowthWidgetClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Botanicus/UI/Plant/WBP_PlantGrowthInfo.WBP_PlantGrowthInfo_C"));
	for (UWidgetComponent* GrowthWidget : PlantGrowthWidgets)
	{
		if (!GrowthWidget)
		{
			continue;
		}
		if (GrowthWidgetClass)
		{
			GrowthWidget->SetWidgetClass(GrowthWidgetClass);
		}
		GrowthWidget->InitWidget();
	}
	RefreshVisuals();
}

void ABotanicusIllegalPlanterActor::ApplyItemDefinition()
{
	Super::ApplyItemDefinition();
	if (!UsesBlueprintAppearance())
	{
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
		Mesh->SetRelativeScale3D(FVector(2.4f, 0.75f, 0.08f));
	}
}

void ABotanicusIllegalPlanterActor::EnsureSlotCount()
{
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	const int32 SlotCount = Settings
		? FMath::Clamp(Settings->GetPlanterLevel(PlanterLevel).SlotCount, 1, MaximumVisualSlots)
		: 4;
	PlantSlots.SetNum(SlotCount);
}

float ABotanicusIllegalPlanterActor::GetWaterAmount() const
{
	float TotalWater = 0.0f;
	for (const FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
	{
		TotalWater += Slot.WaterAmount;
	}
	return TotalWater;
}

void ABotanicusIllegalPlanterActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshGrowthWidgets();
	if (HasAuthority() && UpdatePrimaryUse(DeltaSeconds))
	{
		RefreshVisuals();
		ForceNetUpdate();
		ScheduleAutosave();
	}
	if (!HasAuthority())
	{
		return;
	}

	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	if (!Settings)
	{
		return;
	}
	if (SoilUnits < Settings->GetPlanterLevel(PlanterLevel).RequiredSoilUnits)
	{
		return;
	}

	bool bStageChanged = false;
	bool bAnyGrowing = false;
	for (FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
	{
		if (Slot.PlantId.IsNone() || Slot.GrowthProgress >= 1.0f ||
			Slot.WaterAmount <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FBotanicusIllegalPlantDefinition* Plant = Settings->FindPlant(Slot.PlantId);
		if (!Plant)
		{
			continue;
		}
		const float RequestedWater =
			FMath::Max(0.0f, Plant->WaterConsumptionPerSecond) * DeltaSeconds;
		const float GrowthFraction = RequestedWater > KINDA_SMALL_NUMBER
			? FMath::Min(1.0f, Slot.WaterAmount / RequestedWater)
			: 1.0f;
		Slot.WaterAmount = FMath::Max(
			0.0f, Slot.WaterAmount - RequestedWater * GrowthFraction);
		const int32 PreviousStage = GetGrowthStage(Slot.GrowthProgress);
		Slot.GrowthProgress = FMath::Clamp(
			Slot.GrowthProgress + DeltaSeconds * GrowthFraction /
				FMath::Max(1.0f, Plant->GrowthDurationSeconds),
			0.0f,
			1.0f);
		bStageChanged |= PreviousStage != GetGrowthStage(Slot.GrowthProgress);
		bAnyGrowing = true;
	}
	if (!bAnyGrowing)
	{
		return;
	}

	float TotalGrowth = 0.0f;
	for (const FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
	{
		TotalGrowth += Slot.GrowthProgress;
	}
	const int32 GrowthBucket = FMath::FloorToInt(TotalGrowth * 10.0f);
	const int32 ReplicationBucket = FMath::FloorToInt(TotalGrowth * 100.0f);
	const float MaximumWater = Settings->GetPlanterLevel(PlanterLevel).MaximumWater;
	const int32 WaterBucket = FMath::FloorToInt(
		10.0f * GetWaterAmount() / FMath::Max(0.1f, MaximumWater));
	if (bStageChanged || ReplicationBucket != LastReplicatedGrowthBucket)
	{
		LastReplicatedGrowthBucket = ReplicationBucket;
		RefreshVisuals();
		ForceNetUpdate();
	}
	if (GrowthBucket != LastAutosaveGrowthBucket ||
		WaterBucket != LastAutosaveWaterBucket)
	{
		LastAutosaveGrowthBucket = GrowthBucket;
		LastAutosaveWaterBucket = WaterBucket;
		ScheduleAutosave();
	}
}

FBotanicusInteractionPrompt ABotanicusIllegalPlanterActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	Prompt.bCanInteract = false;
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	const UBotanicusQuickBarComponent* QuickBar = GetQuickBar(Interactor);
	if (!Settings || !QuickBar)
	{
		Prompt.ActionText = NSLOCTEXT("BotanicusIllegalTrade", "Unavailable", "Indisponible");
		return Prompt;
	}

	const FBotanicusIllegalPlanterLevelDefinition& Level = Settings->GetPlanterLevel(PlanterLevel);
	const FName SelectedItem = QuickBar->GetSelectedSlot().ItemKey;
	if (SoilUnits < Level.RequiredSoilUnits)
	{
		Prompt.ActionText = SelectedItem == TEXT("PottingSoil")
			? FText::FromString(FString::Printf(
				TEXT("Maintenir clic gauche : terreau %d/%d"),
				SoilUnits,
				Level.RequiredSoilUnits))
			: NSLOCTEXT(
				"BotanicusIllegalTrade",
				"SelectSoil",
				"Selectionner du terreau");
		Prompt.bCanInteract = false;
		return Prompt;
	}

	const int32 SlotIndex = ResolveAimedSlot(Interactor);
	const FBotanicusIllegalPlantSlotState* Slot = PlantSlots.IsValidIndex(SlotIndex)
		? &PlantSlots[SlotIndex]
		: nullptr;
	if (Slot && !Slot->PlantId.IsNone() && Slot->GrowthProgress >= 1.0f)
	{
		Prompt.ActionText = NSLOCTEXT("BotanicusIllegalTrade", "Harvest", "Récolter");
		Prompt.bCanInteract = true;
	}
	else if (Slot && Slot->PlantId.IsNone() && Settings->FindPlantBySeed(SelectedItem))
	{
		Prompt.ActionText = FText::FromString(FString::Printf(
			TEXT("Clic gauche : planter dans l'emplacement %d"),
			SlotIndex + 1));
		Prompt.bCanInteract = true;
	}
	else if (QuickBar->HasSelectedWateringCan())
	{
		Prompt.ActionText = Slot && !Slot->PlantId.IsNone()
			? NSLOCTEXT("BotanicusIllegalTrade", "Water", "Arroser cette plante")
			: NSLOCTEXT("BotanicusIllegalTrade", "AimAtPlant", "Viser une plante");
		Prompt.bCanInteract = Slot && !Slot->PlantId.IsNone() &&
			Slot->GrowthProgress < 1.0f &&
			Slot->WaterAmount < Level.MaximumWater - KINDA_SMALL_NUMBER &&
			QuickBar->GetSelectedWateringCanWaterLevel() > KINDA_SMALL_NUMBER;
	}
	else
	{
		Prompt.ActionText = NSLOCTEXT("BotanicusIllegalTrade", "SelectSeed", "Sélectionner des graines ou l'arrosoir");
	}
	return Prompt;
}

bool ABotanicusIllegalPlanterActor::CanInteract_Implementation(AActor* Interactor) const
{
	return GetInteractionPrompt_Implementation(Interactor).bCanInteract;
}

void ABotanicusIllegalPlanterActor::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor))
	{
		return;
	}
	UBotanicusQuickBarComponent* QuickBar = GetQuickBar(Interactor);
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	if (!QuickBar || !Settings)
	{
		return;
	}

	const FBotanicusIllegalPlanterLevelDefinition& Level = Settings->GetPlanterLevel(PlanterLevel);
	const FName SelectedItem = QuickBar->GetSelectedSlot().ItemKey;
	if (SoilUnits < Level.RequiredSoilUnits && SelectedItem == TEXT("PottingSoil"))
	{
		if (QuickBar->ConsumeSelectedItem())
		{
			++SoilUnits;
		}
	}
	else
	{
		const int32 SlotIndex = ResolveAimedSlot(Interactor);
		FBotanicusIllegalPlantSlotState* Slot = PlantSlots.IsValidIndex(SlotIndex)
			? &PlantSlots[SlotIndex]
			: nullptr;
		if (Slot && !Slot->PlantId.IsNone() && Slot->GrowthProgress >= 1.0f)
		{
			if (const FBotanicusIllegalPlantDefinition* Plant = Settings->FindPlant(Slot->PlantId))
			{
				int32 AddedSlot = INDEX_NONE;
				if (QuickBar->AddItem(Plant->ProductItemKey, Plant->HarvestQuantity, AddedSlot))
				{
					UE_LOG(LogBotanicusIllegalTrade, Display,
						TEXT("Plant harvested: %s x%d."),
						*Plant->ProductItemKey.ToString(), Plant->HarvestQuantity);
					*Slot = FBotanicusIllegalPlantSlotState();
				}
			}
		}
		else if (Slot && Slot->PlantId.IsNone() &&
			Settings->FindPlantBySeed(SelectedItem))
		{
			if (const FBotanicusIllegalPlantDefinition* Plant = Settings->FindPlantBySeed(SelectedItem))
			{
				if (QuickBar->ConsumeSelectedItem())
				{
					Slot->PlantId = Plant->PlantId;
					Slot->GrowthProgress = 0.0f;
					UE_LOG(LogBotanicusIllegalTrade, Display,
						TEXT("Plant created: %s."), *Plant->PlantId.ToString());
				}
			}
		}
		else if (Slot && !Slot->PlantId.IsNone() &&
			QuickBar->HasSelectedWateringCan())
		{
			const float Transfer = FMath::Min3(
				Settings->WaterPerInteraction,
				Level.MaximumWater - Slot->WaterAmount,
				QuickBar->GetSelectedWateringCanWaterLevel());
			if (Transfer > KINDA_SMALL_NUMBER &&
				QuickBar->ConsumeSelectedWateringCanWater(Transfer))
			{
				Slot->WaterAmount += Transfer;
			}
		}
	}

	RefreshVisuals();
	ForceNetUpdate();
	ScheduleAutosave();
}

void ABotanicusIllegalPlanterActor::BeginPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() || ActivePrimaryUser.IsValid())
	{
		return;
	}

	ABotanicusCharacter* Character = Cast<ABotanicusCharacter>(Interactor);
	UBotanicusQuickBarComponent* QuickBar = GetQuickBar(Character);
	const UBotanicusIllegalTradeSettings* Settings =
		GetDefault<UBotanicusIllegalTradeSettings>();
	if (!Character || !QuickBar || !Settings)
	{
		return;
	}

	const FBotanicusIllegalPlanterLevelDefinition& Level =
		Settings->GetPlanterLevel(PlanterLevel);
	const FName SelectedItem = QuickBar->GetSelectedSlot().ItemKey;
	if (SoilUnits < Level.RequiredSoilUnits)
	{
		if (SelectedItem != TEXT("PottingSoil"))
		{
			if (APlayerController* PlayerController =
					Cast<APlayerController>(Character->GetController()))
			{
				PlayerController->ClientMessage(FString::Printf(
					TEXT("Remplissez d'abord le bac de terreau : %d/%d doses."),
					SoilUnits,
					Level.RequiredSoilUnits));
			}
			return;
		}
		ActivePrimaryUser = Character;
		ActivePrimaryUseMode = EPrimaryUseMode::FillSoil;
		ActiveSoilFillProgress = 0.0f;
		return;
	}

	const int32 SlotIndex = ResolveAimedSlot(Character);
	const FBotanicusIllegalPlantSlotState* Slot =
		PlantSlots.IsValidIndex(SlotIndex) ? &PlantSlots[SlotIndex] : nullptr;
	if (Slot &&
		((!Slot->PlantId.IsNone() && Slot->GrowthProgress >= 1.0f) ||
		 (Slot->PlantId.IsNone() && Settings->FindPlantBySeed(SelectedItem))))
	{
		// Planting and harvesting are immediate primary actions, as on the
		// existing growing pots. Interact performs the authoritative inventory
		// transaction, replication refresh and autosave.
		Interact_Implementation(Character);
		return;
	}
	if (Slot && !Slot->PlantId.IsNone() &&
		Settings->FindPlantBySeed(SelectedItem))
	{
		if (APlayerController* PlayerController =
				Cast<APlayerController>(Character->GetController()))
		{
			PlayerController->ClientMessage(
				TEXT("Cet emplacement contient deja une plante."));
		}
		return;
	}

	if (QuickBar->HasSelectedWateringCan() &&
		Slot && !Slot->PlantId.IsNone() &&
		Slot->GrowthProgress < 1.0f &&
		QuickBar->GetSelectedWateringCanWaterLevel() > KINDA_SMALL_NUMBER &&
		Slot->WaterAmount < Level.MaximumWater - KINDA_SMALL_NUMBER)
	{
		ActivePrimaryUser = Character;
		ActivePrimaryUseMode = EPrimaryUseMode::Water;
		ActiveWaterSlotIndex = SlotIndex;
		ActiveSoilFillProgress = 0.0f;
	}
}

void ABotanicusIllegalPlanterActor::EndPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() ||
		(ActivePrimaryUser.IsValid() && ActivePrimaryUser.Get() != Interactor))
	{
		return;
	}
	ActivePrimaryUser.Reset();
	ActivePrimaryUseMode = EPrimaryUseMode::None;
	ActiveWaterSlotIndex = INDEX_NONE;
	ActiveSoilFillProgress = 0.0f;
}

bool ABotanicusIllegalPlanterActor::UpdatePrimaryUse(float DeltaSeconds)
{
	if (!ActivePrimaryUser.IsValid())
	{
		return false;
	}

	ABotanicusCharacter* Character = ActivePrimaryUser.Get();
	UBotanicusQuickBarComponent* QuickBar = GetQuickBar(Character);
	const UBotanicusIllegalTradeSettings* Settings =
		GetDefault<UBotanicusIllegalTradeSettings>();
	if (!Character || !QuickBar || !Settings ||
		!IsInteractorStillTargeting(Character))
	{
		EndPrimaryUse(Character);
		return false;
	}

	if (ActivePrimaryUseMode == EPrimaryUseMode::Water)
	{
		FBotanicusIllegalPlantSlotState* Slot =
			PlantSlots.IsValidIndex(ActiveWaterSlotIndex)
				? &PlantSlots[ActiveWaterSlotIndex] : nullptr;
		const float MaximumWater =
			Settings->GetPlanterLevel(PlanterLevel).MaximumWater;
		if (!Slot || Slot->PlantId.IsNone() || Slot->GrowthProgress >= 1.0f ||
			ResolveAimedSlot(Character) != ActiveWaterSlotIndex ||
			!QuickBar->HasSelectedWateringCan() ||
			QuickBar->GetSelectedWateringCanWaterLevel() <= KINDA_SMALL_NUMBER ||
			Slot->WaterAmount >= MaximumWater - KINDA_SMALL_NUMBER)
		{
			EndPrimaryUse(Character);
			return false;
		}

		const float Transfer = FMath::Min3(
			Settings->WaterPerInteraction * DeltaSeconds,
			MaximumWater - Slot->WaterAmount,
			QuickBar->GetSelectedWateringCanWaterLevel());
		if (Transfer <= KINDA_SMALL_NUMBER ||
			!QuickBar->ConsumeSelectedWateringCanWater(Transfer))
		{
			EndPrimaryUse(Character);
			return false;
		}

		Slot->WaterAmount = FMath::Min(Slot->WaterAmount + Transfer, MaximumWater);
		if (Slot->WaterAmount >= MaximumWater - KINDA_SMALL_NUMBER)
		{
			EndPrimaryUse(Character);
		}
		return true;
	}

	if (ActivePrimaryUseMode != EPrimaryUseMode::FillSoil ||
		QuickBar->GetSelectedSlot().ItemKey != TEXT("PottingSoil"))
	{
		EndPrimaryUse(Character);
		return false;
	}

	const int32 RequiredSoilUnits =
		Settings->GetPlanterLevel(PlanterLevel).RequiredSoilUnits;
	if (SoilUnits >= RequiredSoilUnits)
	{
		EndPrimaryUse(Character);
		return false;
	}

	ActiveSoilFillProgress += DeltaSeconds;
	if (ActiveSoilFillProgress < 1.0f)
	{
		return false;
	}

	ActiveSoilFillProgress = FMath::Fmod(ActiveSoilFillProgress, 1.0f);
	if (!QuickBar->ConsumeSelectedItem(1))
	{
		EndPrimaryUse(Character);
		return false;
	}

	SoilUnits = FMath::Min(SoilUnits + 1, RequiredSoilUnits);
	if (SoilUnits >= RequiredSoilUnits)
	{
		EndPrimaryUse(Character);
	}
	return true;
}

void ABotanicusIllegalPlanterActor::RestoreIllegalPlanterState(
	int32 InLevel,
	int32 InSoilUnits,
	float InWaterAmount,
	const TArray<FBotanicusIllegalPlantSlotState>& InSlots)
{
	if (!HasAuthority())
	{
		return;
	}
	PlanterLevel = FMath::Max(1, InLevel);
	EnsureSlotCount();
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	const FBotanicusIllegalPlanterLevelDefinition& Level = Settings->GetPlanterLevel(PlanterLevel);
	SoilUnits = FMath::Clamp(InSoilUnits, 0, Level.RequiredSoilUnits);
	bool bHasSavedSlotWater = false;
	for (int32 Index = 0; Index < PlantSlots.Num(); ++Index)
	{
		PlantSlots[Index] = InSlots.IsValidIndex(Index)
			? InSlots[Index]
			: FBotanicusIllegalPlantSlotState();
		PlantSlots[Index].GrowthProgress = FMath::Clamp(PlantSlots[Index].GrowthProgress, 0.0f, 1.0f);
		PlantSlots[Index].WaterAmount = FMath::Clamp(
			PlantSlots[Index].WaterAmount, 0.0f, Level.MaximumWater);
		bHasSavedSlotWater |= PlantSlots[Index].WaterAmount > KINDA_SMALL_NUMBER;
	}
	// Older saves had one shared reserve. Preserve its total without making
	// every plant fully watered when the save is upgraded.
	if (!bHasSavedSlotWater && InWaterAmount > KINDA_SMALL_NUMBER)
	{
		const float WaterPerSlot = FMath::Clamp(
			InWaterAmount / FMath::Max(1, PlantSlots.Num()),
			0.0f, Level.MaximumWater);
		for (FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
		{
			Slot.WaterAmount = WaterPerSlot;
		}
	}
	RefreshVisuals();
	ForceNetUpdate();
}

void ABotanicusIllegalPlanterActor::SetAllGrowthForDevelopment(float NormalizedGrowth)
{
	if (!HasAuthority()) return;
	for (FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
	{
		if (!Slot.PlantId.IsNone()) Slot.GrowthProgress = FMath::Clamp(NormalizedGrowth, 0.0f, 1.0f);
	}
	RefreshVisuals();
	ForceNetUpdate();
	ScheduleAutosave();
}

void ABotanicusIllegalPlanterActor::FillWaterForDevelopment()
{
	if (!HasAuthority()) return;
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	const float MaximumWater = Settings->GetPlanterLevel(PlanterLevel).MaximumWater;
	for (FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
	{
		if (!Slot.PlantId.IsNone()) Slot.WaterAmount = MaximumWater;
	}
	RefreshVisuals();
	ForceNetUpdate();
	ScheduleAutosave();
}

void ABotanicusIllegalPlanterActor::OnRep_PlanterState()
{
	EnsureSlotCount();
	RefreshVisuals();
}

void ABotanicusIllegalPlanterActor::RefreshVisuals()
{
	const UBotanicusIllegalTradeSettings* Settings = GetDefault<UBotanicusIllegalTradeSettings>();
	const FBotanicusIllegalPlanterLevelDefinition& Level = Settings->GetPlanterLevel(PlanterLevel);
	const float SoilFillAlpha = Level.RequiredSoilUnits > 0
		? FMath::Clamp(static_cast<float>(SoilUnits) / Level.RequiredSoilUnits, 0.0f, 1.0f)
		: 0.0f;
	float OccupiedWater = 0.0f;
	int32 OccupiedSlots = 0;
	for (const FBotanicusIllegalPlantSlotState& Slot : PlantSlots)
	{
		if (!Slot.PlantId.IsNone())
		{
			OccupiedWater += Slot.WaterAmount;
			++OccupiedSlots;
		}
	}
	const float AverageWaterRatio = OccupiedWater /
		FMath::Max(0.1f, Level.MaximumWater * FMath::Max(1, OccupiedSlots));
	SoilShapeVisual->SetRelativeLocation(FVector(0.0f, 0.0f, SoilBottomHeight));
	if (ABotanicusPotSoilVisualActor* SoilActor =
			Cast<ABotanicusPotSoilVisualActor>(SoilShapeVisual->GetChildActor()))
	{
		SoilActor->ConfigureSoilShape(
			SoilHalfExtent,
			SoilHalfExtent,
			SoilVolumeHeight,
			SoilFillAlpha,
			SoilUnits > 0,
			true,
			AverageWaterRatio);
	}
	const float Spacing = 52.0f;
	const float Start = -0.5f * Spacing * (PlantSlots.Num() - 1);
	int32 MatureCount = 0;
	for (int32 Index = 0; Index < PlantVisuals.Num(); ++Index)
	{
		UStaticMeshComponent* Visual = PlantVisuals[Index];
		const bool bOccupied = PlantSlots.IsValidIndex(Index) && !PlantSlots[Index].PlantId.IsNone();
		Visual->SetVisibility(bOccupied, true);
		Visual->SetRelativeLocation(FVector(Start + Spacing * Index, 0.0f, 31.0f));
		if (!bOccupied) continue;
		const FBotanicusIllegalPlantSlotState& Slot = PlantSlots[Index];
		const int32 Stage = GetGrowthStage(Slot.GrowthProgress);
		MatureCount += Slot.GrowthProgress >= 1.0f - KINDA_SMALL_NUMBER ? 1 : 0;
		const FBotanicusIllegalPlantDefinition* Plant =
			Settings->FindPlant(Slot.PlantId);
		const TSoftObjectPtr<UStaticMesh>* StageReference = Plant
			? (Stage == 1 ? &Plant->SeedlingMesh
				: Stage == 2 ? &Plant->YoungMesh : &Plant->MatureMesh)
			: nullptr;
		UStaticMesh* StageMesh = StageReference && !StageReference->IsNull()
			? StageReference->LoadSynchronous() : nullptr;
		if (!StageMesh)
		{
			StageMesh = LoadObject<UStaticMesh>(
				nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
		}
		if (Visual->GetStaticMesh() != StageMesh)
		{
			Visual->SetStaticMesh(StageMesh);
		}
		if (StageMesh)
		{
			const FBox Bounds = StageMesh->GetBoundingBox();
			const float MinimumHeight = Plant
				? FMath::Max(0.1f, Plant->MinimumGrowthVisualHeight) : 8.0f;
			const float MatureHeight = Plant
				? FMath::Max(MinimumHeight, Plant->MatureGrowthVisualHeight)
				: 70.0f;
			// Normalizing each mesh to the same progress-based height avoids a
			// size jump when the skin changes at 30% and 70%.
			const float DesiredHeight = FMath::Lerp(
				MinimumHeight, MatureHeight,
				FMath::Clamp(Slot.GrowthProgress, 0.0f, 1.0f));
			const float Scale = DesiredHeight /
				FMath::Max(0.01f, Bounds.GetSize().Z);
			const FVector Centre = Bounds.GetCenter();
			Visual->SetRelativeScale3D(FVector(Scale));
			Visual->SetRelativeLocation(FVector(
				Start + Spacing * Index - Centre.X * Scale,
				-Centre.Y * Scale,
				31.0f - Bounds.Min.Z * Scale));
		}
	}
	const int32 WaterPercent = FMath::RoundToInt(100.0f * AverageWaterRatio);
	StatusText->SetText(FText::Format(
		NSLOCTEXT("BotanicusIllegalTrade", "PlanterStatus", "NOCTIFLORE  |  Eau moy. {0}%  |  Mûres {1}/{2}"),
		FText::AsNumber(WaterPercent), FText::AsNumber(MatureCount), FText::AsNumber(PlantSlots.Num())));
	RefreshGrowthWidgets();
}

void ABotanicusIllegalPlanterActor::RefreshGrowthWidgets()
{
	const UBotanicusIllegalTradeSettings* Settings =
		GetDefault<UBotanicusIllegalTradeSettings>();
	if (!Settings)
	{
		return;
	}

	const FBotanicusIllegalPlanterLevelDefinition& Level =
		Settings->GetPlanterLevel(PlanterLevel);
	const float Spacing = 52.0f;
	const float Start = -0.5f * Spacing * (PlantSlots.Num() - 1);
	const APlayerController* LocalPlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APlayerCameraManager* CameraManager = LocalPlayerController
		? LocalPlayerController->PlayerCameraManager
		: nullptr;

	for (int32 Index = 0; Index < PlantGrowthWidgets.Num(); ++Index)
	{
		UWidgetComponent* GrowthComponent = PlantGrowthWidgets[Index];
		if (!GrowthComponent)
		{
			continue;
		}

		const bool bOccupied = PlantSlots.IsValidIndex(Index) &&
			!PlantSlots[Index].PlantId.IsNone();
		GrowthComponent->SetRelativeLocation(FVector(
			Start + Spacing * Index,
			0.0f,
			PlantGrowthWidgetHeight));
		GrowthComponent->SetRelativeScale3D(
			FVector(PlantGrowthWidgetScale));
		GrowthComponent->SetVisibility(
			bOccupied &&
			!ActorHasTag(TEXT("BotanicusPlacementPreview")));
		if (!bOccupied)
		{
			continue;
		}

		if (CameraManager)
		{
			GrowthComponent->SetWorldRotation(
				(CameraManager->GetCameraLocation() -
				 GrowthComponent->GetComponentLocation()).Rotation());
		}

		if (UBotanicusPlantGrowthWidget* Widget =
				Cast<UBotanicusPlantGrowthWidget>(GrowthComponent->GetWidget()))
		{
			const FBotanicusIllegalPlantSlotState& Slot = PlantSlots[Index];
			const FBotanicusIllegalPlantDefinition* Definition =
				Settings->FindPlant(Slot.PlantId);
			const FText PlantName = Definition && !Definition->DisplayName.IsEmpty()
				? Definition->DisplayName
				: FText::FromName(Slot.PlantId);
			Widget->SetPlantState(
				PlantName,
				FMath::Clamp(
					Slot.WaterAmount / FMath::Max(0.1f, Level.MaximumWater),
					0.0f, 1.0f),
				Slot.GrowthProgress);
		}
	}
}

int32 ABotanicusIllegalPlanterActor::ResolveAimedSlot(AActor* Interactor) const
{
	if (!Interactor || PlantSlots.IsEmpty()) return 0;
	FVector ViewLocation;
	FRotator ViewRotation;
	Interactor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IllegalPlanterAim), false, Interactor);
	FVector TargetLocation = GetActorLocation();
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
		Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 500.0f,
		ECC_Visibility, Params) && Hit.GetActor() == this)
	{
		TargetLocation = Hit.ImpactPoint;
	}
	const float LocalX = GetActorTransform().InverseTransformPosition(TargetLocation).X;
	const float Spacing = 52.0f;
	const float Start = -0.5f * Spacing * (PlantSlots.Num() - 1);
	return FMath::Clamp(FMath::RoundToInt((LocalX - Start) / Spacing), 0, PlantSlots.Num() - 1);
}

bool ABotanicusIllegalPlanterActor::IsInteractorStillTargeting(
	AActor* Interactor) const
{
	if (!IsValid(Interactor) ||
		FVector::DistSquared(
			Interactor->GetActorLocation(),
			GetActorLocation()) > FMath::Square(450.0f))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Interactor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(IllegalPlanterPrimaryUse), false, Interactor);
	return GetWorld() && GetWorld()->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		ViewLocation + ViewRotation.Vector() * 500.0f,
		ECC_Visibility,
		Params) && Hit.GetActor() == this;
}

void ABotanicusIllegalPlanterActor::ScheduleAutosave()
{
	if (HasAuthority())
	{
		if (ABotanicusGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABotanicusGameMode>() : nullptr)
		{
			GameMode->ScheduleInventoryAutosave();
		}
	}
}

void ABotanicusIllegalPlanterActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusIllegalPlanterActor, PlanterLevel);
	DOREPLIFETIME(ABotanicusIllegalPlanterActor, SoilUnits);
	DOREPLIFETIME(ABotanicusIllegalPlanterActor, PlantSlots);
}
