#include "Gardening/BotanicusRobotFaceComponent.h"
#include "Gardening/BotanicusGardeningRobot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace
{
float SmoothAge(double Now,double Start,float Duration)
{
    const float A=Duration>0?FMath::Clamp(float((Now-Start)/Duration),0.f,1.f):1;
    return A*A*(3-2*A);
}
FLinearColor Color(FVector4 V){return FLinearColor(V.X,V.Y,V.Z,V.W);}
}
UBotanicusRobotFaceComponent::UBotanicusRobotFaceComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    bEditableWhenInherited=true;
    FaceMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Botanicus/Items/robot_aroseur/Face/MI_Botanicus_RobotFace.MI_Botanicus_RobotFace")));
}
void UBotanicusRobotFaceComponent::Configure(USkeletalMeshComponent* Mesh)
{
    if(!Mesh || !GetWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    const int32 Slot=Mesh->GetMaterialIndex(FaceMaterialSlot);
    if(Slot==INDEX_NONE)return; // Never overwrite an unrelated slot on a different robot mesh.
    auto* Parent=FaceMaterial.LoadSynchronous();if(!Parent)return;
    if(!FaceMID || Visual!=Mesh || FaceMID->Parent!=Parent)
    {
        FaceMID=UMaterialInstanceDynamic::Create(Parent,this);Visual=Mesh;
    }
    Mesh->SetMaterial(Slot,FaceMID);
    PushSettings();
    FaceMID->SetVectorParameterValue(TEXT("FromWeights"),Color(FromWeights));
    FaceMID->SetVectorParameterValue(TEXT("ToWeights"),Color(ToWeights));
    FaceMID->SetScalarParameterValue(TEXT("FromCharge"),FromCharge);
    FaceMID->SetScalarParameterValue(TEXT("ToCharge"),ToCharge);
    FaceMID->SetScalarParameterValue(TEXT("TransitionStart"),BlendStart);
    FaceMID->SetScalarParameterValue(TEXT("TransitionDuration"),BlendDuration);
    FaceMID->SetVectorParameterValue(TEXT("GazeFromTo"),FLinearColor(GazeFrom.X,GazeFrom.Y,GazeTo.X,GazeTo.Y));
    FaceMID->SetScalarParameterValue(TEXT("GazeStart"),GazeStart);
    FaceMID->SetScalarParameterValue(TEXT("GazeDuration"),GazeDuration);
}
void UBotanicusRobotFaceComponent::PushSettings()
{
    const float Aspect=FMath::Max(.1f,ScreenAspect);
    const float Width=FMath::Clamp(EyeScale,.05f,FMath::Min(.48f,.90f/Aspect));
    const float Height=Width*Aspect; // The source eye is square; preserve its proportions on the physical screen.
    const float Spacing=FMath::Clamp(EyeSpacing,Width*.65f,.94f-Width);
    const FVector2D Room(FMath::Max(0.f,.47f-(Spacing+Width)*.5f),FMath::Max(0.f,.47f-Height*.5f));
    const FVector2D Center(FMath::Clamp(EyePositionX,-Room.X,Room.X),FMath::Clamp(EyePositionY,-Room.Y,Room.Y));
    GazeLimit=FVector2D(FMath::Min(FMath::Max(0.f,GazeMaxOffsetX),Room.X-FMath::Abs(Center.X)),FMath::Min(FMath::Max(0.f,GazeMaxOffsetY),Room.Y-FMath::Abs(Center.Y)));
    FaceMID->SetScalarParameterValue(TEXT("FaceBrightness"),FMath::Clamp(FaceBrightness,0.f,5.f));
    FaceMID->SetVectorParameterValue(TEXT("EyeLayout"),FLinearColor(Width,Height,Spacing,0));
    FaceMID->SetVectorParameterValue(TEXT("EyeCenter"),FLinearColor(Center.X,Center.Y,0,0));
    FaceMID->SetVectorParameterValue(TEXT("ScreenUVRect"),Color(ScreenUVRect));
    FaceMID->SetScalarParameterValue(TEXT("UnfoldMirroredU"),bUnfoldMirroredScreenU?1:0);
    FaceMID->SetVectorParameterValue(TEXT("ScreenMapping"),FLinearColor(FMath::DegreesToRadians(ScreenUVRotation),bFlipScreenU?1:0,bFlipScreenV?1:0,Aspect));
}
float UBotanicusRobotFaceComponent::BlendAt(double Now) const{return SmoothAge(Now,BlendStart,BlendDuration);}
FVector2D UBotanicusRobotFaceComponent::GazeAt(double Now) const{return FMath::Lerp(GazeFrom,GazeTo,SmoothAge(Now,GazeStart,GazeDuration));}
void UBotanicusRobotFaceComponent::SetExpression(int32 Expression,double Now)
{
    if(Expression==CurrentExpression)return;
    const float Blend=BlendAt(Now);
    FromWeights=FMath::Lerp(FromWeights,ToWeights,Blend);FromCharge=FMath::Lerp(FromCharge,ToCharge,Blend);
    ToWeights=FVector4(0,0,0,0);ToCharge=Expression==4?1:0;
    if(Expression<4)ToWeights[Expression]=1;
    // Freeze at the interpolated position, rather than jumping to the last destination.
    GazeFrom=GazeTo=GazeAt(Now);GazeStart=Now;GazeDuration=0;
    FaceMID->SetVectorParameterValue(TEXT("GazeFromTo"),FLinearColor(GazeFrom.X,GazeFrom.Y,GazeTo.X,GazeTo.Y));
    FaceMID->SetScalarParameterValue(TEXT("GazeDuration"),0);
    CurrentExpression=Expression;BlendStart=Now;BlendDuration=FMath::Max(0.f,ExpressionTransitionDuration);
    FaceMID->SetVectorParameterValue(TEXT("FromWeights"),Color(FromWeights));FaceMID->SetVectorParameterValue(TEXT("ToWeights"),Color(ToWeights));
    FaceMID->SetScalarParameterValue(TEXT("FromCharge"),FromCharge);FaceMID->SetScalarParameterValue(TEXT("ToCharge"),ToCharge);
    FaceMID->SetScalarParameterValue(TEXT("TransitionStart"),BlendStart);FaceMID->SetScalarParameterValue(TEXT("TransitionDuration"),BlendDuration);
    NextGaze=Now+BlendDuration+FMath::FRandRange(FMath::Max(.2f,GazeIdleMinTime),FMath::Max(FMath::Max(.2f,GazeIdleMinTime),GazeIdleMaxTime));
}
void UBotanicusRobotFaceComponent::UpdatePresentation(bool bActualWatering)
{
    const auto* R=Cast<ABotanicusGardeningRobot>(GetOwner());if(!R || !FaceMID || !GetWorld())return;
    const double Now=GetWorld()->GetTimeSeconds();
    if(R->CurrentWater<=KINDA_SMALL_NUMBER || R->State==EBotanicusGardeningState::RefillingWater)bWaitingForWater=true;
    if(R->CurrentWater>=R->Config.WaterCapacity)bWaitingForWater=false;
    const double Completed=R->GetPresentation().WateringCompletedAt;
    const double CompletedAge=R->PresentationNow()-Completed;
    const int32 Expression=R->IsDocked() && R->Battery<1-KINDA_SMALL_NUMBER?4:bWaitingForWater?3:bActualWatering?1:
        Completed>=0 && CompletedAge>=0 && CompletedAge<WateringCompletedDisplayDuration?2:0;
    const bool bSurprised=Now<SurpriseUntil;
    if(bSurprised!=bWasSurprised){bWasSurprised=bSurprised;PushSettings();}
    SetExpression(bSurprised?0:Expression,Now);
    if(bSurprised)return;
    if(Expression==0 && Now>=NextGaze)
    {
        GazeFrom=GazeAt(Now);
        GazeTo=FMath::FRand()<.3f?FVector2D::ZeroVector:FVector2D(FMath::FRandRange(-GazeLimit.X,GazeLimit.X),FMath::FRandRange(-GazeLimit.Y,GazeLimit.Y));
        GazeStart=Now;GazeDuration=FMath::FRandRange(.25f,.7f)/FMath::Max(.1f,GazeMoveSpeed);
        NextGaze=Now+GazeDuration+FMath::FRandRange(FMath::Max(.2f,GazeIdleMinTime),FMath::Max(FMath::Max(.2f,GazeIdleMinTime),GazeIdleMaxTime));
        FaceMID->SetVectorParameterValue(TEXT("GazeFromTo"),FLinearColor(GazeFrom.X,GazeFrom.Y,GazeTo.X,GazeTo.Y));
        FaceMID->SetScalarParameterValue(TEXT("GazeStart"),GazeStart);FaceMID->SetScalarParameterValue(TEXT("GazeDuration"),GazeDuration);
    }
}

void UBotanicusRobotFaceComponent::ShowWhipSurprise(float Duration)
{
    if(!GetWorld() || !FaceMID)return;
    SurpriseUntil=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,Duration);
    SetExpression(0,GetWorld()->GetTimeSeconds());
    const float Width=FMath::Min(.47f,EyeScale*1.08f),Height=FMath::Min(.90f,Width*ScreenAspect*1.12f);
    FaceMID->SetVectorParameterValue(TEXT("EyeLayout"),FLinearColor(Width,Height,FMath::Min(EyeSpacing,.94f-Width),0));
    bWasSurprised=true;
    GazeFrom=GazeTo=FVector2D::ZeroVector;GazeStart=GetWorld()->GetTimeSeconds();GazeDuration=0;
    FaceMID->SetVectorParameterValue(TEXT("GazeFromTo"),FLinearColor::Transparent);
    FaceMID->SetScalarParameterValue(TEXT("GazeDuration"),0);
}
