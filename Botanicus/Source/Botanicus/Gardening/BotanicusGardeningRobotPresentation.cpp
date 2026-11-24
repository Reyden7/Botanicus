#include "Gardening/BotanicusGardeningRobot.h"
#include "Gardening/BotanicusRobotFaceComponent.h"
#include "Gardening/BotanicusGardeningRobotBase.h"
#include "Growing/BotanicusPlantPotActor.h"
#include "Growing/BotanicusAutomaticWatering.h"
#include "GameFramework/WorldSettings.h"
#include "Visuals/BotanicusReservoirVisualComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

void ABotanicusGardeningRobot::SetupAnimatedVisual()
{
    auto* Asset=RobotSkeletalMesh.LoadSynchronous();if(!Asset)return;
    auto* Visual=GetMesh();if(Visual->GetSkeletalMeshAsset()!=Asset)Visual->SetSkeletalMesh(Asset);
    // Preserve the component transform authored in the Blueprint, including network smoothing's base offset.
    // A simulated proxy may already include an initial smoothing displacement in BeginPlay.
    const auto* Authored=GetLocalRole()==ROLE_SimulatedProxy ? GetClass()->GetDefaultObject<ABotanicusGardeningRobot>()->GetMesh() : Visual;
    CacheInitialMeshOffset(Authored->GetRelativeLocation(),Authored->GetRelativeRotation());
    GetCharacterMovement()->SetUpdateNavAgentWithOwnersCollisions(true);
    GetCharacterMovement()->UpdateNavAgent(*GetCapsuleComponent());
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetGenerateOverlapEvents(false);Visual->SetCanEverAffectNavigation(false);
    Visual->SetVisibility(true);Visual->SetHiddenInGame(false);Visual->SetSimulatePhysics(false);
    Visual->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    if(auto* AnimClass=RobotAnimationBlueprint.LoadSynchronous())if(Visual->GetAnimClass()!=AnimClass)Visual->SetAnimInstanceClass(AnimClass);
    auto Attachment=[&](FName Bone,FVector Position,FVector Scale)
    {
        const auto& Ref=Asset->GetRefSkeleton();int32 Index=Ref.FindBoneIndex(Bone);FTransform Frame=FTransform::Identity;
        if(Index>=0)
        {
            Frame=Ref.GetRefBonePose()[Index];int32 Parent=Ref.GetParentIndex(Index);
            while(Parent>=0){Frame*=Ref.GetRefBonePose()[Parent];Parent=Ref.GetParentIndex(Parent);}
        }
        return FTransform(Frame.GetRotation().Inverse(),Frame.InverseTransformPosition(Position),Scale/Frame.GetScale3D());
    };
    if(Visual->DoesSocketExist(ReservoirSocket))
    {
        // Only the hidden calibration mount is recomputed. The authored pivot transform is never reset.
        if(WaterTankMount->GetAttachParent()!=Visual || WaterTankMount->GetAttachSocketName()!=ReservoirSocket)
            WaterTankMount->AttachToComponent(Visual,FAttachmentTransformRules::KeepRelativeTransform,ReservoirSocket);
        WaterTankMount->SetRelativeTransform(Attachment(ReservoirSocket,FVector::ZeroVector,FVector::OneVector));
        if(Body->GetAttachParent()!=WaterTankPivot)Body->AttachToComponent(WaterTankPivot,FAttachmentTransformRules::KeepRelativeTransform);
        ReservoirVisual->Configure(Body,false);Body->SetVisibility(true,false);Body->SetHiddenInGame(false,false);
    }
    else {Body->SetVisibility(false,false);Body->SetHiddenInGame(true,false);ReservoirVisual->SetReservoirVisible(false);}
    if(Visual->DoesSocketExist(OutletSocket))
    {WaterOutlet->AttachToComponent(Visual,FAttachmentTransformRules::SnapToTargetIncludingScale,OutletSocket);WaterOutlet->SetRelativeTransform(Attachment(OutletSocket,OutletMeshSpacePosition,FVector::OneVector));}
    if(RobotFace)RobotFace->Configure(Visual);
    OnRep_Water();
}
double ABotanicusGardeningRobot::PresentationNow() const
{
    if(!GetWorld())return 0;const auto* G=GetWorld()->GetGameState();return G?G->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
}
float ABotanicusGardeningRobot::AnimationDuration(const TSoftObjectPtr<UAnimSequence>& Reference) const
{
    auto* A=Reference.LoadSynchronous();return A?A->GetPlayLength()/FMath::Max(.01f,FMath::Abs(A->RateScale)):0;
}
void ABotanicusGardeningRobot::SetActivity(EBotanicusRobotActivity Activity,float Duration,float AnimationStart)
{
    if(!HasAuthority())return;
    Presentation.Activity=Activity;Presentation.StartedAt=PresentationNow();Presentation.Duration=Duration;Presentation.AnimationStart=AnimationStart;
    const double RealNow = GetWorld()->GetRealTimeSeconds();
    if (Activity == EBotanicusRobotActivity::StoppingForWater || Activity == EBotanicusRobotActivity::AimingAtPlant ||
        Activity == EBotanicusRobotActivity::PreparingWater)
    {
        const float Dilation = FMath::Max(0.01f, GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation());
        WateringPhaseDeadline = RealNow + FMath::Max(FMath::Max(1.f, WateringStartTimeout), Duration / Dilation + 0.5f);
    }
    if (Activity == EBotanicusRobotActivity::Watering) LastWaterTransfer = RealNow;
    ForceNetUpdate();OnRep_Activity();
}
void ABotanicusGardeningRobot::OnRep_Activity()
{
    const auto A=Presentation.Activity;
    const bool bControlled=A==EBotanicusRobotActivity::WhipBump || A==EBotanicusRobotActivity::TurningAtDock || A==EBotanicusRobotActivity::ReversingToDock || A==EBotanicusRobotActivity::LeavingDock || A==EBotanicusRobotActivity::AimingAtPlant;
    SetActorTickEnabled(HasAuthority() && bControlled);
    if(HasAuthority())
    {
        GetCharacterMovement()->bOrientRotationToMovement=!bControlled && A!=EBotanicusRobotActivity::Docked;
        if(!bControlled)GetCharacterMovement()->MaxWalkSpeed=Config.MovementSpeed;
        if(bControlled){DockLastProgress=GetWorld()->GetTimeSeconds();DockLastPosition=GetActorLocation();}
    }
    if(HasActorBegunPlay())UpdateCosmetics();
}
bool ABotanicusGardeningRobot::GetDockApproach(FVector& Point) const
{
    if(!Base || !Base->IsStationAvailable())return false;
    const FVector Desired=Base->ApproachLocation();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Ground;
    if(!Nav || !Nav->ProjectPointToNavigation(Desired,Ground,FVector(35,35,150)))return false;
    Point=Ground.Location;return true;
}
bool ABotanicusGardeningRobot::DockLaneClear(const FVector& To) const
{
    // Check above the low charging pad. CharacterMovement still sweeps and steps over the real pad below.
    const float Lift=FMath::Min(20.f,GetCharacterMovement()->MaxStepHeight);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RobotDockLane),false,this);FHitResult H;
    const auto* C=GetCapsuleComponent();const auto Shape=FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()-.5f);
    return !GetWorld()->SweepSingleByChannel(H,GetActorLocation()+FVector(0,0,Lift),To+FVector(0,0,Lift),FQuat::Identity,ECC_Pawn,Shape,Q);
}
void ABotanicusGardeningRobot::FailDocking()
{
    ReleaseTarget();
    GetCharacterMovement()->StopMovementImmediately();SetActivity(EBotanicusRobotActivity::None);
    State=EBotanicusGardeningState::Blocked;NextSearch=GetWorld()->GetTimeSeconds()+Config.UnreachableRetrySeconds;
}
void ABotanicusGardeningRobot::BeginWaterPreparation()
{
    Presentation.WateringCompletedAt=-1;
    if(auto* AI=Cast<AAIController>(GetController()))AI->StopMovement();GetCharacterMovement()->StopMovementImmediately();
    SetActivity(EBotanicusRobotActivity::StoppingForWater,StopRunDuration());
}
void ABotanicusGardeningRobot::FinishWatering(bool bReturn,bool bRetry)
{
    const auto A=Presentation.Activity;bReturnAfterFinish=bReturn;
    auto* Arm=ArmAnimation.LoadSynchronous();const float Length=Arm?Arm->GetPlayLength():0;
    const float Rate=Arm?FMath::Max(.01f,FMath::Abs(Arm->RateScale)):1;
    float Position=0;
    if(A==EBotanicusRobotActivity::PreparingWater)Position=FMath::Clamp(Length-float(PresentationNow()-Presentation.StartedAt)*Rate,0.f,Length);
    else if(A!=EBotanicusRobotActivity::Watering)Position=Length;
    bRetryAfterFinish = bRetry;
    if (!bRetry) ReleaseTarget();
    State=EBotanicusGardeningState::SearchingPlant;
    SetActivity(EBotanicusRobotActivity::FinishingWater,(Length-Position)/Rate,Position);
}
bool ABotanicusGardeningRobot::UpdateActivity(double Now)
{
    const auto A=Presentation.Activity;const double Age=PresentationNow()-Presentation.StartedAt;
    const bool bPrepare=A==EBotanicusRobotActivity::StoppingForWater || A==EBotanicusRobotActivity::AimingAtPlant || A==EBotanicusRobotActivity::PreparingWater;
    if(bPrepare)
    {
        FBotanicusAutomaticWateringNeed Need;
        const bool bCanContinue = CanAfford(ReturnPathLength, 0);
        if(!bEnabled || !Base || !Base->IsStationAvailable() || !Target || !BotanicusAutomaticWatering::Read(Target,TargetSlot,1.f /* species maximum */,Need) || Need.Current>=Need.Target-KINDA_SMALL_NUMBER || CurrentWater<=KINDA_SMALL_NUMBER || !bCanContinue)
        {FinishWatering(!bCanContinue || CurrentWater<=KINDA_SMALL_NUMBER);return true;}
        WaterTarget=Need.SoilPoint;
        if (!IsWithinWateringReach(WaterTarget))
        {
            RecoverWatering(TEXT("plant out of reach during preparation"));
            return true;
        }
        if (GetWorld()->GetRealTimeSeconds() >= WateringPhaseDeadline)
        {
            RecoverWatering(TEXT("watering preparation timed out"));
            return true;
        }
    }
    if(A==EBotanicusRobotActivity::StoppingForWater && Age>=Presentation.Duration)
        SetActivity(EBotanicusRobotActivity::AimingAtPlant,3);
    else if(A==EBotanicusRobotActivity::PreparingWater && Age>=Presentation.Duration)
    {
        if (!HasWaterLine(WaterTarget))
        {
            RecoverWatering(TEXT("water line blocked with arm deployed"));
            return true;
        }
        State=EBotanicusGardeningState::Watering;SetActivity(EBotanicusRobotActivity::Watering);
    }
    else if(A==EBotanicusRobotActivity::FinishingWater && Age>=Presentation.Duration)
    {
        SetActivity(EBotanicusRobotActivity::None);
        const bool bRetry = bRetryAfterFinish;
        bRetryAfterFinish = false;
        if (bRetry && bEnabled && Battery > 0) RetryWateringApproach();
        else if(bEnabled && Battery>0){if(bReturnAfterFinish)ReturnToBase();else{State=EBotanicusGardeningState::SearchingPlant;NextSearch=Now;}}
        else State=Battery<=0?EBotanicusGardeningState::Blocked:EBotanicusGardeningState::Inactive;
    }
    else if(A==EBotanicusRobotActivity::StoppingAtDock && Age>=Presentation.Duration)
    {
        if(!Base || !Base->IsStationAvailable()){FailDocking();return true;}
        SetActivity(EBotanicusRobotActivity::TurningAtDock,5);
    }
    if(A==EBotanicusRobotActivity::TurningAtDock || A==EBotanicusRobotActivity::ReversingToDock || A==EBotanicusRobotActivity::LeavingDock)
    {
        if(!Base || !Base->IsStationAvailable() || Battery<=0 || !bEnabled || Age>Presentation.Duration+2){FailDocking();return true;}
    }
    return A!=EBotanicusRobotActivity::None && A!=EBotanicusRobotActivity::Watering && A!=EBotanicusRobotActivity::ApproachingDock && A!=EBotanicusRobotActivity::Docked;
}
void ABotanicusGardeningRobot::Tick(float Dt)
{
    Super::Tick(Dt);
    if(HasAuthority() && Presentation.Activity==EBotanicusRobotActivity::WhipBump){UpdateWhipBump();return;}
    if(!HasAuthority() || !Base || !Base->IsStationAvailable() || Carrier)return;
    const auto A=Presentation.Activity;
    if(A==EBotanicusRobotActivity::AimingAtPlant)
    {
        const float Yaw=(WaterTarget-GetActorLocation()).Rotation().Yaw;
        const float Next=FMath::FixedTurn(GetActorRotation().Yaw,Yaw,DockTurnSpeed*Dt);SetActorRotation(FRotator(0,Next,0));
        if (FMath::Abs(FMath::FindDeltaAngleDegrees(Next, Yaw)) <= 5.f)
        {
            SetActorRotation(FRotator(0, Yaw, 0));
            SetActivity(EBotanicusRobotActivity::PreparingWater, AnimationDuration(ArmAnimation));
        }
        return;
    }
    if(A==EBotanicusRobotActivity::TurningAtDock)
    {
        const float Yaw=Base->DockForward().Rotation().Yaw;
        const float Next=FMath::FixedTurn(GetActorRotation().Yaw,Yaw,DockTurnSpeed*Dt);SetActorRotation(FRotator(0,Next,0));
        if(FMath::Abs(FMath::FindDeltaAngleDegrees(Next,Yaw))<1)
        {
            DockDestination=Base->DockLocation()+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            if(!DockLaneClear(DockDestination)){FailDocking();return;}
            SetActivity(EBotanicusRobotActivity::ReversingToDock,12);
        }
        return;
    }
    if(A!=EBotanicusRobotActivity::ReversingToDock && A!=EBotanicusRobotActivity::LeavingDock)return;
    FVector Delta=DockDestination-GetActorLocation();Delta.Z=0;const float Distance=Delta.Size();
    if(Distance<=1.5f)
    {
        GetCharacterMovement()->StopMovementImmediately();
        if(A==EBotanicusRobotActivity::ReversingToDock){SetActivity(EBotanicusRobotActivity::Docked);State=GetDockedState();}
        else {const FVector Next=PendingDestination;const auto NextState=PendingState;SetActivity(EBotanicusRobotActivity::None);if(!Navigate(Next,NextState))FailDocking();}
        return;
    }
    GetCharacterMovement()->MaxWalkSpeed=FMath::Clamp(Distance*2.f,8.f,FMath::Max(8.f,DockSpeed));
    AddMovementInput(Delta/Distance,1,true);
    if(FVector::DistSquared2D(GetActorLocation(),DockLastPosition)>4){DockLastPosition=GetActorLocation();DockLastProgress=GetWorld()->GetTimeSeconds();}
    if(GetWorld()->GetTimeSeconds()-DockLastProgress>3)FailDocking();
}
