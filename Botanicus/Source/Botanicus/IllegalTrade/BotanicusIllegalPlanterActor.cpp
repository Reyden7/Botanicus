// Copyright Epic Games, Inc. All Rights Reserved.

#include "IllegalTrade/BotanicusIllegalPlanterActor.h"
#include "Visuals/BotanicusSoilPourComponent.h"
#include "Growing/BotanicusPlanterSoil.h"
#include "Gardening/BotanicusGardeningSubsystem.h"
#include "Growing/BotanicusAutomaticWatering.h"
#include "Environment/BotanicusClandestineDoor.h"
#include "BotanicusGameState.h"
#include "EngineUtils.h"
#include "UI/BotanicusPlantContextSubsystem.h"
#include "Visuals/BotanicusJuiceWorldSubsystem.h"
#include "Audio/BotanicusAudioWorldSubsystem.h"

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
	constexpr int32 PlanterSlotCount = 4;

	UBotanicusQuickBarComponent* GetQuickBar(AActor* Interactor)
	{
		const ABotanicusCharacter* Character = Cast<ABotanicusCharacter>(Interactor);
		return Character ? Character->GetQuickBarComponent() : nullptr;
	}

	int32 GetGrowthStage(float Progress)
	{
		return Progress >= 0.70f ? 3 : Progress >= 0.30f ? 2 : 1;
	}

	float GetGrowthVisualHeight(
		const FBotanicusIllegalPlantDefinition* Plant, float Progress)
	{
		const float MinimumHeight = Plant
			? FMath::Max(0.1f, Plant->MinimumGrowthVisualHeight) : 8.0f;
		const float MatureHeight = Plant
			? FMath::Max(MinimumHeight, Plant->MatureGrowthVisualHeight)
			: 70.0f;
		return FMath::Lerp(
			MinimumHeight, MatureHeight,
			FMath::Clamp(Progress, 0.0f, 1.0f));
	}
}

ABotanicusIllegalPlanterActor::ABotanicusIllegalPlanterActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.25f;

	// The imported planter is the visible container. Keep the inherited Mesh
	// editable in BP_Item_IllegalPlanter for future art adjustments.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlanterFinder(
		TEXT("/Game/Botanicus/Items/itemsMesh/Trafic/jardinière/Untitled.Untitled"));
	if (PlanterFinder.Succeeded())
	{
		Mesh->SetStaticMesh(PlanterFinder.Object);
	}
	Mesh->SetRelativeLocation(FVector::ZeroVector);
	Mesh->SetRelativeScale3D(FVector::OneVector);
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
		Wall->SetVisibility(false);
		if (CubeMesh) Wall->SetStaticMesh(CubeMesh);
		return Wall;
	};
	LeftWallVisual = CreateWall(TEXT("LeftWallVisual"), FVector(0.0f, -53.0f, 44.0f), FVector(1.9f, 0.06f, 0.88f));
	RightWallVisual = CreateWall(TEXT("RightWallVisual"), FVector(0.0f, 53.0f, 44.0f), FVector(1.9f, 0.06f, 0.88f));
	FrontWallVisual = CreateWall(TEXT("FrontWallVisual"), FVector(92.0f, 0.0f, 44.0f), FVector(0.06f, 1.06f, 0.88f));
	BackWallVisual = CreateWall(TEXT("BackWallVisual"), FVector(-92.0f, 0.0f, 44.0f), FVector(0.06f, 1.06f, 0.88f));

    SlotSoilLevels.SetNumZeroed(4);
    for (int32 I=0; I<4; ++I)
    {
        auto* C=CreateDefaultSubobject<UChildActorComponent>(*FString::Printf(TEXT("SlotSoil%d"),I));
        C->SetupAttachment(SceneRoot); C->SetChildActorClass(ABotanicusPotSoilVisualActor::StaticClass());
        SlotSoilVisuals.Add(C);
    }
	SoilPour=CreateDefaultSubobject<UBotanicusSoilPourComponent>(TEXT("SoilPour"));
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
    RefreshPlanterDimensions();
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

    if(auto* Context=GetWorld()->GetSubsystem<UBotanicusPlantContextSubsystem>()) Context->RegisterPlant(this);
    if(auto* Robots=GetWorld()->GetSubsystem<UBotanicusGardeningSubsystem>())Robots->Register(this);
}

