#include "Gardening/BotanicusGardeningAnimInstance.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Animation/AnimSequence.h"
#include "Gardening/BotanicusRobotAudioComponent.h"
void UBotanicusGardeningAnimInstance::NativeUpdateAnimation(float Dt)
{
 Super::NativeUpdateAnimation(Dt);auto* Robot=Cast<ABotanicusGardeningRobot>(TryGetPawnOwner());if(!Robot)return;
 const double Now=Robot->PresentationNow();const auto& P=Robot->GetPresentation();
 const float Age=FMath::Max(0.f,float(Now-P.StartedAt));
 const FVector Velocity=Robot->GetVelocity();const float Speed=Velocity.Size2D();SignedSpeed=FVector::DotProduct(Velocity,Robot->GetActorForwardVector());
 auto* Start=Robot->StartRunAnimation.Get();auto* Stop=Robot->StopRunAnimation.Get();auto* Arm=Robot->ArmAnimation.Get();auto* Water=Robot->WaterAnimation.Get();auto* Run=Robot->RunAnimation.Get();
 auto At=[](UAnimSequence* A,float Seconds){return A?FMath::Clamp(Seconds*FMath::Abs(A->RateScale),0.f,A->GetPlayLength()):0.f;};
 auto Done=[](UAnimSequence* A,float Seconds){return !A || Seconds*FMath::Abs(A->RateScale)>=A->GetPlayLength();};
 if(!bInitialized){bMoving=Speed>StartSpeed;bInitialized=true;}
 else if(!bMoving && Speed>StartSpeed){bMoving=true;LocalTransition=1;LocalTransitionAt=Now;QuietSince=-1;}
 else if(bMoving)
 {
  if(Speed<StopSpeed){if(QuietSince<0)QuietSince=Now;if(Now-QuietSince>=StopDebounce){bMoving=false;LocalTransition=2;LocalTransitionAt=Now;}}
  else QuietSince=-1;
 }
 BodyPoseIndex=0;BodyTime=0;ArmPoseIndex=0;ArmTime=Arm?Arm->GetPlayLength():0;
 const float TransitionAge=FMath::Max(0.f,float(Now-LocalTransitionAt));
 if(LocalTransition==1 && !Done(Start,TransitionAge)){BodyPoseIndex=1;BodyTime=At(Start,TransitionAge);}
 else if(LocalTransition==2 && !Done(Stop,TransitionAge*FMath::Max(.01f,Robot->StopRunPlayRate))){BodyPoseIndex=2;BodyTime=At(Stop,TransitionAge*FMath::Max(.01f,Robot->StopRunPlayRate));}
 if(P.Activity==EBotanicusRobotActivity::StoppingForWater || P.Activity==EBotanicusRobotActivity::StoppingAtDock)
 {BodyPoseIndex=2;BodyTime=At(Stop,Age*FMath::Max(.01f,Robot->StopRunPlayRate));LocalTransition=0;}
 else if(P.Activity==EBotanicusRobotActivity::PreparingWater)
 {BodyPoseIndex=0;BodyTime=0;ArmPoseIndex=1;ArmTime=Arm?FMath::Max(0.f,Arm->GetPlayLength()-At(Arm,Age)):0;LocalTransition=0;}
 else if(P.Activity==EBotanicusRobotActivity::Watering)
 {
  const float Length=Water?Water->GetPlayLength():0;
  BodyPoseIndex=3;ArmPoseIndex=2;BodyTime=ArmTime=Length>0?FMath::Fmod(Age*FMath::Abs(Water->RateScale),Length):0;LocalTransition=0;
 }
 else if(P.Activity==EBotanicusRobotActivity::FinishingWater)
 {BodyPoseIndex=0;BodyTime=0;ArmPoseIndex=3;ArmTime=Arm?FMath::Min(Arm->GetPlayLength(),P.AnimationStart+At(Arm,Age)):0;LocalTransition=0;}
 if(P.Activity==EBotanicusRobotActivity::TurningAtDock || P.Activity==EBotanicusRobotActivity::ReversingToDock || P.Activity==EBotanicusRobotActivity::Docked)
 {BodyPoseIndex=0;BodyTime=0;LocalTransition=0;}
 if(Run && Run->GetPlayLength()>0 && Speed>.1f)
 {
  const float Direction=SignedSpeed<0?-1.f:1.f;
  WheelTime=FMath::Fmod(WheelTime+Direction*Speed/FMath::Max(1.f,Robot->Config.MovementSpeed)*WheelSpeedScale*FMath::Abs(Run->RateScale)*Dt,Run->GetPlayLength());
  if(WheelTime<0)WheelTime+=Run->GetPlayLength();
 }
}

void UBotanicusGardeningAnimInstance::NativePostEvaluateAnimation()
{
 Super::NativePostEvaluateAnimation();
 if(auto* R=Cast<ABotanicusGardeningRobot>(TryGetPawnOwner()))if(R->RobotAudio)R->RobotAudio->ObserveEvaluatedArmMotion();
}
