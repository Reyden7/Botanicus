// Copyright Epic Games, Inc. All Rights Reserved.

#include "Delivery/BotanicusDeliveryParcelActor.h"

#include "BotanicusCharacter.h"
#include "BotanicusGameMode.h"
#include "BotanicusPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Catalog/BotanicusItemCatalogSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Delivery/BotanicusLargeEquipmentActor.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "QuickBar/BotanicusQuickBarComponent.h"
#include "UObject/ConstructorHelpers.h"

ABotanicusDeliveryParcelActor::ABotanicusDeliveryParcelActor()
{
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;

	InteractionAction =
		NSLOCTEXT("BotanicusDelivery", "UnpackParcel", "Deballer");
	InteractionName =
		NSLOCTEXT("BotanicusDelivery", "DeliveryCarton", "Carton livre");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}

	for (int32 SegmentIndex = 0;
		 SegmentIndex < TapeSegmentCount;
		 ++SegmentIndex)
	{
		UStaticMeshComponent* Segment =
			CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(
					TEXT("Red Tape Segment %02d"),
					SegmentIndex));
		Segment->SetupAttachment(SceneRoot);
		Segment->SetStaticMesh(
			CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TapeSegments.Add(Segment);
		if (UMaterialInstanceDynamic* TapeMaterial =
				Segment->CreateAndSetMaterialInstanceDynamic(0))
		{
			TapeMaterial->SetVectorParameterValue(
				TEXT("Color"),
				FLinearColor(1.0f, 0.015f, 0.015f, 1.0f));
		}
	}

	LeftFlap = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Opened Left Flap"));
	LeftFlap->SetupAttachment(SceneRoot);
	LeftFlap->SetStaticMesh(
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
	LeftFlap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightFlap = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Opened Right Flap"));
	RightFlap->SetupAttachment(SceneRoot);
	RightFlap->SetStaticMesh(
		CubeFinder.Succeeded() ? CubeFinder.Object : nullptr);
	RightFlap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionIndicator =
		CreateDefaultSubobject<UTextRenderComponent>(
			TEXT("Interaction Indicator"));
	InteractionIndicator->SetupAttachment(SceneRoot);
	InteractionIndicator->SetTextRenderColor(FColor(255, 210, 30));
	InteractionIndicator->SetHorizontalAlignment(EHTA_Center);
	InteractionIndicator->SetVerticalAlignment(EVRTA_TextCenter);
	InteractionIndicator->SetWorldSize(25.0f);
	InteractionIndicator->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	RefreshParcelAppearance();
}

void ABotanicusDeliveryParcelActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, ItemKey);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, Quantity);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, CutCoverageMask);
	DOREPLIFETIME(ABotanicusDeliveryParcelActor, bOpened);
}

void ABotanicusDeliveryParcelActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && ActiveCutter.IsValid() && !bOpened)
	{
		UpdateCutterTrace();
	}

	if (ActorHasTag(TEXT("BotanicusPlacementPreview")))
	{
		if (InteractionIndicator)
		{
			InteractionIndicator->SetVisibility(false);
		}
		return;
	}

	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	APlayerCameraManager* CameraManager =
		UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!InteractionIndicator || !LocalPawn || !CameraManager)
	{
		return;
	}

	const FVector CameraToParcel =
		GetActorLocation() - CameraManager->GetCameraLocation();
	const float CameraDistance = CameraToParcel.Size();
	const bool bPlayerCanSeePrompt =
		FVector::DistSquared(
			LocalPawn->GetActorLocation(),
			GetActorLocation()) <= FMath::Square(450.0f) &&
		CameraDistance > KINDA_SMALL_NUMBER &&
		FVector::DotProduct(
			CameraManager->GetCameraRotation().Vector(),
			CameraToParcel / CameraDistance) >=
			FMath::Cos(FMath::DegreesToRadians(24.0f));
	InteractionIndicator->SetVisibility(bPlayerCanSeePrompt);
	if (bPlayerCanSeePrompt)
	{
		const int32 Progress =
			FMath::RoundToInt(
				CountCutSegments() * 100.0f / TapeSegmentCount);
		InteractionIndicator->SetText(
			FText::FromString(
				bOpened
					? TEXT("[ E ] DEBALLER")
					: FString::Printf(
						TEXT(
							"[ E MAINTENU ] DEPLACER\n"
							"CUTTER + CLIC GAUCHE : %d%%"),
						Progress)));
		InteractionIndicator->SetWorldRotation(
			(CameraManager->GetCameraLocation() -
			 InteractionIndicator->GetComponentLocation()).Rotation());
	}
}