void ABotanicusIllegalPlanterActor::ApplyItemDefinition()
{
	Super::ApplyItemDefinition();
	if (!UsesBlueprintAppearance())
	{
		Mesh->SetRelativeLocation(FVector::ZeroVector);
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}
    RefreshPlanterDimensions();
    RefreshSlotSoil();
}

void ABotanicusIllegalPlanterActor::EnsureSlotCount()
{
	PlantSlots.SetNum(PlanterSlotCount);
}

FVector ABotanicusIllegalPlanterActor::GetSlotLocalLocation(int32 SlotIndex) const
{
    const BotanicusPlanterSoil::FPlantLayout Layout(GetPlanterSoilExtent(), PlanterSlotCount, PlantEdgeMargin);
    const FVector Variation = PlantSlots.IsValidIndex(SlotIndex)
        ? PlantSlots[SlotIndex].PlantPlacementVariation : FVector::ZeroVector;
    const FVector Amplitude = BotanicusPlanterSoil::PlantVariationAmplitude(
        bEnableRandomPlantPlacement, PlantRandomOffsetX, PlantRandomOffsetY, PlantRandomOffsetZ);
    return Layout.Position(SlotIndex, GetSoilVisualHeight(), Variation, Amplitude);
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
	if (SoilUnits <= 0)
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
		const float AudioPreviousGrowth = Slot.GrowthProgress;
		const int32 PreviousStage = GetGrowthStage(Slot.GrowthProgress);
		Slot.GrowthProgress = FMath::Clamp(
			Slot.GrowthProgress + DeltaSeconds * GrowthFraction /
				FMath::Max(1.0f, Plant->GrowthDurationSeconds),
			0.0f,
			1.0f);
		UBotanicusAudioWorldSubsystem::Growth(this, AudioPreviousGrowth, Slot.GrowthProgress, GetActorLocation());
		const int32 VisualIndex=static_cast<int32>(&Slot-PlantSlots.GetData());
		UBotanicusJuiceWorldSubsystem::Growth(this, AudioPreviousGrowth, Slot.GrowthProgress, PlantVisuals.IsValidIndex(VisualIndex) ? PlantVisuals[VisualIndex].Get() : nullptr);
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
	const int32 SlotIndex = ResolveAimedSlot(Interactor);
	if (GetSoilFillRatio()<1.f && (SelectedItem==TEXT("PottingSoil") || (PlantSlots.IsValidIndex(SlotIndex) && PlantSlots[SlotIndex].PlantId.IsNone())))
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
	const int32 Aimed=ResolveAimedSlot(Interactor);
    if(!PlantSlots.IsValidIndex(Aimed) || (PlantSlots[Aimed].PlantId.IsNone() && GetSoilFillRatio()<1.f))return;
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
					if (auto* Character = Cast<ABotanicusCharacter>(Interactor))
						Character->PlayRobotAction(EBotanicusRobotAction::Harvest, GetSoilSurfaceForSlot(SlotIndex), SelectedItem);
					UBotanicusAudioWorldSubsystem::Emit(this, TEXT("Harvest"), GetActorLocation());
					UBotanicusJuiceWorldSubsystem::HarvestSucceeded(this, PlantVisuals.IsValidIndex(SlotIndex) ? PlantVisuals[SlotIndex].Get() : nullptr);
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
                    Slot->PlantPlacementVariation = bEnableRandomPlantPlacement ? BotanicusPlanterSoil::MakePlantVariation() : FVector::ZeroVector;
					if (auto* Character = Cast<ABotanicusCharacter>(Interactor))
						Character->PlayRobotAction(EBotanicusRobotAction::Plant, GetSoilSurfaceForSlot(SlotIndex), SelectedItem);
					UBotanicusAudioWorldSubsystem::Emit(this, TEXT("Plant"), GetActorLocation());
					UBotanicusJuiceWorldSubsystem::PlantSucceeded(this, GetSoilSurfaceForSlot(SlotIndex));
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
				UBotanicusAudioWorldSubsystem::WaterPulse(Interactor);
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
    const FName SelectedItem=QuickBar->GetSelectedSlot().ItemKey;
    const int32 SlotIndex=ResolveAimedSlot(Character);
    if(!PlantSlots.IsValidIndex(SlotIndex))return;
    if(SelectedItem==TEXT("PottingSoil"))
    {
        if(GetSoilFillRatio()>=1.f)return;
        ActivePrimaryUser=Character;ActivePrimaryUseMode=EPrimaryUseMode::FillSoil;return;
    }
    if(PlantSlots[SlotIndex].PlantId.IsNone() && GetSoilFillRatio()<1.f)return;
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
	}
}

