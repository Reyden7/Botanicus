// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/BotanicusCommunicationDoorActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ConfigureFramePiece(
		UStaticMeshComponent* Component,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		const FVector& Location,
		const FVector& Scale)
	{
		Component->SetupAttachment(Parent);
		Component->SetStaticMesh(Mesh);
		Component->SetRelativeLocation(Location);
		Component->SetRelativeScale3D(Scale);
		Component->SetCollisionProfileName(
			UCollisionProfile::BlockAll_ProfileName);
	}
}

ABotanicusCommunicationDoorActor::
	ABotanicusCommunicationDoorActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	LeftPost =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Left Post"));
	RightPost =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Right Post"));
	Header =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Header"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	constexpr float DoorWidth = 180.0f;
	constexpr float DoorHeight = 240.0f;
	constexpr float FrameWidth = 22.0f;
	constexpr float FrameDepth = 45.0f;
	ConfigureFramePiece(
		LeftPost,
		Root,
		Cube,
		FVector(
			0.0f,
			-(DoorWidth + FrameWidth) * 0.5f,
			DoorHeight * 0.5f),
		FVector(
			FrameDepth / 100.0f,
			FrameWidth / 100.0f,
			DoorHeight / 100.0f));
	ConfigureFramePiece(
		RightPost,
		Root,
		Cube,
		FVector(
			0.0f,
			(DoorWidth + FrameWidth) * 0.5f,
			DoorHeight * 0.5f),
		FVector(
			FrameDepth / 100.0f,
			FrameWidth / 100.0f,
			DoorHeight / 100.0f));
	ConfigureFramePiece(
		Header,
		Root,
		Cube,
		FVector(0.0f, 0.0f, DoorHeight + FrameWidth * 0.5f),
		FVector(
			FrameDepth / 100.0f,
			(DoorWidth + FrameWidth * 2.0f) / 100.0f,
			FrameWidth / 100.0f));
}

void ABotanicusCommunicationDoorActor::SetPreviewMode(bool bPreview)
{
	SetReplicates(!bPreview);
	SetActorEnableCollision(!bPreview);
	for (UStaticMeshComponent* Piece : {LeftPost, RightPost, Header})
	{
		if (Piece)
		{
			Piece->SetRenderCustomDepth(bPreview);
			Piece->SetCustomDepthStencilValue(1);
			Piece->SetCollisionEnabled(
				bPreview
					? ECollisionEnabled::NoCollision
					: ECollisionEnabled::QueryAndPhysics);
		}
	}
}