FBotanicusInteractionPrompt
ABotanicusDeliveryParcelActor::GetInteractionPrompt_Implementation(
	AActor* Interactor) const
{
	FBotanicusInteractionPrompt Prompt;
	Prompt.TargetName = InteractionName;
	Prompt.ActionText = bOpened
		? NSLOCTEXT("BotanicusDelivery", "TakeParcelContents", "Deballer")
		: NSLOCTEXT(
			"BotanicusDelivery",
			"CutParcelTape",
			"Couper le scotch rouge avec le cutter");
	Prompt.bCanInteract = bOpened;
	return Prompt;
}

void ABotanicusDeliveryParcelActor::InitializeParcel(
	FName InItemKey,
	int32 InQuantity)
{
	RestoreParcelState(InItemKey, InQuantity, 0, false);
}

void ABotanicusDeliveryParcelActor::RestoreParcelState(
	FName InItemKey,
	int32 InQuantity,
	uint16 InCutCoverageMask,
	bool bInOpened)
{
	if (!HasAuthority())
	{
		return;
	}
	ItemKey = InItemKey;
	Quantity = FMath::Max(1, InQuantity);
	CutCoverageMask = InCutCoverageMask;
	bOpened = bInOpened;
	RefreshParcelAppearance();
	ForceNetUpdate();
}

void ABotanicusDeliveryParcelActor::BeginCutting(AActor* Interactor)
{
	if (!HasAuthority() || bOpened || ActiveCutter.IsValid())
	{
		return;
	}
	ABotanicusCharacter* Character = Cast<ABotanicusCharacter>(Interactor);
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	if (!Character ||
		!QuickBar ||
		QuickBar->GetSelectedSlot().ItemKey != TEXT("BoxCutter"))
	{
		SendInteractorMessage(
			Interactor,
			TEXT("Selectionnez le cutter pour couper le scotch rouge."));
		return;
	}
	ActiveCutter = Character;
	bHasLastCutSample = false;
	SendInteractorMessage(
		Interactor,
		TEXT(
			"Maintenez le clic gauche et tracez toute la longueur du scotch rouge."));
}

void ABotanicusDeliveryParcelActor::EndCutting(AActor* Interactor)
{
	if (!HasAuthority() ||
		(ActiveCutter.IsValid() && ActiveCutter.Get() != Interactor))
	{
		return;
	}
	ActiveCutter.Reset();
	bHasLastCutSample = false;
}

void ABotanicusDeliveryParcelActor::UpdateCutterTrace()
{
	ABotanicusCharacter* Character = ActiveCutter.Get();
	const UBotanicusQuickBarComponent* QuickBar =
		Character ? Character->GetQuickBarComponent() : nullptr;
	APlayerController* Controller =
		Character
			? Cast<APlayerController>(Character->GetController())
			: nullptr;
	if (!Character ||
		!QuickBar ||
		!Controller ||
		QuickBar->GetSelectedSlot().ItemKey != TEXT("BoxCutter") ||
		FVector::DistSquared(
			Character->GetActorLocation(),
			GetActorLocation()) > FMath::Square(450.0f))
	{
		EndCutting(Character);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(BotanicusParcelCut),
		false,
		Character);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			ViewLocation + ViewRotation.Vector() * 700.0f,
			ECC_Visibility,
			QueryParams) ||
		Hit.GetActor() != this)
	{
		bHasLastCutSample = false;
		return;
	}

	const FVector LocalHit =
		GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
	const float TapeHalfLength = ParcelHalfExtent.X * 0.92f;
	const float TapeHalfWidth =
		FMath::Max(7.0f, ParcelHalfExtent.Y * 0.18f);
	if (FMath::Abs(LocalHit.X) > TapeHalfLength + 4.0f ||
		FMath::Abs(LocalHit.Y) > TapeHalfWidth ||
		FMath::Abs(LocalHit.Z - ParcelHalfExtent.Z) > 18.0f)
	{
		bHasLastCutSample = false;
		return;
	}

	const float Normalized =
		FMath::Clamp(
			(LocalHit.X + TapeHalfLength) /
				(TapeHalfLength * 2.0f),
			0.0f,
			0.9999f);
	const int32 SegmentIndex =
		FMath::Clamp(
			FMath::FloorToInt(Normalized * TapeSegmentCount),
			0,
			TapeSegmentCount - 1);
	if (bHasLastCutSample &&
		FMath::Abs(LocalHit.X - LastCutLocalX) <=
			TapeHalfLength * 0.45f)
	{
		const float LastNormalized =
			FMath::Clamp(
				(LastCutLocalX + TapeHalfLength) /
					(TapeHalfLength * 2.0f),
				0.0f,
				0.9999f);
		const int32 LastSegment =
			FMath::Clamp(
				FMath::FloorToInt(
					LastNormalized * TapeSegmentCount),
				0,
				TapeSegmentCount - 1);
		for (int32 Index = FMath::Min(LastSegment, SegmentIndex);
			 Index <= FMath::Max(LastSegment, SegmentIndex);
			 ++Index)
		{
			MarkTapeSegment(Index);
		}
	}
	else
	{
		MarkTapeSegment(SegmentIndex);
	}
	LastCutLocalX = LocalHit.X;
	bHasLastCutSample = true;
}