void ABotanicusIllegalPlanterActor::EndPrimaryUse(AActor* Interactor)
{
	if (!HasAuthority() ||
		(ActivePrimaryUser.IsValid() && ActivePrimaryUser.Get() != Interactor))
	{
		return;
	}
    SoilPour->StopTransfer();
    if (ActivePrimaryUseMode==EPrimaryUseMode::FillSoil) { RefreshVisuals(); ForceNetUpdate(); ScheduleAutosave(); }
	UBotanicusAudioWorldSubsystem::WaterEnd(ActivePrimaryUser.Get());
	ActivePrimaryUser.Reset();
	ActivePrimaryUseMode = EPrimaryUseMode::None;
	ActiveWaterSlotIndex = INDEX_NONE;
}

bool ABotanicusIllegalPlanterActor::UpdatePrimaryUse(float DeltaSeconds)
{
	if (!ActivePrimaryUser.IsValid())
	{
        if (ActivePrimaryUseMode!=EPrimaryUseMode::None) EndPrimaryUse(nullptr);
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
		UBotanicusAudioWorldSubsystem::WaterPulse(Character);
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

    if(GetSoilFillRatio()>=1.f){EndPrimaryUse(Character);return false;}
    const auto Transfer=BotanicusPlanterSoil::Transfer(SlotSoilLevels,4,SoilUnits,QuickBar,FMath::Max(0.f,DeltaSeconds)/1.5f);
    if(Transfer.bChanged)SoilPour->Transferred(Character,0);
    if(Transfer.bChanged){RefreshVisuals();ForceNetUpdate();}
    if(Transfer.bPaidBag)ScheduleAutosave();
    if(Transfer.bStopped || GetSoilFillRatio()>=1.f || QuickBar->GetSelectedSlot().ItemKey!=TEXT("PottingSoil"))EndPrimaryUse(Character);
    return false;
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
    SlotSoilLevels.SetNumZeroed(4);
    for (int32 I=0; I<4; ++I) SlotSoilLevels[I]=I<SoilUnits?1.f:0.f;
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
    RefreshSlotSoil();
	int32 MatureCount = 0;
	for (int32 Index = 0; Index < PlantVisuals.Num(); ++Index)
	{
		UStaticMeshComponent* Visual = PlantVisuals[Index];
		const bool bOccupied = PlantSlots.IsValidIndex(Index) && !PlantSlots[Index].PlantId.IsNone();
		Visual->SetVisibility(bOccupied, true);
		const FVector BaseLocation = GetSlotLocalLocation(Index);
        Visual->SetRelativeLocation(BaseLocation);
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
			// Normalizing each mesh to the same progress-based height avoids a
			// size jump when the skin changes at 30% and 70%.
			const float DesiredHeight =
				GetGrowthVisualHeight(Plant, Slot.GrowthProgress);
			const float Scale = DesiredHeight /
				FMath::Max(0.01f, Bounds.GetSize().Z);
			const FVector Centre = Bounds.GetCenter();
			Visual->SetRelativeScale3D(FVector(Scale));
			Visual->SetRelativeLocation(FVector(
				BaseLocation.X - Centre.X * Scale,
				BaseLocation.Y - Centre.Y * Scale,
				BaseLocation.Z - Bounds.Min.Z * Scale));
		}
	}
	for (int32 Index=0;Index<PlantVisuals.Num();++Index)
		if(PlantSlots.IsValidIndex(Index) && !PlantSlots[Index].PlantId.IsNone()) UBotanicusJuiceWorldSubsystem::BindFoliage(PlantVisuals[Index],true,PlantSlots[Index].PlantId==TEXT("Noctiflore"),PlantSlots[Index].GrowthProgress);
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
	const float PlanterRimHeight = Mesh
		? GetActorTransform().InverseTransformPosition(
			Mesh->Bounds.Origin +
			FVector::UpVector * Mesh->Bounds.BoxExtent.Z).Z
		: SoilBottomHeight + SoilVolumeHeight;
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
		if(!GrowthComponent->ComponentHasTag(TEXT("BotanicusPlantContextUI"))) GrowthComponent->SetRelativeScale3D(
			FVector(PlantGrowthWidgetScale));
		if (!bOccupied || ActorHasTag(TEXT("BotanicusPlacementPreview"))) GrowthComponent->SetVisibility(false);
		if (!bOccupied)
		{
			continue;
		}
		const FBotanicusIllegalPlantSlotState& Slot = PlantSlots[Index];
		const FBotanicusIllegalPlantDefinition* Definition =
			Settings->FindPlant(Slot.PlantId);
		const FVector BaseLocation = GetSlotLocalLocation(Index);
        const float PlantTopHeight =
			BaseLocation.Z +
			GetGrowthVisualHeight(Definition, Slot.GrowthProgress);
		const float WidgetBelowPivot =
			GrowthComponent->GetDrawSize().Y * PlantGrowthWidgetScale *
			GrowthComponent->GetPivot().Y;
		if(GrowthComponent->GetWidgetSpace()==EWidgetSpace::World && !GrowthComponent->ComponentHasTag(TEXT("BotanicusPlantContextUI"))) GrowthComponent->SetRelativeLocation(FVector(
			BaseLocation.X,
			BaseLocation.Y,
			FMath::Max(PlantTopHeight, PlanterRimHeight) +
			PlantGrowthWidgetClearance + WidgetBelowPivot));

		if (CameraManager && GrowthComponent->IsVisible() && GrowthComponent->GetWidgetSpace()==EWidgetSpace::World && !GrowthComponent->ComponentHasTag(TEXT("BotanicusPlantContextUI")))
		{
			GrowthComponent->SetWorldRotation(
				(CameraManager->GetCameraLocation() -
				 GrowthComponent->GetComponentLocation()).Rotation());
		}

		if (UBotanicusPlantGrowthWidget* Widget =
				Cast<UBotanicusPlantGrowthWidget>(GrowthComponent->GetWidget()))
		{
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
	if (!Interactor || PlantSlots.IsEmpty()) return INDEX_NONE;
	FVector ViewLocation;
	FRotator ViewRotation;
	Interactor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    if (const auto* Pawn=Cast<APawn>(Interactor))
        if (const auto* PC=Cast<APlayerController>(Pawn->GetController())) PC->GetPlayerViewPoint(ViewLocation,ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IllegalPlanterAim), false, Interactor);
	FVector TargetLocation = GetActorLocation();
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
		Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 500.0f,
		ECC_Visibility, Params) && Hit.GetActor() == this)
	{
		TargetLocation = Hit.ImpactPoint;
	}
    else if(!BotanicusPlanterSoil::TrySurfaceAim(this,Cast<APawn>(Interactor),GetPlanterSoilExtent(),GetSoilVisualHeight(),TargetLocation))return INDEX_NONE;
    const BotanicusPlanterSoil::FPlantLayout Layout(GetPlanterSoilExtent(), PlanterSlotCount, PlantEdgeMargin);
    return Layout.ResolveSlot(GetActorTransform().InverseTransformPosition(TargetLocation));
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
    if (const auto* Pawn=Cast<APawn>(Interactor))
        if (const auto* PC=Cast<APlayerController>(Pawn->GetController())) PC->GetPlayerViewPoint(ViewLocation,ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(IllegalPlanterPrimaryUse), false, Interactor);
	FVector SoilPoint;
    if(BotanicusPlanterSoil::TrySurfaceAim(this,Cast<APawn>(Interactor),GetPlanterSoilExtent(),GetSoilVisualHeight(),SoilPoint))return true;
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
    DOREPLIFETIME(ABotanicusIllegalPlanterActor, SlotSoilLevels);
	DOREPLIFETIME(ABotanicusIllegalPlanterActor, PlantSlots);
}


