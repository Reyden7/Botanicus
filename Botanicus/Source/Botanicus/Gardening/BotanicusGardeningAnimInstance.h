#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BotanicusGardeningAnimInstance.generated.h"
/** AnimGraph inputs only. No gameplay, quantities, RPC or root-motion extraction. */
UCLASS(Transient,Blueprintable)
class BOTANICUS_API UBotanicusGardeningAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 virtual void NativePostEvaluateAnimation() override;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Robot") int32 BodyPoseIndex=0;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Robot") float BodyTime=0;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Robot") int32 ArmPoseIndex=0;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Robot") float ArmTime=0;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Robot") float WheelTime=0;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Robot") float SignedSpeed=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Wheels") float WheelSpeedScale=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Locomotion") float StartSpeed=8;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Locomotion") float StopSpeed=3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot|Locomotion") float StopDebounce=.18f;
private:
 bool bInitialized=false,bMoving=false;
 double LocalTransitionAt=-100,QuietSince=-1;
 int32 LocalTransition=0;
};