void ABotanicusDeliveryParcelActor::MarkTapeSegment(
	int32 SegmentIndex)
{
	if (!HasAuthority() ||
		SegmentIndex < 0 ||
		SegmentIndex >= TapeSegmentCount ||
		bOpened)
	{
		return;
	}
	const uint16 SegmentBit =
		static_cast<uint16>(1u << SegmentIndex);
	if ((CutCoverageMask & SegmentBit) != 0)
	{
		return;
	}
	CutCoverageMask |= SegmentBit;
	const bool bReachedStart = (CutCoverageMask & 1u) != 0;
	const bool bReachedEnd =
		(CutCoverageMask & (1u << (TapeSegmentCount - 1))) != 0;
	if (CountCutSegments() >= TapeSegmentCount - 2 &&
		bReachedStart &&
		bReachedEnd)
	{
		ABotanicusCharacter* CutterCharacter = ActiveCutter.Get();
		bOpened = true;
		ActiveCutter.Reset();
		bHasLastCutSample = false;
		SendInteractorMessage(
			CutterCharacter,
			TEXT("Scotch coupe : appuyez sur E pour deballer le carton."));
	}
	RefreshParcelAppearance();
	ForceNetUpdate();
	if (ABotanicusGameMode* GameMode =
			GetWorld()->GetAuthGameMode<ABotanicusGameMode>())
	{
		GameMode->ScheduleInventoryAutosave();
	}
}

int32 ABotanicusDeliveryParcelActor::CountCutSegments() const
{
	int32 Count = 0;
	for (int32 SegmentIndex = 0;
		 SegmentIndex < TapeSegmentCount;
		 ++SegmentIndex)
	{
		Count +=
			(CutCoverageMask & (1u << SegmentIndex)) != 0 ? 1 : 0;
	}
	return Count;
}

void ABotanicusDeliveryParcelActor::RefreshParcelAppearance()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;

	FVector CartonScale(0.48f, 0.38f, 0.30f);
	if (Definition)
	{
		const float LargestContentScale =
			Definition->WorldScale.GetAbsMax();
		if (Definition->WeightClass ==
				EBotanicusItemWeightClass::OnePlayerCarry ||
			Definition->WeightClass ==
				EBotanicusItemWeightClass::TwoPlayerCarry)
		{
			CartonScale = FVector(1.25f, 0.90f, 0.75f);
		}
		else if (
			Definition->WeightClass ==
				EBotanicusItemWeightClass::Handheld ||
			LargestContentScale >= 0.55f)
		{
			CartonScale = FVector(0.78f, 0.58f, 0.46f);
		}
		const float QuantityFactor =
			FMath::Clamp(
				1.0f + FMath::Log2(
					static_cast<float>(FMath::Max(1, Quantity))) *
					0.035f,
				1.0f,
				1.28f);
		CartonScale *= QuantityFactor;
	}
	Mesh->SetRelativeScale3D(CartonScale);
	ParcelHalfExtent = CartonScale * 50.0f;

	const float TapeLength = ParcelHalfExtent.X * 2.0f * 0.92f;
	const float SegmentLength = TapeLength / TapeSegmentCount;
	const float TapeWidth =
		FMath::Clamp(ParcelHalfExtent.Y * 0.24f, 8.0f, 16.0f);
	for (int32 SegmentIndex = 0;
		 SegmentIndex < TapeSegments.Num();
		 ++SegmentIndex)
	{
		UStaticMeshComponent* Segment = TapeSegments[SegmentIndex];
		if (!Segment)
		{
			continue;
		}
		Segment->SetRelativeLocation(
			FVector(
				-TapeLength * 0.5f +
					(SegmentIndex + 0.5f) * SegmentLength,
				0.0f,
				ParcelHalfExtent.Z + 1.2f));
		Segment->SetRelativeScale3D(
			FVector(
				SegmentLength / 100.0f,
				TapeWidth / 100.0f,
				0.012f));
		Segment->SetVisibility(
			!bOpened &&
			(CutCoverageMask & (1u << SegmentIndex)) == 0);
	}

	const FVector FlapScale(
		CartonScale.X * 0.98f,
		CartonScale.Y * 0.47f,
		0.018f);
	if (LeftFlap)
	{
		LeftFlap->SetRelativeLocation(
			FVector(
				0.0f,
				-ParcelHalfExtent.Y * 0.52f,
				ParcelHalfExtent.Z + 5.0f));
		LeftFlap->SetRelativeScale3D(FlapScale);
		LeftFlap->SetRelativeRotation(FRotator(-28.0f, 0.0f, 0.0f));
		LeftFlap->SetVisibility(bOpened);
	}
	if (RightFlap)
	{
		RightFlap->SetRelativeLocation(
			FVector(
				0.0f,
				ParcelHalfExtent.Y * 0.52f,
				ParcelHalfExtent.Z + 5.0f));
		RightFlap->SetRelativeScale3D(FlapScale);
		RightFlap->SetRelativeRotation(FRotator(28.0f, 0.0f, 0.0f));
		RightFlap->SetVisibility(bOpened);
	}
	if (InteractionIndicator)
	{
		InteractionIndicator->SetRelativeLocation(
			FVector(0.0f, 0.0f, ParcelHalfExtent.Z + 70.0f));
	}
}

void ABotanicusDeliveryParcelActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!HasAuthority() || !bInteractionEnabled)
	{
		return;
	}
	if (!bOpened)
	{
		SendInteractorMessage(
			Interactor,
			TEXT(
				"Le carton est ferme : selectionnez le cutter et coupez tout le scotch rouge."));
		return;
	}
	if (ReleaseContents(Interactor))
	{
		SetInteractionEnabled(false);
		Destroy();
	}
}

bool ABotanicusDeliveryParcelActor::ReleaseContents(
	AActor* Interactor)
{
	ABotanicusCharacter* Character =
		Cast<ABotanicusCharacter>(Interactor);
	const UGameInstance* GameInstance = GetGameInstance();
	const UBotanicusItemCatalogSubsystem* Catalog =
		GameInstance
			? GameInstance->GetSubsystem<
				UBotanicusItemCatalogSubsystem>()
			: nullptr;
	const FBotanicusItemDefinition* Definition =
		Catalog ? Catalog->FindItem(ItemKey) : nullptr;
	if (!Character || !Definition)
	{
		return false;
	}

	if (Definition->WeightClass ==
		EBotanicusItemWeightClass::Hotbar)
	{
		UBotanicusQuickBarComponent* QuickBar =
			Character->GetQuickBarComponent();
		int32 AddedSlotIndex = INDEX_NONE;
		if (!QuickBar ||
			!QuickBar->AddItem(ItemKey, Quantity, AddedSlotIndex))
		{
			SendInteractorMessage(
				Interactor,
				TEXT(
					"Hotbar pleine : liberez de la place avant de deballer."));
			return false;
		}
		return true;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation =
		GetActorLocation() +
		GetActorRightVector() * (ParcelHalfExtent.Y + 65.0f) +
		FVector(0.0f, 0.0f, 35.0f);
	UClass* ContentClass =
		Definition->WorldActorClass.LoadSynchronous();
	if (Definition->WeightClass ==
		EBotanicusItemWeightClass::Handheld)
	{
		if (!ContentClass ||
			!ContentClass->IsChildOf(
				ABotanicusPlaceableItemActor::StaticClass()))
		{
			return false;
		}
		ABotanicusPlaceableItemActor* Item =
			GetWorld()->SpawnActor<ABotanicusPlaceableItemActor>(
				ContentClass,
				SpawnLocation,
				GetActorRotation(),
				SpawnParameters);
		if (!Item)
		{
			return false;
		}
		Item->InitializePlacedItem(ItemKey, Quantity);
		return true;
	}

	if (!ContentClass ||
		!ContentClass->IsChildOf(
			ABotanicusLargeEquipmentActor::StaticClass()))
	{
		ContentClass =
			ABotanicusLargeEquipmentActor::StaticClass();
	}
	ABotanicusLargeEquipmentActor* Equipment =
		GetWorld()->SpawnActor<ABotanicusLargeEquipmentActor>(
			ContentClass,
			SpawnLocation,
			GetActorRotation(),
			SpawnParameters);
	if (!Equipment)
	{
		return false;
	}
	Equipment->InitializeEquipment(ItemKey);
	return true;
}

void ABotanicusDeliveryParcelActor::SendInteractorMessage(
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

void ABotanicusDeliveryParcelActor::OnRep_ParcelState()
{
	RefreshParcelAppearance();
}