bool ABotanicusIllegalPlanterActor::IsAutomaticWateringMature(int32 Slot) const
{
    return !PlantSlots.IsValidIndex(Slot) || PlantSlots[Slot].GrowthProgress>=1.f;
}
bool ABotanicusIllegalPlanterActor::IsAutomaticWateringAllowed() const
{
    const auto* Clock=GetWorld()?GetWorld()->GetGameState<ABotanicusGameState>():nullptr;
    if(!Clock || !Clock->IsIllegalTradeActive())return false;
    auto* Door=AutomaticWateringDoor.Get();
    if(!IsValid(Door) || !Door->ContainsRearLocation(GetActorLocation()))
    {
        AutomaticWateringDoor.Reset();
        for(TActorIterator<ABotanicusClandestineDoor> I(GetWorld());I;++I)
            if(I->ContainsRearLocation(GetActorLocation())){AutomaticWateringDoor=*I;break;}
        Door=AutomaticWateringDoor.Get();
    }
    return IsValid(Door) && Door->IsRobotAccessOpen();
}
bool ABotanicusIllegalPlanterActor::GetAutomaticWateringNeed(int32 Slot,float Ratio,FBotanicusAutomaticWateringNeed& Out) const
{
    if(!GetIsReplicated() || IsActorBeingDestroyed() || IsHidden() || !PlantSlots.IsValidIndex(Slot) ||
        IsAutomaticWateringMature(Slot) || !IsAutomaticWateringAllowed() ||
        ActorHasTag(TEXT("BotanicusPlacementPreview")) || ActorHasTag(TEXT("BotanicusEquippedPreview")) || ActorHasTag(TEXT("BotanicusInspectionPreview")))return false;
    for(const AActor* Parent=GetAttachParentActor();Parent;Parent=Parent->GetAttachParentActor())if(Parent->IsA<APawn>())return false;
    const auto* Settings=GetDefault<UBotanicusIllegalTradeSettings>();const auto& Level=Settings->GetPlanterLevel(PlanterLevel);
    const auto& Plant=PlantSlots[Slot];
    if(!IsSlotSoilFull(Slot) || Plant.PlantId.IsNone() || !Settings->FindPlant(Plant.PlantId) || Level.MaximumWater<=KINDA_SMALL_NUMBER)return false;
    Out.Current=FMath::Clamp(Plant.WaterAmount/Level.MaximumWater,0.f,1.f);
    Out.Minimum=.5f; // Designer rule: start below 50% of this slot's existing reserve.
    Out.Maximum=1.f;Out.Target=FMath::Lerp(Out.Minimum,Out.Maximum,FMath::Clamp(Ratio,0.f,1.f));
    Out.UnitsPerMoisture=Level.MaximumWater; // Illegal water already uses watering-can reservoir units directly.
    Out.SoilPoint=GetSoilSurfaceForSlot(Slot);
    return true;
}
float ABotanicusIllegalPlanterActor::ApplyAutomaticWater(int32 Slot,float Ratio,float AvailableUnits,bool bFirstPulse)
{
    FBotanicusAutomaticWateringNeed Need;
    if(!HasAuthority() || AvailableUnits<=0 || !GetAutomaticWateringNeed(Slot,Ratio,Need))return 0;
    const float Used=FMath::Min(AvailableUnits,Need.UnitsNeeded());if(Used<=0)return 0;
    auto& Plant=PlantSlots[Slot];Plant.WaterAmount=FMath::Min(Need.Maximum*Need.UnitsPerMoisture,Plant.WaterAmount+Used);
    RefreshVisuals();ForceNetUpdate();ScheduleAutosave();return Used;
}

