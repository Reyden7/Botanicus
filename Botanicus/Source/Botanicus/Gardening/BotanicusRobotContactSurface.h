#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BotanicusRobotContactSurface.generated.h"
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
USTRUCT()
struct FBotanicusContactInfluence
{
    GENERATED_BODY()
    UPROPERTY() int32 Bone=0;
    UPROPERTY() float Weight=1;
    UPROPERTY() FVector BonePosition=FVector::ZeroVector;
};
USTRUCT()
struct FBotanicusContactVertex
{
    GENERATED_BODY()
    UPROPERTY() TArray<FBotanicusContactInfluence> Influences;
};
/** Read-only cosmetic ray data baked from the existing meshes. No collision or rendered geometry.
 * Keeps surface picking available on a server and in cooked builds without CPU render buffers. */
UCLASS()
class BOTANICUS_API UBotanicusRobotContactSurface : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY() TSoftObjectPtr<USkeletalMesh> SourceMesh;
    UPROPERTY() TArray<FName> Bones;
    UPROPERTY() TArray<FBotanicusContactVertex> Vertices;
    UPROPERTY() TArray<int32> Indices;
    UPROPERTY() TSoftObjectPtr<UStaticMesh> SourceTank;
    UPROPERTY() TArray<FVector> TankVertices;
    UPROPERTY() TArray<int32> TankIndices;
    bool Trace(USkeletalMeshComponent* Mesh,UStaticMeshComponent* Tank,const FHitResult& Hit,FVector& Point,FVector& Normal) const;
#if WITH_EDITOR
    bool Bake(USkeletalMesh* Mesh,UStaticMesh* Tank);
#endif
};
