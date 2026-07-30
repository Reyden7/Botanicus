// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotanicusCommunicationDoorActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Replicated open doorway created between two connected buildings. */
UCLASS()
class BOTANICUS_API ABotanicusCommunicationDoorActor : public AActor
{
	GENERATED_BODY()

public:
	ABotanicusCommunicationDoorActor();

	void SetPreviewMode(bool bPreview);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LeftPost;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RightPost;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Header;
};
