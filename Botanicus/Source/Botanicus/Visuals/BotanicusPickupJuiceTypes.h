#pragma once
#include "CoreMinimal.h"
#include "Visuals/BotanicusPlacementJuiceTypes.h"
#include "BotanicusPickupJuiceTypes.generated.h"
class UStaticMesh;
class UMaterialInterface;
USTRUCT()
struct FBotanicusPickupMaterial {
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UMaterialInterface> Asset;
 UPROPERTY() TArray<FName> ScalarNames;
 UPROPERTY() TArray<float> Scalars;
 UPROPERTY() TArray<FName> VectorNames;
 UPROPERTY() TArray<FLinearColor> Vectors;
};
USTRUCT()
struct FBotanicusPickupMesh {
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UStaticMesh> Asset;
 UPROPERTY() FTransform WorldTransform;
 UPROPERTY() TArray<FBotanicusPickupMaterial> Materials;
 UPROPERTY() TArray<FTransform> Instances;
 UPROPERTY() TArray<FVector> Vertices;
 UPROPERTY() TArray<FVector> Normals;
 UPROPERTY() TArray<FVector2D> UVs;
 UPROPERTY() TArray<int32> Triangles;
 UPROPERTY() bool bDisableNanite=false;
};
/** A bounded visual snapshot, independent of the collected actor's lifetime. No inventory data. */
USTRUCT()
struct FBotanicusPickupVisual {
 GENERATED_BODY()
 UPROPERTY() TArray<FBotanicusPickupMesh> Meshes;
 UPROPERTY() FName SourceName;
 UPROPERTY() double ServerTime=0;
 UPROPERTY() FTransform SourceTransform;
 UPROPERTY() bool bContentsOnly=false;
 UPROPERTY() FVector Anchor=FVector::ZeroVector;
};
USTRUCT()
struct FBotanicusPickupAnimation {
 GENERATED_BODY()
 UPROPERTY() TArray<FBotanicusPlacementCopy> Meshes;
 TWeakObjectPtr<APawn> Pawn;
 TWeakObjectPtr<AActor> CarriedSource;
 FVector Anchor=FVector::ZeroVector;
 double StartedAt=0;
 float Duration=.2f,StartScale=1,EndScale=.05f,CurrentScale=1;
};
USTRUCT()
struct FBotanicusPickupHandMask {
 GENERATED_BODY()
 TWeakObjectPtr<AActor> Actor;
 TWeakObjectPtr<APawn> Pawn;
 FName ItemKey;
 UPROPERTY() TArray<FBotanicusPlacementCopy> Meshes;
 double Until=0;
};
struct FBotanicusPickupHandGate {
 TWeakObjectPtr<APawn> Pawn;
 FName ItemKey;
 double Until=0;
};
