// Copyright Epic Games, Inc. All Rights Reserved.


#include "BotanicusCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "BotanicusCharacter.h"

ABotanicusCameraManager::ABotanicusCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void ABotanicusCameraManager::UpdateCamera(float DeltaTime)
{
	if (const auto* Character = Cast<ABotanicusCharacter>(PCOwner ? PCOwner->GetPawn() : nullptr);
		Character && Character->IsLocallyControlled() && Character->IsEmoteCameraInUse())
	{
		// Keep the cinematic view out of the engine's normal client camera updates.
		bShouldSendClientSideCameraUpdate = false;
	}
	Super::UpdateCamera(DeltaTime);
}
