#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BotanicusRobotFaceComponent.generated.h"

class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Local presentation only. The robot's existing 10 Hz cosmetic update chooses destinations;
 * the material interpolates gaze and interrupted crossfades continuously on the GPU. */
UCLASS(ClassGroup=(Botanicus),meta=(BlueprintSpawnableComponent))
class BOTANICUS_API UBotanicusRobotFaceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBotanicusRobotFaceComponent();
    void Configure(USkeletalMeshComponent* Mesh);
    void ShowWhipSurprise(float Duration=.5f);
    void UpdatePresentation(bool bActualWatering);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face") TSoftObjectPtr<UMaterialInterface> FaceMaterial;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face") FName FaceMaterialSlot=TEXT("corps_visage_noir");
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face",meta=(ClampMin="0",ClampMax="5")) float FaceBrightness=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Eyes",meta=(ClampMin="0.05",ClampMax="0.48")) float EyeScale=.42f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Eyes",meta=(ClampMin="0",ClampMax="0.8")) float EyeSpacing=.42f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Eyes",meta=(ClampMin="-0.4",ClampMax="0.4")) float EyePositionX=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Eyes",meta=(ClampMin="-0.4",ClampMax="0.4")) float EyePositionY=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Gaze",meta=(ClampMin="0",ClampMax="0.3")) float GazeMaxOffsetX=.035f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Gaze",meta=(ClampMin="0",ClampMax="0.3")) float GazeMaxOffsetY=.06f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Gaze",meta=(ClampMin="0.1",ClampMax="5")) float GazeMoveSpeed=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Gaze",meta=(ClampMin="0.2")) float GazeIdleMinTime=2;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Gaze",meta=(ClampMin="0.2")) float GazeIdleMaxTime=5;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Expressions",meta=(ClampMin="0",ClampMax="1")) float ExpressionTransitionDuration=.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Expressions",meta=(ClampMin="0")) float WateringCompletedDisplayDuration=2;
    /** Normalized source UV rectangle: U min, V min, width, height. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Mapping") FVector4 ScreenUVRect=FVector4(.25f,.1875f,.125f,.25f);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Mapping",meta=(ClampMin="0.1")) float ScreenAspect=1.6f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Mapping",meta=(ClampMin="-180",ClampMax="180")) float ScreenUVRotation=0;
    /** The supplied rig mirrors the two screen halves in its atlas. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Mapping") bool bUnfoldMirroredScreenU=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Mapping") bool bFlipScreenU=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face|Mapping") bool bFlipScreenV=false;
    /** 0 eyes, 1 watering, 2 completed, 3 low water, 4 charging. Cosmetic, never replicated. */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="Face") int32 CurrentExpression=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="Face") TObjectPtr<UMaterialInstanceDynamic> FaceMID;
private:
    float BlendAt(double Now) const;
    FVector2D GazeAt(double Now) const;
    void SetExpression(int32 Expression,double Now);
    void PushSettings();
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Visual;
    FVector4 FromWeights=FVector4(1,0,0,0),ToWeights=FVector4(1,0,0,0);
    float FromCharge=0,ToCharge=0,BlendDuration=0,GazeDuration=0;
    double BlendStart=0,GazeStart=0,NextGaze=0;
    FVector2D GazeFrom=FVector2D::ZeroVector,GazeTo=FVector2D::ZeroVector;
    FVector2D GazeLimit=FVector2D::ZeroVector;
    bool bWaitingForWater=false,bWasSurprised=false;
    double SurpriseUntil=-1;
};