void ABotanicusIllegalPlanterActor::RestoreSoilLevels(const TArray<float>& Levels)
{
    if (!HasAuthority() || Levels.IsEmpty()) return;
    SlotSoilLevels.SetNumZeroed(4); SoilUnits=0;
    for (int32 I=0; I<4; ++I)
    {
        SlotSoilLevels[I]=Levels.IsValidIndex(I)?FMath::Clamp(Levels[I],0.f,1.f):0.f;
        if (IsSlotSoilFull(I)) ++SoilUnits;
    }
    RefreshVisuals(); ForceNetUpdate();
}
FVector ABotanicusIllegalPlanterActor::GetSoilSurfaceForSlot(int32 Slot) const
{
    FVector Local = GetSlotLocalLocation(Slot);
    Local.Z = GetSoilVisualHeight();
    return GetActorTransform().TransformPosition(Local);
}

bool ABotanicusIllegalPlanterActor::ResolveHarvestTarget(const AActor* Interactor,int32& Slot) const
{
    Slot=ResolveAimedSlot(const_cast<AActor*>(Interactor));
    if(const auto* Bar=GetQuickBar(const_cast<AActor*>(Interactor));GetSoilFillRatio()<1.f && Bar && Bar->GetSelectedSlot().ItemKey==TEXT("PottingSoil"))return false;
    return IsSlotSoilFull(Slot) && PlantSlots.IsValidIndex(Slot) && !PlantSlots[Slot].PlantId.IsNone() &&
        PlantSlots[Slot].GrowthProgress>=1.f && (!ActivePrimaryUser.IsValid() || ActivePrimaryUser.Get()==Interactor);
}
void ABotanicusIllegalPlanterActor::RefreshSlotSoil()
{
    float Ratio=GetSoilFillRatio(),Wetness=0;int32 Occupied=0;
    if(GetWorld() && !GetWorld()->IsGameWorld())Ratio=FMath::Clamp(SoilVisualPreviewFillRatio,0.f,1.f);
    const float Maximum=GetDefault<UBotanicusIllegalTradeSettings>()->GetPlanterLevel(PlanterLevel).MaximumWater;
    for(const auto& Slot:PlantSlots)if(!Slot.PlantId.IsNone()){Wetness+=Slot.WaterAmount/FMath::Max(.1f,Maximum);++Occupied;}
    const FVector2D Heights = GetPlanterSoilHeightRange();
    BotanicusPlanterSoil::Surface(SoilShapeVisual,GetPlanterSoilExtent(),Heights.X,Heights.Y,Ratio,Wetness/FMath::Max(1,Occupied));
    for(auto& Old:SlotSoilVisuals)BotanicusPlanterSoil::Hide(Old);
}

void ABotanicusIllegalPlanterActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshPlanterDimensions();
    RefreshVisuals();
}
float ABotanicusIllegalPlanterActor::GetSoilFillRatio() const
{
    return FMath::Clamp(BotanicusPlanterSoil::Total(SlotSoilLevels,4)/4.f,0.f,1.f);
}
float ABotanicusIllegalPlanterActor::GetSoilVisualHeight() const
{
    const FVector2D Heights = GetPlanterSoilHeightRange();
    return FMath::Lerp(Heights.X, Heights.Y, GetSoilFillRatio());
}
FVector ABotanicusIllegalPlanterActor::GetSoilPourTarget(const APawn* Source) const
{
    return BotanicusPlanterSoil::AimedSurface(this,Source,GetPlanterSoilExtent(),GetSoilVisualHeight());
}


FVector ABotanicusIllegalPlanterActor::GetPlanterDimensionScale() const
{
    return FVector(FMath::Max(0.25f, PlanterLengthScale),
        FMath::Max(0.25f, PlanterWidthScale), FMath::Max(0.25f, PlanterHeightScale));
}

FVector2D ABotanicusIllegalPlanterActor::GetPlanterSoilExtent() const
{
    const FVector Scale = GetPlanterDimensionScale();
    return SoilHalfExtent * FVector2D(Scale.X, Scale.Y);
}

