#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "BotanicusVisitorMeshComponent.generated.h"

class ABotanicusCashRegisterActor;

/** Small local gaze layer, applied to the existing evaluated Manny pose. */
UCLASS()
class BOTANICUS_API UBotanicusVisitorMeshComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()
public:
    virtual void FinalizeBoneTransform() override;
    UPROPERTY(EditAnywhere,Category="Visitor Presentation") bool bVisitorGazeEnabled=true;
    UPROPERTY(EditAnywhere,Category="Visitor Presentation",meta=(ClampMin="0",ClampMax="25")) float MaximumGazeYaw=18.f;
    UPROPERTY(EditAnywhere,Category="Visitor Presentation",meta=(ClampMin="0",ClampMax="15")) float MaximumGazePitch=12.f;
    UPROPERTY(EditAnywhere,Category="Visitor Presentation",meta=(ClampMin="1",ClampMax="8")) float GazeBlendSpeed=4.f;
    FRotator GetAppliedGaze() const {return CurrentGaze;}
    int32 GetScanReactionCount() const {return ScanReactions;}
    int32 GetPaymentReactionCount() const {return PaymentReactions;}
private:
    void ApplyVisitorGaze();
    void RotateSubtree(int32 BoneIndex,const FQuat& Delta);
    TWeakObjectPtr<ABotanicusCashRegisterActor> VisualRegister;
    FRotator CurrentGaze=FRotator::ZeroRotator;
    double ScanAt=-100,PaymentAt=-100,NextTargetRefresh=0;
    FVector LookTarget=FVector::ZeroVector;
    bool bHasLookTarget=false,bPresentationInitialized=false;
    uint8 PreviousState=255,PreviousStage=0;
    int32 ScanReactions=0,PaymentReactions=0;
};
