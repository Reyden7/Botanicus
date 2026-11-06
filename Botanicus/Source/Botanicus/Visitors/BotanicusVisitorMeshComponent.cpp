#include "Visitors/BotanicusVisitorMeshComponent.h"
#include "Visitors/BotanicusVisitorCharacter.h"
#include "Sales/BotanicusCashRegisterActor.h"
#include "Sales/Checkout/BotanicusCheckoutScanner.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UBotanicusVisitorMeshComponent::FinalizeBoneTransform()
{
    // Before the engine flips the pose buffers: the authored animation and its
    // cached local pose remain untouched. No component/actor Tick is introduced.
    if(bNeedToFlipSpaceBaseBuffers) ApplyVisitorGaze();
    Super::FinalizeBoneTransform();
}

void UBotanicusVisitorMeshComponent::RotateSubtree(int32 BoneIndex,const FQuat& Delta)
{
    auto& Pose=GetEditableComponentSpaceTransforms();
    if(!Pose.IsValidIndex(BoneIndex) || !GetSkeletalMeshAsset()) return;
    const auto& Skeleton=GetSkeletalMeshAsset()->GetRefSkeleton();
    const FVector Pivot=Pose[BoneIndex].GetTranslation();
    for(int32 I=BoneIndex;I<Pose.Num();++I)
    {
        if(I!=BoneIndex && !Skeleton.BoneIsChildOf(I,BoneIndex)) continue;
        Pose[I].SetTranslation(Pivot+Delta.RotateVector(Pose[I].GetTranslation()-Pivot));
        Pose[I].SetRotation((Delta*Pose[I].GetRotation()).GetNormalized());
    }
}

void UBotanicusVisitorMeshComponent::ApplyVisitorGaze()
{
    auto* Visitor=Cast<ABotanicusVisitorCharacter>(GetOwner());
    UWorld* World=GetWorld();
    if(!Visitor || !World || World->GetNetMode()==NM_DedicatedServer || !bVisitorGazeEnabled || !GetSkeletalMeshAsset()) return;
    const int32 HeadIndex=GetBoneIndex(TEXT("head"));
    auto& Pose=GetEditableComponentSpaceTransforms();
    if(!Pose.IsValidIndex(HeadIndex)) return; // Skeleton/LOD without head: authored pose only.
    const auto* GS=World->GetGameState();
    const double Now=GS ? GS->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    const auto State=Visitor->VisitorState;
    const uint8 Stage=Visitor->CheckoutStage;
    const bool bStateChanged=PreviousState!=static_cast<uint8>(State);
    if(bPresentationInitialized)
    {
        if(Stage==2 && PreviousStage==0 && (State==EBotanicusVisitorState::Paying || PreviousState==static_cast<uint8>(EBotanicusVisitorState::Paying)))
        {ScanAt=Now;++ScanReactions;}
        if(State==EBotanicusVisitorState::Leaving && PreviousState==static_cast<uint8>(EBotanicusVisitorState::Paying) && Stage==2)
        {PaymentAt=Now;++PaymentReactions;}
    }
    bPresentationInitialized=true;PreviousState=static_cast<uint8>(State);PreviousStage=Stage;
    if(bStateChanged) NextTargetRefresh=0;
    if(Now>=NextTargetRefresh)
    {
        NextTargetRefresh=Now+.18;bHasLookTarget=false;
        if(State==EBotanicusVisitorState::Inspecting || State==EBotanicusVisitorState::Approaching)
        {LookTarget=Visitor->PresentationFocus;bHasLookTarget=!LookTarget.IsNearlyZero();}
        else if(State==EBotanicusVisitorState::Paying || State==EBotanicusVisitorState::SelfCheckout || Now-PaymentAt<.45)
        {
            if(!VisualRegister.IsValid())
            {
                double Best=FMath::Square(450.);
                for(TActorIterator<ABotanicusCashRegisterActor> It(World);It;++It)
                {
                    const double Distance=FVector::DistSquared2D(Visitor->GetActorLocation(),It->GetActorLocation());
                    if(Distance<Best) {Best=Distance;VisualRegister=*It;}
                }
            }
            auto* Cash=VisualRegister.Get();
            const uint32 Seed=GetTypeHash(Visitor->GetCheckoutCustomerId());
            const double Phase=(Seed%600)*.01; // Stagger attention changes between customers.
            const uint32 Choice=HashCombine(Seed,GetTypeHash(FMath::FloorToInt((Now+Phase)/6.)))%3;
            if(Now-PaymentAt<.45 && Cash && Cash->GetReceipt().CustomerId==Visitor->GetCheckoutCustomerId())
            {LookTarget=Cash->GetReceipt().PaymentAnchor.GetLocation();bHasLookTarget=true;}
            else if(Choice==0 && Visitor->bCarryingPlant)
            {LookTarget=Visitor->GetCheckoutPlantLocation();bHasLookTarget=true;}
            else if(Choice==1 && Cash)
            {
                auto* Tool=Cash->GetDockScanner();auto* Cashier=Tool ? Tool->GetCarrier() : nullptr;
                LookTarget=Cashier ? Cashier->GetPawnViewLocation() : Cash->GetActorLocation()-Cash->GetActorForwardVector()*85+FVector(0,0,145);
                bHasLookTarget=true;
            }
        }
    }
    FRotator Desired=FRotator::ZeroRotator;
    const bool bStationary=Visitor->GetVelocity().SizeSquared2D()<FMath::Square(30.f);
    if(bHasLookTarget && (bStationary || Now-PaymentAt<.45 || State==EBotanicusVisitorState::Approaching))
    {
        const FVector HeadWorld=GetComponentTransform().TransformPosition(Pose[HeadIndex].GetTranslation());
        const FRotator Aim=Visitor->GetActorTransform().InverseTransformVectorNoScale(LookTarget-HeadWorld).Rotation();
        Desired.Yaw=FMath::Clamp(FRotator::NormalizeAxis(Aim.Yaw),-MaximumGazeYaw,MaximumGazeYaw);
        Desired.Pitch=FMath::Clamp(Aim.Pitch,-MaximumGazePitch,MaximumGazePitch);
    }
    const double ScanAge=Now-ScanAt;
    if(ScanAge>=0 && ScanAge<.32) Desired.Pitch-=3.f*FMath::Sin(PI*ScanAge/.32);
    CurrentGaze=FMath::RInterpTo(CurrentGaze,Desired,FMath::Clamp(World->GetDeltaSeconds(),0.f,.05f),GazeBlendSpeed);
    const FVector Up=GetComponentTransform().InverseTransformVectorNoScale(Visitor->GetActorUpVector());
    const FVector Right=GetComponentTransform().InverseTransformVectorNoScale(Visitor->GetActorRightVector());
    auto Delta=[&](float Weight){return FQuat(Up,FMath::DegreesToRadians(CurrentGaze.Yaw*Weight))*FQuat(Right,FMath::DegreesToRadians(-CurrentGaze.Pitch*Weight));};
    const int32 NeckIndex=GetBoneIndex(TEXT("neck_01"));
    const bool bNeck=Pose.IsValidIndex(NeckIndex);
    if(bNeck) RotateSubtree(NeckIndex,Delta(.3f));
    RotateSubtree(HeadIndex,Delta(bNeck ? .7f : 1.f));
}
