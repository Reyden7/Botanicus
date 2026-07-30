// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ping/BotanicusPingMarker.h"

#include "Botanicus.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ABotanicusPingMarker::ABotanicusPingMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);

	MarkerText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("MarkerText"));
	SetRootComponent(MarkerText);
	MarkerText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerText->SetGenerateOverlapEvents(false);
	MarkerText->SetHorizontalAlignment(EHTA_Center);
	MarkerText->SetVerticalAlignment(EVRTA_TextCenter);
	MarkerText->SetWorldSize(140.0f);
	MarkerText->SetXScale(1.25f);
	MarkerText->SetYScale(1.25f);
	MarkerText->SetTextRenderColor(FColor(255, 210, 0, 255));
	MarkerText->SetText(FText::FromString(TEXT("!")));
}

void ABotanicusPingMarker::BeginPlay()
{
	Super::BeginPlay();
	RefreshMarkerText();
}

void ABotanicusPingMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator ControllerIt =
			 World->GetPlayerControllerIterator();
		 ControllerIt;
		 ++ControllerIt)
	{
		APlayerController* Controller = ControllerIt->Get();
		if (!Controller || !Controller->IsLocalController())
		{
			continue;
		}

		FVector CameraLocation;
		FRotator CameraRotation;
		Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
		const FVector ToCamera =
			CameraLocation - GetActorLocation();
		if (!ToCamera.IsNearlyZero())
		{
			FRotator FacingRotation = ToCamera.Rotation();
			FacingRotation.Yaw += 180.0f;
			SetActorRotation(FacingRotation);
		}
		break;
	}
}

void ABotanicusPingMarker::InitializePing(
	const FString& InOwnerDisplayName,
	float LifeTime)
{
	check(HasAuthority());
	OwnerDisplayName = InOwnerDisplayName;
	RefreshMarkerText();
	SetLifeSpan(FMath::Max(0.1f, LifeTime));
}

void ABotanicusPingMarker::OnRep_OwnerDisplayName()
{
	RefreshMarkerText();
}

void ABotanicusPingMarker::RefreshMarkerText()
{
	if (MarkerText)
	{
		MarkerText->SetText(FText::FromString(TEXT("!")));
	}
}

void ABotanicusPingMarker::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ABotanicusPingMarker,
		OwnerDisplayName);
}
