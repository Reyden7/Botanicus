#pragma once
#include "CoreMinimal.h"
#include "BotanicusPlacementJuiceTypes.generated.h"
class UMeshComponent;
class APawn;
class UMaterialInterface;
class UMaterialInstanceDynamic;
USTRUCT()
struct FBotanicusHologramMesh
{
    GENERATED_BODY()
    TWeakObjectPtr<UMeshComponent> Mesh;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Materials;
    bool bCustomDepth=false,bForceDisableNanite=false;
};
USTRUCT()
struct FBotanicusPlacementHologram
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY() TArray<FBotanicusHologramMesh> Meshes;
    FLinearColor Color=FLinearColor::White,Rim=FLinearColor::White;
    bool bValid=false;
};
USTRUCT()
struct FBotanicusPlacementCopy
{
    GENERATED_BODY()
    TWeakObjectPtr<UMeshComponent> Source;
    UPROPERTY() TObjectPtr<UMeshComponent> Copy;
    FTransform FinalWorldTransform;
    bool bMain=true,bDepth=false,bShadow=true,bCustom=false,bRay=true;
};
USTRUCT()
struct FBotanicusPlacementAnimation
{
    GENERATED_BODY()
    TWeakObjectPtr<AActor> Source;
    UPROPERTY() TArray<FBotanicusPlacementCopy> Meshes;
    FTransform FinalTransform;
    FVector Anchor=FVector::ZeroVector,Start=FVector::ZeroVector;
    FName AudioId;
    double StartedAt=0;
    float Fly=.1f,Grow=.15f,Bounce=.12f,Initial=.3f,Overshoot=1.08f,Height=2.5f,Arc=7,HandScale=.15f;
    float CurrentScale=0,CurrentHeight=0;
};
USTRUCT()
struct FBotanicusPendingPlacement
{
    GENERATED_BODY()
    TWeakObjectPtr<AActor> Source;
    TWeakObjectPtr<APawn> Pawn;
    FTransform FinalTransform;
    FVector Start=FVector::ZeroVector;
    FName ItemKey,AudioId;
    double ExpiresAt=0;
};