FVector2D ABotanicusIllegalPlanterActor::GetPlanterSoilHeightRange() const
{
    const FBoxSphereBounds Bounds = Mesh->CalcBounds(Mesh->GetRelativeTransform());
    const float Bottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;
    const float Height = 2.f * Bounds.BoxExtent.Z;
    // The imported model has a raised cavity floor and an uneven rim.
    return BotanicusPlanterSoil::FitHeightRange(SoilVisualMinHeight, SoilVisualMaxHeight,
        Bottom + Height * 0.42f, Bottom + Height * 0.8f);
}

void ABotanicusIllegalPlanterActor::RefreshPlanterDimensions()
{
    if (IsTemplate())
    {
        return;
    }

    const FVector Scale = GetPlanterDimensionScale();
    const auto* TemplateMesh = Cast<UStaticMeshComponent>(Mesh->GetArchetype());
    const FTransform Reference = UsesBlueprintAppearance() && TemplateMesh
        ? TemplateMesh->GetRelativeTransform() : FTransform::Identity;
    FTransform Sized = Reference;
    Sized.SetScale3D(Reference.GetScale3D() * Scale);
    const FBoxSphereBounds Before = Mesh->CalcBounds(Reference);
    const FBoxSphereBounds After = Mesh->CalcBounds(Sized);
    const float Bottom = Before.Origin.Z - Before.BoxExtent.Z;
    Sized.AddToTranslation(FVector(0, 0, Bottom - (After.Origin.Z - After.BoxExtent.Z)));
    if (!Mesh->GetRelativeTransform().Equals(Sized))
    {
        Mesh->SetRelativeTransform(Sized);
    }

    const float WallHeight = 2.f * After.BoxExtent.Z;
    for (UStaticMeshComponent* Wall : {LeftWallVisual.Get(), RightWallVisual.Get(),
        FrontWallVisual.Get(), BackWallVisual.Get()})
    {
        const auto* TemplateWall = Cast<UStaticMeshComponent>(Wall->GetArchetype());
        if (!TemplateWall)
        {
            continue;
        }

        FTransform WallTransform = TemplateWall->GetRelativeTransform();
        FVector Location = WallTransform.GetLocation() * Scale;
        Location.Z = Bottom + WallHeight * 0.5f;
        FVector WallScale = WallTransform.GetScale3D() * Scale;
        WallScale.Z = WallHeight / 100.f;
        WallTransform.SetLocation(Location);
        WallTransform.SetScale3D(WallScale);
        if (!Wall->GetRelativeTransform().Equals(WallTransform))
        {
            Wall->SetRelativeTransform(WallTransform);
        }
    }
}
