#pragma once
#include "CoreMinimal.h"
#include "BotanicusCheckoutVisualTypes.generated.h"
class UStaticMeshComponent;
class UAudioComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class ABotanicusCheckoutScanner;
class ABotanicusCashRegisterActor;
class ABotanicusVisitorCharacter;
USTRUCT()
struct FBotanicusCheckoutBeam
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Beam;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Impact;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ContactMaterial;
    TWeakObjectPtr<ABotanicusCashRegisterActor> Register;
    TWeakObjectPtr<ABotanicusCheckoutScanner> Scanner;
    TWeakObjectPtr<ABotanicusVisitorCharacter> Customer;
    UPROPERTY() TObjectPtr<UAudioComponent> ScanVoice;
    bool bAudioFading=false;
    FGuid Id;
    double StartedAt=0;
    float Up=.6f,Down=.6f,Fade=.12f;
    TArray<FVector> Vertices,Normals;
    TArray<FVector2D> UV,Circle,RingUV;
    TArray<FVector> RingVertices,RingNormals;
    FBox TargetBounds=FBox(ForceInit);
    FVector Contact=FVector::ZeroVector;
    float Progress=0,BandWidth=0;
};
